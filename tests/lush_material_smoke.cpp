#include "lush_material_smoke.h"
#include "camera.h"
#include "graphics/renderer.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
}

void verifyLushMaterials(int width, int height)
{
    // Native raster tests prove these are holes with geometry behind them,
    // rather than dark painted speckles or a translucent cube overlay.
    World world;
    for (int y = World::Min.y; y < World::MaxExclusive.y; ++y)
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z)
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x)
                world.set({x, y, z}, Block::Air);
    Camera camera;
    camera.rotate(35, 25);
    camera.setPosition({0.5f, 6.5f, 4.5f});
    WorldRenderer renderer;
    const int patch = std::max(16, static_cast<int>(height * 0.12f));
    std::vector<float> depth(static_cast<std::size_t>(patch) * patch);
    std::vector<unsigned char> pixels(depth.size() * 4);
    const auto draw = [&](bool underground = false, float time = 0.0f) {
        renderer.render(world, camera, width, height, false, underground, time, false, false);
        glReadPixels(width / 2 - patch / 2, height / 2 - patch / 2,
                     patch, patch, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
        glReadPixels(width / 2 - patch / 2, height / 2 - patch / 2,
                     patch, patch, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        require(glGetError() == GL_NO_ERROR, "Lush material render caused an OpenGL error");
    };
    for (const auto leaf : {Block::Leaves, Block::GlowLeaves, Block::BlossomLeaves}) {
        world.set({0, 6, 0}, leaf);
        draw();
        const auto open = depth;
        const auto still = pixels;
        const auto holes = std::count_if(open.begin(), open.end(), [](float d) { return d > 0.9999f; });
        require(holes > static_cast<int>(depth.size() / 20) && holes < static_cast<int>(depth.size() * 3 / 4),
                "Leaf blocks must retain both shaped foliage and visible cutout gaps");
        draw(false, 90.0f);
        require(pixels == still, "Leaf coverage and paint must not shimmer with elapsed time");
        world.set({0, 6, -1}, Block::Calcite);
        draw();
        int visibleBacking = 0;
        for (std::size_t i = 0; i < depth.size(); ++i)
            visibleBacking += open[i] > 0.9999f && depth[i] < 0.9999f;
        require(visibleBacking > holes * 9 / 10,
                "Adjacent solid faces must remain visible through leaf cutouts");
        world.set({0, 6, -1}, Block::Air);
    }
    const auto sample = [&](Block material) {
        world.set({0, 6, 0}, material);
        draw(true);
        glm::vec3 color(0);
        unsigned char low = 255, high = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            color += glm::vec3(pixels[i], pixels[i + 1], pixels[i + 2]);
            low = std::min(low, pixels[i + 1]); high = std::max(high, pixels[i + 1]);
        }
        require(high > low + 4, "Lush blocks must retain painted surface variation");
        return color / static_cast<float>(depth.size());
    };
    constexpr std::array blocks{
        Block::MossStone, Block::MossBrick, Block::RootedDirt, Block::Clay,
        Block::Silt, Block::RootWood, Block::AgedPlanks, Block::Calcite,
        Block::Moss, Block::BlossomLeaves, Block::AmberCrystal, Block::GlowMoss};
    std::array<glm::vec3, blocks.size()> colors{};
    for (std::size_t i = 0; i < blocks.size(); ++i) colors[i] = sample(blocks[i]);
    require(colors[3].r > colors[3].b + 15 && colors[8].g > colors[8].r + 10,
            "Clay and velvet moss must have distinct warm-earth and green palettes");
    require(colors[9].r > colors[9].g + 8 && colors[10].r > colors[10].b + 35,
            "Blossom leaves and amber crystals must retain pink and golden palettes");
    require(colors[11].g > colors[8].g + 15,
            "Glow moss must remain visibly luminous beside ordinary velvet moss");
    require(glm::length(colors[6] - colors[5]) > 10 && glm::length(colors[7] - colors[0]) > 20,
            "Weathered wood and pale calcite must not collapse to existing block colors");
    std::cout << "Lush materials passed: 12 painted blocks, stable leaf cutouts, visible backing faces\n";
}
} // namespace voxel
