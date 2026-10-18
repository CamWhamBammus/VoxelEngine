#include "water_graphics_smoke.h"
#include "camera.h"
#include "graphics/renderer.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace voxel {
namespace {

using Pixels = std::vector<unsigned char>;

void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error("Water rendering: " + message);
}

Camera lookAt(glm::vec3 eye, glm::vec3 target)
{
    Camera camera;
    camera.setPosition(eye);
    const auto direction = glm::normalize(target - eye);
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x)) + 125.0f,
                  glm::degrees(std::asin(direction.y)) + 25.0f);
    camera.setFieldOfView(50.0f);
    return camera;
}

void layer(World& world, int y, Block material)
{
    for (int z = -7; z <= 7; ++z) {
        for (int x = -7; x <= 7; ++x) world.set({x, y, z}, material);
    }
}

// Compare a broad patch, allowing tiny driver rounding differences rather than
// tying the test to one exact painted pattern, reflection, or foam pixel.
double difference(const Pixels& first, const Pixels& second)
{
    double total = 0;
    for (std::size_t i = 0; i < first.size(); i += 4) {
        for (int channel = 0; channel < 3; ++channel) {
            total += std::abs(int(first[i + channel]) - int(second[i + channel]));
        }
    }
    return total / (first.size() / 4 * 3);
}

double changedFraction(const Pixels& first, const Pixels& second)
{
    std::size_t changed = 0;
    for (std::size_t i = 0; i < first.size(); i += 4) {
        int change = 0;
        for (int channel = 0; channel < 3; ++channel) {
            change = std::max(change, std::abs(int(first[i + channel]) - int(second[i + channel])));
        }
        changed += change > 3;
    }
    return double(changed) / (first.size() / 4);
}

// Sentinel textures exercise the units used by the opaque color/depth capture.
// They are complete textures so this test does not provoke driver warnings.
struct CallerTextures {
    GLint active = 0;
    std::array<GLint, 2> previous{};
    std::array<GLuint, 2> sentinel{};

    CallerTextures()
    {
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGenTextures(2, sentinel.data());
        constexpr std::array<unsigned char, 4> pixel{31, 63, 95, 255};
        for (int index = 0; index < 2; ++index) {
            glActiveTexture(GL_TEXTURE2 + index);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous[index]);
            glBindTexture(GL_TEXTURE_2D, sentinel[index]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        glActiveTexture(GL_TEXTURE3);
    }

    void check() const
    {
        GLint current = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &current);
        require(current == GL_TEXTURE3, "changed the caller's active texture unit");
        for (int index = 0; index < 2; ++index) {
            glActiveTexture(GL_TEXTURE2 + index);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &current);
            require(current == static_cast<GLint>(sentinel[index]), "changed a scene texture binding");
        }
        glActiveTexture(GL_TEXTURE3);
    }

    ~CallerTextures()
    {
        for (int index = 0; index < 2; ++index) {
            glActiveTexture(GL_TEXTURE2 + index);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous[index]));
        }
        glDeleteTextures(2, sentinel.data());
        glActiveTexture(static_cast<GLenum>(active));
    }
};

} // namespace

void verifyWaterRendering(int width, int height)
{
    require(width >= 160 && height >= 120, "framebuffer is too small for the basin fixture");
    width = std::min(width, 400);
    height = std::min(height, 300);
    World basin;
    for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) basin.set({x, y, z}, Block::Air);
        }
    }
    layer(basin, 4, Block::Sand);
    layer(basin, 5, Block::Water);
    for (int edge = -8; edge <= 8; ++edge) {
        for (const auto cell : {glm::ivec3(edge, 5, -8), glm::ivec3(edge, 5, 8),
                               glm::ivec3(-8, 5, edge), glm::ivec3(8, 5, edge)}) {
            basin.set(cell, Block::Stone);
        }
    }
    const auto camera = lookAt({0.5f, 10.5f, 9.5f}, {0.5f, 6.0f, 0.5f});
    WorldRenderer renderer;
    CallerTextures textures;
    const auto capture = [&](WorldRenderer& view, const World& world, const Camera& eye,
                             float time, int renderWidth, int renderHeight) {
        view.render(world, eye, renderWidth, renderHeight, false, true, time);
        textures.check();
        const int patchWidth = renderWidth / 4, patchHeight = renderHeight / 4;
        Pixels pixels(static_cast<std::size_t>(patchWidth) * patchHeight * 4);
        glReadPixels((renderWidth - patchWidth) / 2, (renderHeight - patchHeight) / 2,
                     patchWidth, patchHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        const auto error = glGetError();
        require(error == GL_NO_ERROR, "OpenGL error " + std::to_string(error));
        return pixels;
    };
    const auto sample = [&](const World& world, const Camera& eye, float time = 0.0f) {
        return capture(renderer, world, eye, time, width, height);
    };

    const auto sand = sample(basin, camera);
    require(difference(sand, sample(basin, camera)) < 0.15, "fixed-time rendering is not deterministic");
    layer(basin, 4, Block::Brick);
    const auto brick = sample(basin, camera);
    const double bedChange = difference(sand, brick);
    require(bedChange > 3.0 && changedFraction(sand, brick) > 0.25,
            "submerged floor is hidden or stale (sand/brick change " + std::to_string(bedChange) + ")");

    const auto animated = sample(basin, camera, 1.75f);
    const double motion = difference(brick, animated);
    require(motion > 0.3 && changedFraction(brick, animated) > 0.03,
            "surface stopped evolving (change " + std::to_string(motion) + ")");
    require(difference(brick, sample(basin, camera)) < 0.15,
            "returning to a fixed time retained a previous frame");

    // Returning from another target size and another world must refresh both
    // opaque textures. A new renderer is an independent cache reference.
    capture(renderer, basin, camera, 0.0f, width / 2, height / 2);
    require(difference(brick, sample(basin, camera)) < 0.15, "resize retained a stale scene capture");
    World replacement = basin;
    layer(replacement, 4, Block::Sand);
    require(difference(sand, sample(replacement, camera)) < 0.15, "world switch retained the previous bed");
    require(difference(brick, sample(basin, camera)) < 0.15, "switching back retained another world's capture");
    {
        WorldRenderer fresh;
        require(difference(brick, capture(fresh, basin, camera, 0.0f, width, height)) < 0.15,
                "reused renderer differs from a fresh renderer");
    }

    // A foreground opaque wall hides water behind it even though water draws
    // later. Its center must not inherit animation or underwater bed edits.
    World covered = basin;
    for (int x = -7; x <= 7; ++x) {
        for (int y = 5; y <= 11; ++y) covered.set({x, y, 4}, Block::Planks);
    }
    const auto wall = sample(covered, camera);
    layer(covered, 4, Block::Sand);
    require(difference(wall, brick) > 5.0, "opaque occluder did not cover the basin probe");
    require(difference(wall, sample(covered, camera, 1.75f)) < 0.15,
            "water or its refraction rendered through an opaque occluder");

    layer(basin, 4, Block::Water);
    layer(basin, 3, Block::Water);
    layer(basin, 2, Block::Brick);
    const double depthChange = difference(brick, sample(basin, camera));
    require(depthChange > 2.0, "water ignored the lakebed depth (change " + std::to_string(depthChange) + ")");

    // Looking up just beneath the surface sees a distant ceiling through only
    // a short water path. Air above the surface must not be counted as water.
    const auto nearSurface = lookAt({0.5f, 5.7f, 0.5f}, {0.5f, 10.0f, -1.0f});
    const auto deeper = lookAt({0.5f, 3.3f, 0.5f}, {0.5f, 7.6f, -1.0f});
    layer(basin, 13, Block::Sand);
    const auto nearSand = sample(basin, nearSurface);
    const auto deepSand = sample(basin, deeper);
    layer(basin, 13, Block::Brick);
    const double nearChange = difference(nearSand, sample(basin, nearSurface));
    const double deepChange = difference(deepSand, sample(basin, deeper));
    require(nearChange > 8.0, "near-surface upward view hides the ceiling (change " + std::to_string(nearChange) + ")");
    require(deepChange < nearChange * 0.85,
            "underwater absorption ignores camera-to-surface depth (near/deep "
            + std::to_string(nearChange) + "/" + std::to_string(deepChange) + ")");
    std::cout << "Water rendering passed: visible bed " << bedChange << ", animation " << motion
              << ", depth " << depthChange << ", underwater near/deep " << nearChange << '/' << deepChange
              << "; opaque occlusion, fixed time, resize, world/renderer switches and texture bindings\n";
}

} // namespace voxel
