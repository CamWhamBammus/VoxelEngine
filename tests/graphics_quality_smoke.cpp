#include "graphics_quality_smoke.h"
#include "camera.h"
#include "graphics/renderer.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {
constexpr int width = 480, height = 300;
using Pixels = std::vector<unsigned char>;

void require(bool passed, const char* message)
{
    if (!passed) throw std::runtime_error(message);
}
int brightest(const Pixels& pixels)
{
    int result = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        result = std::max({result, int(pixels[i]), int(pixels[i + 1]), int(pixels[i + 2])});
    return result;
}
Pixels capture()
{
    Pixels pixels(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    require(glGetError() == GL_NO_ERROR, "Graphics quality rendering generated an OpenGL error");
    return pixels;
}
void target(Camera& camera, glm::vec3 point)
{
    const auto direction = glm::normalize(point - camera.position()), old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}
} // namespace

void verifyGraphicsQuality()
{
    World world;
    for (int z = -6; z <= 7; ++z) for (int y = 38; y <= 45; ++y)
        for (int x = -6; x <= 6; ++x)
            if (x == -6 || x == 6 || y == 38 || y == 45 || z == -6 || z == 7)
                world.set({x, y, z}, Block::Stone);
    Camera camera;
    camera.setPosition({.5f, 41.6f, 4.5f});
    target(camera, {.5f, 41.6f, -6.f});
    WorldRenderer renderer;
    const auto draw = [&](bool carried = false, std::span<const HubAsset> assets = {}) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthRange(0, 1);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        renderer.render(world, camera, width, height, false, true, 0, carried, false,
                        assets, {}, {}, {}, {}, {}, {}, {}, true);
        return capture();
    };
    bool invalidLighting = false, invalidDetail = false;
    try { renderer.setLightingQuality(static_cast<LightingQuality>(99)); }
    catch (const std::invalid_argument&) { invalidLighting = true; }
    try { renderer.setDetailDistance(static_cast<DetailDistance>(99)); }
    catch (const std::invalid_argument&) { invalidDetail = true; }
    require(invalidLighting && invalidDetail, "Renderer accepted an unknown graphics quality value");

    const std::array<HubAsset, 1> lamp{{{HubAssetKind::LanternCap, {2.5f, 41.4f, -2.5f}}}};
    for (const auto quality : {LightingQuality::Low, LightingQuality::Medium, LightingQuality::High}) {
        renderer.setLightingQuality(quality);
        require(brightest(draw()) <= 1, "A quality setting adds ambient light to an unlit cave");
        require(brightest(draw(true)) > 15, "A quality setting disables the carried lantern");
        const auto lit = draw(false, lamp);
        require(brightest(lit) > 30, "A quality setting disables placed lamps");
        const auto originalCount = renderer.drawnTriangles(), rebuilds = renderer.sceneRebuildCount();
        // Authored caves retain all relief and placed models even at the
        // shortest distance, so parkour and building previews stay faithful.
        for (const auto detail : {DetailDistance::Near, DetailDistance::Medium, DetailDistance::Far}) {
            renderer.setDetailDistance(detail);
            require(draw(false, lamp) == lit && renderer.drawnTriangles() == originalCount,
                    "Detail settings hide or change authored section geometry");
            require(renderer.sceneRebuildCount() == rebuilds, "A graphics setting rebuilt static geometry");
        }
        for (int y = 39; y < 45; ++y) for (int x = -5; x < 6; ++x)
            world.set({x, y, 0}, Block::Stone);
        require(brightest(draw(false, lamp)) <= 1, "Low quality lighting leaks through a solid cave partition");
        for (int y = 39; y < 45; ++y) for (int x = -5; x < 6; ++x)
            world.set({x, y, 0}, Block::Air);
    }

    // A real hub view verifies that the slider removes submitted decorative
    // triangles, and returning to Far produces the original frame exactly.
    buildUndergroundScene(world);
    camera.setPosition({62, 70, 81}); target(camera, {8, 70, 8});
    const auto hub = [&] {
        renderer.render(world, camera, width, height, false, true, 1, false, true);
        return capture();
    };
    renderer.setDetailDistance(DetailDistance::Far);
    const auto original = hub();
    const auto farCount = renderer.drawnTriangles(), total = renderer.triangleCount();
    const auto rebuilds = renderer.sceneRebuildCount();
    renderer.setDetailDistance(DetailDistance::Medium); hub();
    const auto mediumCount = renderer.drawnTriangles();
    renderer.setDetailDistance(DetailDistance::Near); hub();
    const auto nearCount = renderer.drawnTriangles();
    require(nearCount < mediumCount && mediumCount < farCount,
            "Village detail distance does not reduce submitted triangle counts");
    require(renderer.triangleCount() == total && renderer.sceneRebuildCount() == rebuilds,
            "Changing village detail destroys or rebuilds source geometry");
    renderer.setDetailDistance(DetailDistance::Far);
    require(hub() == original && renderer.drawnTriangles() == farCount,
            "Restoring Far detail does not restore the original village frame");
    std::cout << "Graphics quality passed: dark caves, solid shadows, held lamps, authored geometry and hub detail ("
              << nearCount << " / " << mediumCount << " / " << farCount << " triangles)\n";
}
} // namespace voxel
