#include "cave_lighting_smoke.h"
#include "camera.h"
#include "graphics/renderer.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Frame {
    int width, height;
    std::vector<unsigned char> pixels;
    int brightest() const
    {
        int result = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4)
            result = std::max({result, int(pixels[i]), int(pixels[i+1]), int(pixels[i+2])});
        return result;
    }
    double wallBrightness() const
    {
        double sum = 0;
        int count = 0;
        // The left side of the back wall excludes the placed lamp, any held
        // model, and their bloom. Bright panes alone cannot pass this test.
        for (int y = height * 2 / 5; y < height * 3 / 5; ++y)
            for (int x = width / 4; x < width * 2 / 5; ++x) {
                const auto i = static_cast<std::size_t>(y * width + x) * 4;
                sum += .2126 * pixels[i] + .7152 * pixels[i+1] + .0722 * pixels[i+2];
                ++count;
            }
        return sum / count;
    }
};

} // namespace

void verifyCaveLighting(int width, int height)
{
    width = std::min(width, 640); height = std::min(height, 360);
    require(width >= 320 && height >= 180, "Framebuffer is too small for dark cave checks");
    World world;
    for (int z = -6; z <= 7; ++z) for (int y = 38; y <= 45; ++y)
        for (int x = -6; x <= 6; ++x)
            if (x == -6 || x == 6 || y == 38 || y == 45 || z == -6 || z == 7)
                world.set({x,y,z}, Block::Stone);
    Camera camera;
    camera.setPosition({.5f,41.6f,4.5f});
    camera.rotate(35,25); // Face the far wall at eye level.
    WorldRenderer renderer;
    const auto draw = [&](bool dark = true, bool carried = false,
                          std::span<const HubAsset> assets = {}) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0,0,width,height);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthRange(0,1);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        renderer.render(world,camera,width,height,false,true,0,carried,false,
                        assets,{},{},{},{},{},{},{},dark);
        Frame frame{width,height,std::vector<unsigned char>(static_cast<std::size_t>(width)*height*4)};
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,frame.pixels.data());
        require(glGetError() == GL_NO_ERROR, "Dark cave rendering generated an OpenGL error");
        return frame;
    };
    const auto dark = draw();
    require(dark.brightest() <= 1, "An unlit stone cave must be pitch black, including fog and postprocessing");
    const auto rebuilds = renderer.sceneRebuildCount();
    const auto legacy = draw(false);
    require(legacy.wallBrightness() > 10, "Dark cave mode changed the usual hub/builder lighting");
    require(renderer.sceneRebuildCount() == rebuilds, "Changing lighting mode rebuilt static geometry");

    world.set({-3,40,-3},Block::Leaves);
    world.set({-2,40,-3},Block::BlossomLeaves);
    const std::array<HubAsset,1> fern{{{HubAssetKind::Fern,{-1.5f,39.f,-2.5f}}}};
    require(draw(true,false,fern).brightest() <= 1,
            "Ordinary leaves and greenery must not glow in an unlit cave");
    world.set({-3,40,-3},Block::Air);
    world.set({-2,40,-3},Block::Air);

    const std::array<HubAsset,1> lamp{{{HubAssetKind::LanternCap,{2.5f,41.4f,-2.5f}}}};
    const auto placed = draw(true,false,lamp);
    require(placed.wallBrightness() > dark.wallBrightness() + 8,
            "A placed lamp must illuminate cave stone, not only its own emissive panes");
    const auto carried = draw(true,true);
    require(carried.wallBrightness() > dark.wallBrightness() + 4,
            "The selected lantern must illuminate an otherwise dark cave");

    // The lamp remains active in the other room. Its rays point toward the
    // visible floor and side walls, but must not pass through this partition.
    for (int y = 39; y < 45; ++y) for (int x = -5; x < 6; ++x)
        world.set({x,y,0},Block::Stone);
    require(draw(true,false,lamp).brightest() <= 1,
            "A stone partition leaks light from the lamp in the next room");
    for (int y = 39; y < 45; ++y) for (int x = -5; x < 6; ++x)
        world.set({x,y,0},Block::Air);

    // Neither water surfaces, shoreline foam nor underwater haze may supply
    // their own ambient light. The lit version must still show the water.
    for (int z = -5; z < 7; ++z) for (int x = -5; x < 6; ++x)
        world.set({x,39,z},Block::Water);
    camera.rotate(0,-25);
    require(draw().brightest() <= 1, "Unlit cave water glows above the surface");
    require(draw(true,true).brightest() > 15, "A lantern cannot light cave water");
    camera.setPosition({.5f,39.5f,3.5f});
    require(draw().brightest() <= 1, "Underwater haze lifts black in an unlit cave");
    world = World{};
    require(draw().brightest() <= 1, "The clear background of an exploration cave is not black");
    std::cout << "Dark cave lighting passed: black without sources, placed/carried lights, stone shadows and water\n";
}

} // namespace voxel
