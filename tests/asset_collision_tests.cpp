#include "asset_collision.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <glm/geometric.hpp>

namespace {
using namespace voxel;
int failures = 0;

void check(bool condition, const std::string& description)
{
    if (!condition) { std::cerr << description << '\n'; ++failures; }
}

bool near(float a, float b, float tolerance = 0.00005f)
{
    return std::abs(a - b) < tolerance;
}

AssetCollision collisionFor(HubAsset asset)
{
    AssetCollision collision;
    collision.rebuild(std::span(&asset, 1));
    return collision;
}

glm::vec3 rotate(glm::vec3 point, int facing)
{
    switch (facing) {
    case 1: return {point.z, point.y, -point.x};
    case 2: return {-point.x, point.y, -point.z};
    case 3: return {-point.z, point.y, point.x};
    default: return point;
    }
}

void transformedSurfaces()
{
    constexpr std::array<HubAssetKind, 5> kinds{{HubAssetKind::Boulder, HubAssetKind::MossyLog,
        HubAssetKind::ShelfFungi, HubAssetKind::GiantMushroom, HubAssetKind::RootArch}};
    constexpr std::array<glm::vec3, 5> aims{{{0,7,0}, {.6f,7,0}, {0,7,.2f},
        {.4f,7,0}, {0,7,0}}};
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        for (const std::uint32_t variant : {0u, 29u}) {
            const auto base = collisionFor({kinds[i], {0,0,0}, 0, variant});
            const auto expected = base.raycast(aims[i], {0,-1,0}, 15);
            check(expected.has_value(), "parkour model has an actual top surface");
            if (!expected) continue;
            for (int facing = 0; facing < 4; ++facing) {
                const glm::vec3 offset(-21.25f, 17.5f, 40.75f);
                const auto placed = collisionFor({kinds[i], offset, facing, variant});
                const auto hit = placed.raycast(offset + rotate(aims[i], facing), {0,-3,0}, 15);
                check(hit && glm::length(hit->point - offset - rotate(expected->point, facing)) < .0001f
                      && near(hit->distance, expected->distance, .0001f) && hit->normal.y > 0,
                      "rotated and translated variant keeps the visible top, normal and ray distance");
                if (!hit) continue;
                const auto minimum = hit->point + glm::vec3(-.03f, 2, -.03f);
                const auto maximum = minimum + glm::vec3(.06f, 1.8f, .06f);
                const float fall = placed.sweep(minimum, maximum, 1, -5);
                check(fall < 0 && fall > -2.001f,
                      "a falling body lands on the model surface instead of its anchor block");
                const auto restingMin = minimum + glm::vec3(0, fall, 0);
                const auto restingMax = maximum + glm::vec3(0, fall, 0);
                check(!placed.overlaps(restingMin, restingMax), "exact landed contact is not penetration");
                check(placed.sweep(restingMin, restingMax, 1, -.05f) > -.0001f,
                      "a landed body remains supported on the fractional model top");
            }
        }
    }
}

void everyKindHasSurfaces()
{
    for (std::size_t kind = 0; kind < HubAssetCount; ++kind) {
        for (int facing = 0; facing < 4; ++facing) {
            const HubAsset asset{static_cast<HubAssetKind>(kind), {-16.5f,19.25f,-12.75f},
                                 facing, std::uint32_t(41 + facing)};
            const auto mesh = buildHubAsset(asset);
            auto collision = collisionFor(asset);
            if (isItemPickup(asset.kind)) {
                const auto low = asset.position - glm::vec3(1);
                const auto high = asset.position + glm::vec3(1);
                check(collision.empty() && !collision.overlaps(low, high),
                      "legacy keys and dropped items never occupy player space");
                check(near(collision.sweep(low, high, 0, 4), 4)
                      && !collision.raycast(asset.position + glm::vec3(0, 2, 0), {0,-1,0}, 4),
                      "dropped items neither stop movement nor provide a solid surface");
                continue;
            }
            check(!collision.empty(), "every scenery and chest kind receives collision geometry");
            float area = 0;
            glm::vec3 center{}, normal{};
            for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const auto a = mesh.vertices[mesh.indices[i]].position;
                const auto b = mesh.vertices[mesh.indices[i+1]].position;
                const auto c = mesh.vertices[mesh.indices[i+2]].position;
                const auto cross = glm::cross(b-a,c-a);
                if (glm::length(cross) <= area) continue;
                area = glm::length(cross);
                center = (a+b+c)/3.0f;
                normal = glm::normalize(cross);
            }
            check(collision.overlaps(center-glm::vec3(.001f), center+glm::vec3(.001f)),
                  "visible surface intersects a small box for every kind and facing");
            const auto front = collision.raycast(center+normal*.01f, -normal, .011f);
            const auto back = collision.raycast(center-normal*.01f, normal, .011f);
            check(front && back, "asset surfaces are collidable from both sides");
        }
    }
}
} // namespace

int main()
{
    AssetCollision empty;
    check(empty.empty() && !empty.overlaps({0,0,0}, {1,1,1}), "empty collision index contains no geometry");
    check(near(empty.sweep({0,0,0}, {1,1,1}, 0, 4), 4), "empty scene allows full motion");
    check(!empty.raycast({0,0,0}, {0,-1,0}, 10), "empty scene has no ray hit");

    auto anvil = collisionFor({HubAssetKind::Anvil, {0,0,0}});
    check(near(anvil.sweep({-.1f,2,-.1f}, {.1f,3.8f,.1f}, 1, -20), -.31f),
          "a long fall lands exactly on the anvil's 1.69m top without tunneling");
    check(near(anvil.sweep({-.1f,-1,-.1f}, {.1f,-.1f,.1f}, 1, 20), .1f),
          "upward motion hits the underside of the anvil base");
    check(near(anvil.sweep({-2,.4f,-.1f}, {-1.8f,.6f,.1f}, 0, 8), 1.27f),
          "positive horizontal movement hits the actual base wall");
    check(near(anvil.sweep({1.8f,.4f,-.1f}, {2,.6f,.1f}, 0, -8), -1.27f),
          "negative horizontal movement hits the opposite base wall");
    check(near(anvil.sweep({-.1f,1.69f,-.1f}, {.1f,3.49f,.1f}, 0, .2f), .2f),
          "resting on a top permits tangent motion across triangle seams");
    check(near(anvil.sweep({-.1f,1.69f,-.1f}, {.1f,3.49f,.1f}, 1, .7f), .7f),
          "a touching body may jump away from a surface");
    check(anvil.overlaps({-.1f,.4f,-.1f}, {.1f,.6f,.1f}),
          "a box teleported entirely inside closed geometry is occupied");
    check(!anvil.overlaps({-.1f,1.69f,-.1f}, {.1f,3.49f,.1f}), "touching top is valid placement");

    auto arch = collisionFor({HubAssetKind::RootArch,{0,0,0},0,17});
    check(!arch.overlaps({-.3f,.1f,-.3f}, {.3f,1.9f,.3f}), "arch opening stays empty");
    check(near(arch.sweep({-.3f,.1f,-4}, {.3f,1.9f,-3.4f}, 2, 8), 8),
          "a full body can pass through the arch without an invisible bounding-box wall");
    check(!arch.raycast({0,1,-4}, {0,0,1}, 8), "ray passes through the arch opening");
    check(!arch.raycast({0,1,0}, {0,-1,0}, 4), "arch has no phantom floor in its middle");

    auto mushroom = collisionFor({HubAssetKind::GiantMushroom,{0,0,0},0,29});
    check(!mushroom.overlaps({.5f,.3f,-.3f}, {1.1f,2.1f,.3f}),
          "space beside a mushroom stem and below the cap remains walkable");
    check(mushroom.sweep({.5f,.3f,-.3f}, {1.1f,2.1f,.3f}, 1, 2) < 1,
          "jumping beneath a mushroom hits its actual cap underside");

    auto log = collisionFor({HubAssetKind::MossyLog,{0,0,0},0,11});
    check(!log.overlaps({-.1f,.34f,-.04f}, {.1f,.42f,.04f}), "hollow log bore is not filled by collision");
    check(!log.raycast({-2,.38f,0}, {1,0,0}, 4), "ray passes through the open log bore");
    auto shelf = collisionFor({HubAssetKind::ShelfFungi,{0,0,0},0,11});
    check(!shelf.overlaps({-1,-.2f,.5f}, {-.9f,0,.6f}), "empty corners of a shelf model have no phantom surface");

    transformedSurfaces();
    everyKindHasSurfaces();

    // Exercise negative bins, nearest-hit ordering, large sparse queries, and
    // replacing a populated index without keeping stale editor placements.
    std::vector<HubAsset> many;
    for (int i = -500; i < 500; ++i) many.push_back({HubAssetKind::Anvil, {float(i*12),10,0}, i&3});
    AssetCollision indexed;
    indexed.rebuild(many);
    const auto nearest = indexed.raycast({-5999.9f,20,0}, {0,-1,0}, 30);
    check(nearest && near(nearest->point.y,11.69f,.001f), "spatial bins find a distant negative-coordinate model");
    check(indexed.raycast({-100000,10.5f,0},{1,0,0},200000).has_value(),
          "long queries stay bounded by the actual sparse scene");
    const auto shared = indexed;
    indexed.rebuild({});
    check(indexed.empty() && !shared.empty(), "rebuild replaces geometry while copies retain their immutable scene");
    indexed.rebuild(std::span(many).first(1));
    indexed.clear();
    check(indexed.empty(), "clear removes all model collisions");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    check(anvil.overlaps({nan,0,0},{1,1,1}), "invalid placement bounds are rejected safely");
    check(anvil.sweep({0,0,0},{1,1,1},3,1) == 0 && anvil.sweep({0,0,0},{1,1,1},0,infinity) == 0,
          "invalid movement axis and displacement are rejected safely");
    check(!anvil.raycast({0,3,0},{0,0,0},10) && !anvil.raycast({nan,3,0},{0,-1,0},10)
          && !anvil.raycast({0,3,0},{0,-1,0},-1), "invalid rays have no hit");
    const std::array<HubAsset,3> invalid{{{HubAssetKind::Anvil,{nan,0,0}},
        {static_cast<HubAssetKind>(HubAssetCount),{0,0,0}}, {HubAssetKind::Anvil,{infinity,0,0}}}};
    indexed.rebuild(invalid);
    check(indexed.empty(), "invalid placements do not poison the index");

    if (!failures) std::cout << "Asset collision passed: exact surfaces, sweeps, openings, rotations and spatial indexing\n";
    return failures ? 1 : 0;
}
