#include "surface_relief_smoke.h"
#include "camera.h"
#include "graphics/renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace voxel {
namespace {
constexpr int Width = 640, Height = 360;
using Pixel = std::array<unsigned char, 4>;
struct Frame {
    std::vector<Pixel> color = std::vector<Pixel>(Width * Height);
    std::vector<float> depth = std::vector<float>(Width * Height);
};

void require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

void aim(Camera& camera, glm::vec3 target)
{
    const auto direction = glm::normalize(target - camera.position()), old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}

Frame draw(WorldRenderer& renderer, const World& world, const Camera& camera, bool underground,
           std::span<const HubAsset> assets = {})
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, Width, Height);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthRange(0, 1);
    renderer.render(world, camera, Width, Height, false, underground, 0, false, false, assets);
    Frame result;
    glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, result.color.data());
    glReadPixels(0, 0, Width, Height, GL_DEPTH_COMPONENT, GL_FLOAT, result.depth.data());
    require(glGetError() == GL_NO_ERROR, "Surface relief rendering generated an OpenGL error");
    return result;
}

glm::vec3 unproject(const glm::mat4& inverse, std::size_t pixel, float depth)
{
    const auto point = inverse * glm::vec4((static_cast<float>(pixel % Width) + .5f) / Width * 2 - 1,
        (static_cast<float>(pixel / Width) + .5f) / Height * 2 - 1, depth * 2 - 1, 1);
    return glm::vec3(point) / point.w;
}

void verifyStairs()
{
    constexpr std::array materials{Block::Planks, Block::Stone, Block::Brick, Block::Limestone,
        Block::Slate, Block::MossStone, Block::Path, Block::AgedPlanks};
    constexpr glm::ivec3 anchor{15, 40, 15}; // Samples straddle the X and Z chunk seams.
    World world;
    WorldRenderer renderer;
    Camera camera;
    camera.setFieldOfView(48);
    std::vector<glm::ivec3> previous;
    for (const auto material : materials) for (int facing = 0; facing < 4; ++facing) {
        for (const auto cell : previous) world.set(cell, Block::Air);
        previous.clear();
        const auto stairs = stairBlock(material, facing);
        const auto front = -stairUpDirection(stairs);
        const glm::ivec3 side{front.z, 0, -front.x};
        for (int x = -1; x <= 1; ++x) {
            const auto cell = anchor + side * x;
            world.set(cell, stairs);
            previous.push_back(cell);
        }
        const auto center = glm::vec3(anchor) + glm::vec3(.5f);
        camera.setPosition(center + glm::vec3(front) * 4.6f + glm::vec3(side) * .6f + glm::vec3(0, 2.5f, 0));
        aim(camera, center);
        const auto inverse = glm::inverse(camera.projectionMatrix(float(Width) / Height) * camera.viewMatrix());
        const auto flat = draw(renderer, world, camera, false);
        const auto raised = draw(renderer, world, camera, true);
        std::vector<std::size_t> raisedPixels;
        std::size_t treads = 0, risers = 0, readable = 0;
        for (std::size_t i = 0; i < flat.depth.size(); ++i) {
            if (flat.depth[i] > .9999f || raised.depth[i] > flat.depth[i] - .000005f) continue;
            const auto original = unproject(inverse, i, flat.depth[i]);
            const auto detail = unproject(inverse, i, raised.depth[i]);
            // Ignore edge pixels: multisample silhouettes may combine a near
            // step with distant background, unlike an interior face sample.
            if (glm::length(original - detail) > .12f) continue;
            raisedPixels.push_back(i);
            const float localY = original.y - anchor.y;
            const bool tread = std::abs(localY - .5f) < .003f || std::abs(localY - 1.0f) < .003f;
            treads += tread;
            risers += !tread && localY > .03f && localY < .97f;
            readable += std::max({raised.color[i][0], raised.color[i][1], raised.color[i][2]}) > 15;
        }
        const auto label = std::string(blockName(stairs)) + " facing " + std::to_string(facing);
        require(treads > 40 && risers > 20, "Stairs lack raised treads or risers: " + label
            + " (" + std::to_string(treads) + "/" + std::to_string(risers) + ")");
        require(readable > raisedPixels.size() * 9 / 10, "Stair relief has black self-shadowed facets: " + label);

        const auto torch = anchor + front * 2 + side * 2 + glm::ivec3(0, 2, 0);
        world.set(torch, Block::Lantern); previous.push_back(torch);
        const auto lit = draw(renderer, world, camera, true);
        std::size_t warmed = 0;
        for (const auto i : raisedPixels) warmed += lit.color[i][0] > raised.color[i][0] + 8;
        require(warmed > raisedPixels.size() / 4, "Point lights do not reach raised stair faces: " + label);
        const auto rebuilds = renderer.sceneRebuildCount();
        const auto repeated = draw(renderer, world, camera, true);
        require(repeated.color == lit.color && repeated.depth == lit.depth
                && renderer.sceneRebuildCount() == rebuilds,
                "Stationary stair surfaces changed or rebuilt between frames: " + label);
    }
}

void verifyAdditionalSurfaces()
{
    World world;
    WorldRenderer renderer;
    Camera camera;
    camera.setPosition({.5f, 40.5f, 4.5f});
    aim(camera, {.5f, 40.5f, .5f});
    constexpr std::array materials{Block::MossBrick, Block::RoofTiles, Block::MossStone,
        Block::Calcite, Block::Crystal, Block::AmberCrystal};
    for (const auto material : materials) {
        for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x)
            world.set({x, 40 + y, 0}, material);
        const auto flat = draw(renderer, world, camera, false);
        const auto raised = draw(renderer, world, camera, true);
        std::size_t visibleDepth = 0;
        for (std::size_t i = 0; i < flat.depth.size(); ++i)
            visibleDepth += flat.depth[i] < .9999f && raised.depth[i] < flat.depth[i] - .000005f;
        require(visibleDepth > 40, "Expanded block material lacks visible depth: " + std::string(blockName(material)));
    }
}

void verifyPavingReplacement()
{
    World world;
    world.set({0, 40, 0}, Block::Path);
    Camera camera;
    camera.setFieldOfView(45);
    camera.setPosition({.5f, 44, 3});
    aim(camera, {.5f, 41, .5f});
    WorldRenderer renderer;
    const auto original = draw(renderer, world, camera, true);
    const auto originalTriangles = renderer.triangleCount();
    const std::array assets{HubAsset{HubAssetKind::PathPaving, {.5f, 41, .5f}, 0, 17}};
    const auto dressed = draw(renderer, world, camera, true, assets);
    require(renderer.triangleCount() < originalTriangles + buildHubAsset(assets[0]).indices.size() / 3,
            "Authored paving retains overlapping integral slabs below it");
    const auto rebuilds = renderer.sceneRebuildCount();
    const auto repeated = draw(renderer, world, camera, true, assets);
    require(repeated.depth == dressed.depth && repeated.color == dressed.color
            && renderer.sceneRebuildCount() == rebuilds,
            "An unchanged paving replacement rebuilt its terrain or changed frames");

    // A plain cube under the same asset is the depth reference: its top must
    // match a dressed Path block with no integral stones poking through.
    world.set({0, 40, 0}, Block::Dirt);
    const auto plain = draw(renderer, world, camera, true, assets);
    const auto inverse = glm::inverse(camera.projectionMatrix(float(Width) / Height) * camera.viewMatrix());
    std::size_t capPixels = 0;
    for (std::size_t i = 0; i < plain.depth.size(); ++i) {
        if (plain.depth[i] > .9999f) continue;
        const auto point = unproject(inverse, i, plain.depth[i]);
        if (point.y < 41.002f || point.x < .03f || point.x > .97f || point.z < .03f || point.z > .97f) continue;
        ++capPixels;
        require(std::abs(plain.depth[i] - dressed.depth[i]) < .000005f,
                "Integral path slabs protrude through an authored paving asset");
    }
    require(capPixels > 100, "Paving replacement test missed its top surface");
    world.set({0, 40, 0}, Block::Path);
    draw(renderer, world, camera, true, assets);
    const auto restored = draw(renderer, world, camera, true);
    require(restored.depth == original.depth && restored.color == original.color
            && renderer.triangleCount() == originalTriangles,
            "Removing an authored paving tile failed to restore its integral path geometry");
}
} // namespace

void verifySurfaceRelief(int width, int height)
{
    require(width >= Width && height >= Height, "Framebuffer is too small for the surface relief smoke test");
    verifyStairs();
    verifyAdditionalSurfaces();
    verifyPavingReplacement();
    std::cout << "Surface relief passed: 32 stair variants, real tread/riser depth, point lighting, chunk seams, stable cached draws, six expanded surfaces and paving replacements\n";
}
} // namespace voxel
