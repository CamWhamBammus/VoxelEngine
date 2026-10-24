#include "ore_material_smoke.h"
#include "camera.h"
#include "graphics/palette_previews.h"
#include "graphics/renderer.h"
#include "ores.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace voxel {
namespace {
using Pixel = std::array<unsigned char, 4>;
using Image = std::vector<Pixel>;

void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

std::size_t difference(const Image& a, const Image& b)
{
    std::size_t changed = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto delta = std::abs(int(a[i][0]) - int(b[i][0]))
            + std::abs(int(a[i][1]) - int(b[i][1])) + std::abs(int(a[i][2]) - int(b[i][2]));
        changed += a[i][3] > 127 && b[i][3] > 127 && delta > 18;
    }
    return changed;
}

void verifyPreviewColors()
{
    PalettePreviews previews;
    std::vector<int> ids;
    for (const auto& ore : oreCatalog()) ids.push_back(PalettePreviews::blockId(ore.block));
    previews.ensure(ids);
    require(previews.generatedCount() == oreCatalog().size(), "Ore palette omitted one of the eighteen source blocks");
    const auto texture = previews.texture();
    previews.ensure(ids);
    require(previews.texture() == texture && previews.generatedCount() == ids.size(),
            "Opening the ore palette again rebuilt cached thumbnails");
    GLint previousActive = 0, previousTexture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActive);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glBindTexture(GL_TEXTURE_2D, texture);
    std::vector<Pixel> atlas(static_cast<std::size_t>(PalettePreviews::Width) * PalettePreviews::Height);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas.data());
    glBindTexture(GL_TEXTURE_2D, previousTexture);
    glActiveTexture(previousActive);
    require(glGetError() == GL_NO_ERROR, "Reading ore preview tiles generated an OpenGL error");
    std::vector<Image> tiles;
    for (const auto id : ids) {
        Image tile;
        const int left = id % PalettePreviews::Columns * PalettePreviews::TileSize;
        const int bottom = id / PalettePreviews::Columns * PalettePreviews::TileSize;
        for (int y = 0; y < PalettePreviews::TileSize; ++y)
            for (int x = 0; x < PalettePreviews::TileSize; ++x)
                tile.push_back(atlas[static_cast<std::size_t>(bottom + y) * PalettePreviews::Width + left + x]);
        std::set<Pixel> colors;
        std::size_t covered = 0;
        for (const auto& pixel : tile) if (pixel[3] > 127) { ++covered; colors.insert(pixel); }
        require(covered > tile.size() / 3, "Ore preview no longer has a complete cube silhouette");
        require(colors.size() > 64, "Ore preview lost its painted mineral detail");
        tiles.push_back(std::move(tile));
    }
    for (std::size_t i = 0; i < tiles.size(); ++i)
        for (std::size_t j = i + 1; j < tiles.size(); ++j)
            require(difference(tiles[i], tiles[j]) > 80,
                    "Two ore thumbnails render as the same material: " + std::string(blockName(oreCatalog()[i].block))
                    + " and " + std::string(blockName(oreCatalog()[j].block)));

    const auto colored = [&](ItemId item, auto predicate) {
        const auto& tile = tiles[static_cast<std::size_t>(item)];
        return std::count_if(tile.begin(), tile.end(), [&](const auto& p) { return p[3] > 127 && predicate(p); });
    };
    require(colored(ItemId::GoldOre, [](const Pixel& p) { return p[0] > p[2] * 1.3f && p[1] > p[2] * 1.12f; }) > 50,
            "Gold preview lost its golden nuggets");
    require(colored(ItemId::Amethyst, [](const Pixel& p) { return p[0] > p[1] * 1.12f && p[2] > p[1] * 1.20f; }) > 50,
            "Amethyst preview lost its violet crystal faces");
    require(colored(ItemId::Ruby, [](const Pixel& p) { return p[0] > p[1] * 1.25f && p[0] > p[2] * 1.15f; }) > 50,
            "Ruby preview lost its crimson crystals");
    require(colored(ItemId::Sapphire, [](const Pixel& p) { return p[2] > p[0] * 1.25f && p[2] > p[1] * 1.15f; }) > 50,
            "Sapphire preview lost its blue crystal faces");
    require(colored(ItemId::Emerald, [](const Pixel& p) { return p[1] > p[0] * 1.25f && p[1] > p[2] * 1.12f; }) > 50,
            "Emerald preview lost its green crystals");
    require(colored(ItemId::MithrilOre, [](const Pixel& p) { return p[1] > p[0] * 1.20f && p[2] > p[0] * 1.12f; }) > 50,
            "Mithril preview lost its turquoise metal seams");
}
} // namespace

void verifyOreMaterials(int width, int height)
{
    require(width >= 640 && height >= 360, "Framebuffer is too small for the ore material smoke test");
    verifyPreviewColors();
    width = 640; height = 360;
    World world;
    constexpr glm::ivec3 anchor{0, 40, 0};
    Camera camera;
    camera.setPosition({.5f, 40.5f, 4.0f});
    camera.rotate(35, 25);
    WorldRenderer renderer;
    constexpr int patch = 180;
    std::vector<float> depth(patch * patch);
    std::vector<Image> frames;
    const auto draw = [&](float time = 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthRange(0, 1);
        renderer.render(world, camera, width, height, false, true, time, true, false);
        Image pixels(patch * patch);
        glReadPixels(width / 2 - patch / 2, height / 2 - patch / 2, patch, patch,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glReadPixels(width / 2 - patch / 2, height / 2 - patch / 2, patch, patch,
                     GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
        require(glGetError() == GL_NO_ERROR, "Ore world rendering generated an OpenGL error");
        return pixels;
    };
    const auto plane = [&](Block block) {
        for (int y = -2; y <= 2; ++y) for (int x = -2; x <= 2; ++x)
            world.set(anchor + glm::ivec3(x, y, 0), block);
    };
    plane(Block::Dirt);
    draw();
    const auto flatDepth = depth;
    for (const auto& ore : oreCatalog()) {
        plane(ore.block);
        const auto pixels = draw();
        require(std::all_of(depth.begin(), depth.end(), [](float d) { return d > .8f && d < .9999f; }),
                "An appended ore ID lost its solid rendered face");
        std::set<Pixel> colors(pixels.begin(), pixels.end());
        require(colors.size() > 100, "World ore material collapsed to a flat color");
        if (ore.block != Block::Crystal && ore.block != Block::AmberCrystal) {
            std::size_t raised = 0;
            for (std::size_t i = 0; i < depth.size(); ++i) raised += depth[i] < flatDepth[i] - .00001f;
            require(raised > 40, "Ore has painted veins but no visible embedded relief: " + std::string(blockName(ore.block)));
        }
        const auto rebuilds = renderer.sceneRebuildCount();
        const auto repeated = draw();
        require(repeated == pixels, "Still ore material changes between identical frames");
        require(renderer.sceneRebuildCount() == rebuilds, "Displaying ore materials rebuilt static terrain");
        frames.push_back(pixels);
    }
    for (std::size_t i = 0; i < frames.size(); ++i)
        for (std::size_t j = i + 1; j < frames.size(); ++j)
            require(difference(frames[i], frames[j]) > frames[i].size() / 20,
                    "World ore shader maps distinct source blocks onto the same material");
    require(world.takeDirtyChunks().size() == World::ChunkCount,
            "Rendering ore materials consumed the world's unrelated pending chunk updates");
    std::cout << "Ore materials passed: 18 distinct painted blocks, 16 embedded relief materials, creative previews and stable cached draws\n";
}
} // namespace voxel
