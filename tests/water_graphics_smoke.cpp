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

glm::vec3 averageColor(const Pixels& pixels)
{
    glm::vec3 total(0);
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        total += glm::vec3(pixels[i], pixels[i + 1], pixels[i + 2]);
    return total / float(pixels.size() / 4);
}

double brightness(const Pixels& pixels)
{
    return glm::dot(averageColor(pixels), glm::vec3(.2126f, .7152f, .0722f));
}

double illuminatedFraction(const Pixels& pixels)
{
    std::size_t lit = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        lit += .2126 * pixels[i] + .7152 * pixels[i + 1] + .0722 * pixels[i + 2] > 8;
    return double(lit) / (pixels.size() / 4);
}

void verifySubmergedLights(int width, int height)
{
    World pool;
    // The deep bed and far wall lie outside the lamp's radius. The center
    // probe therefore measures light in the water, not a lit floor seen
    // through it. Both source models remain well to the left of the probe.
    for (int y = 18; y <= 50; ++y) for (int z = -18; z <= 18; ++z)
        for (int x = -12; x <= 12; ++x) {
            const bool boundary = x == -12 || x == 12 || z == -18 || z == 18 || y == 18 || y == 50;
            pool.set({x,y,z}, boundary ? Block::Stone : y <= 35 ? Block::Water : Block::Air);
        }
    const glm::ivec3 source{-4,33,0};
    const auto camera = lookAt({.5f,41.f,7.5f}, {.5f,36.f,.5f});
    WorldRenderer renderer;
    CallerTextures textures;
    const auto sample = [&](WorldRenderer& view, const Camera& eye,
                            std::span<const HubAsset> assets = {}) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthRange(0,1);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        view.render(pool, eye, width, height, false, true, 0, false, false,
                    assets, {}, {}, {}, {}, {}, {}, {}, true);
        textures.check();
        const int patchWidth = width / 6, patchHeight = height / 6;
        Pixels pixels(static_cast<std::size_t>(patchWidth) * patchHeight * 4);
        glReadPixels((width - patchWidth) / 2, (height - patchHeight) / 2,
                     patchWidth, patchHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        require(glGetError() == GL_NO_ERROR, "submerged-light fixture generated an OpenGL error");
        return pixels;
    };
    const auto dark = sample(renderer, camera);
    require(brightness(dark) <= 1, "water glows in a cave with no light source");

    pool.set(source, Block::Lantern);
    const auto warm = sample(renderer, camera);
    const double warmBrightness = brightness(warm);
    require(warmBrightness > 8 && illuminatedFraction(warm) > .65,
            "submerged lantern lights only its panes, not the surrounding water (water brightness "
            + std::to_string(warmBrightness) + ")");

    pool.set(source, Block::Crystal);
    const auto cool = sample(renderer, camera);
    const auto warmColor = averageColor(warm), coolColor = averageColor(cool);
    require(brightness(cool) > 8 && illuminatedFraction(cool) > .65,
            "submerged crystal does not illuminate a broad water patch");
    require(coolColor.b - coolColor.r > warmColor.b - warmColor.r + 5,
            "water ignores the color of its submerged light source");

    pool.set(source, Block::Water);
    const std::array<HubAsset,1> lamp{{{HubAssetKind::LanternCap, {-3.5f,33.3f,.5f}}}};
    const auto placed = sample(renderer, camera, lamp);
    require(brightness(placed) > 8 && illuminatedFraction(placed) > .65,
            "a placed underwater lantern asset cannot light the water");
    require(brightness(sample(renderer, camera)) <= 1,
            "removing a submerged lamp leaves stale water illumination");

    pool.set(source, Block::Lantern);
    require(difference(warm, sample(renderer, camera)) < .15,
            "switching from an asset lamp back to a block lamp retained old light data");
    // This partition ends at the waterline and lies outside the camera probe.
    // It blocks the submerged source, without hiding the sampled water face.
    for (int y = 19; y <= 35; ++y) for (int z = -17; z < 18; ++z)
        pool.set({-2,y,z}, Block::Stone);
    const double shadowed = brightness(sample(renderer, camera));
    require(shadowed < std::max(2.0, warmBrightness * .12),
            "submerged light leaks through a solid partition (water brightness "
            + std::to_string(shadowed) + ")");
    for (int y = 19; y <= 35; ++y) for (int z = -17; z < 18; ++z)
        pool.set({-2,y,z}, Block::Water);
    require(difference(warm, sample(renderer, camera)) < .15,
            "removing an underwater partition did not restore the light");

    const auto rebuilds = renderer.sceneRebuildCount();
    const auto nearby = lookAt({.65f,41.1f,7.65f}, {.5f,36.f,.5f});
    const auto farther = lookAt({.5f,44.f,11.7f}, {.5f,36.f,.5f});
    require(brightness(sample(renderer, nearby)) > warmBrightness * .6,
            "a small camera movement switches off submerged lighting");
    const auto distant = sample(renderer, farther);
    require(brightness(distant) > warmBrightness * .6,
            "water illumination disappears when backing away from the pool");
    require(renderer.sceneRebuildCount() == rebuilds,
            "moving around submerged lighting rebuilt static scenery");
    require(difference(warm, sample(renderer, camera)) < .15,
            "returning to the pool view retained camera-dependent light data");
    WorldRenderer fresh;
    require(difference(distant, sample(fresh, farther)) < .15,
            "cached submerged lighting differs from a fresh renderer");
    std::cout << "Submerged water lights passed: broad warm/cool pools " << warmBrightness
              << '/' << brightness(cool) << ", asset lamp " << brightness(placed)
              << ", shadow " << shadowed
              << "; darkness, source edits, stone occlusion, distance and cache stability\n";
}

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

    // A swimmer sees the solid lakebed directly: no water surface is drawn
    // between this camera and the bed. It still needs underwater absorption.
    const auto aboveBeforeDive = sample(basin, camera);
    const auto swimmer = lookAt({0.5f, 4.7f, 5.5f}, {0.5f, 3.0f, -1.0f});
    const auto submergedBrick = sample(basin, swimmer);
    World drained = basin;
    for (int y = 3; y <= 5; ++y) layer(drained, y, Block::Air);
    const auto dryBed = sample(drained, swimmer);
    const double submergedChange = difference(submergedBrick, dryBed);
    require(submergedChange > 2.0 && changedFraction(submergedBrick, dryBed) > 0.25,
            "submerged opaque bed has no underwater haze (change " + std::to_string(submergedChange) + ")");
    layer(basin, 2, Block::Sand);
    require(difference(submergedBrick, sample(basin, swimmer)) > 2.0,
            "underwater haze hides nearby lakebed material detail");
    layer(basin, 2, Block::Brick);
    sample(basin, swimmer);
    require(difference(aboveBeforeDive, sample(basin, camera)) < 0.15,
            "underwater tint remains after the camera leaves water");

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
              << ", swimmer bed haze " << submergedChange
              << "; immediate water exit, opaque occlusion, fixed time, resize, world/renderer switches and texture bindings\n";
    verifySubmergedLights(width, height);
}

} // namespace voxel
