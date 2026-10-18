#include "hub_assets.h"
#include "scene.h"
#include "graphics/asset_builder.h"

#include <algorithm>
#include <cmath>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

std::uint32_t variation(int x, int z, std::uint32_t seed)
{
    auto value = seed ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u)
                      ^ (static_cast<std::uint32_t>(z) * 0x85ebca6bu);
    value ^= value >> 16; value *= 0x7feb352du;
    return value ^ (value >> 15);
}

glm::vec3 buildingPoint(const HubBuilding& building, glm::vec3 p)
{
    switch (building.facing) {
    case 1: p = {p.z, p.y, building.width - p.x}; break;
    case 2: p = {building.width - p.x, p.y, building.depth - p.z}; break;
    case 3: p = {building.depth - p.z, p.y, p.x}; break;
    }
    return glm::vec3(building.origin) + p;
}

bool air(const World& world, glm::vec3 point)
{
    const glm::ivec3 cell(glm::floor(point));
    return World::contains(cell) && world.get(cell) == Block::Air;
}

} // namespace

std::vector<HubAsset> planHubAssets(const World& world)
{
    std::vector<HubAsset> assets;
    const auto add = [&](HubAssetKind kind, glm::vec3 position, int facing = 0, std::uint32_t seed = 0) {
        if (World::contains(glm::ivec3(glm::floor(position)))) assets.push_back({kind, position, facing, seed});
    };

    // Lantern fittings are anchored to actual light blocks, including edited
    // worlds. Existing lanterns remain the source of their warm illumination.
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        const auto origin = world.origin(index);
        const auto& chunk = world.chunk(index);
        for (int y = 0; y < Chunk::Size; ++y) {
            for (int z = 0; z < Chunk::Size; ++z) {
                for (int x = 0; x < Chunk::Size; ++x) {
                    if (chunk.get(x, y, z) != Block::Lantern) continue;
                    const auto cell = origin + glm::ivec3(x, y, z);
                    const auto center = glm::vec3(cell) + 0.5f;
                    if (!air(world, center + glm::vec3(0, 1, 0))) continue;
                    int wall = -1;
                    constexpr glm::ivec3 behind[4]{{0,0,-1},{-1,0,0},{0,0,1},{1,0,0}};
                    for (int facing = 0; facing < 4; ++facing)
                        if (isSolid(world.get(cell + behind[facing]))) { wall = facing; break; }
                    add(wall < 0 ? HubAssetKind::LanternCap : HubAssetKind::WallBracket,
                        center, std::max(0, wall), variation(cell.x, cell.z, world.seed()));
                }
            }
        }
    }

    // Architecture follows the same local axes as the four building styles.
    // Check real wall/floor anchors so replacing a world never leaves ghost props.
    std::size_t number = 0;
    for (const auto& building : sceneBuildingPlan()) {
        const auto tag = static_cast<std::uint32_t>(number++);
        const auto corner = glm::ivec3(glm::floor(buildingPoint(building, {0.5f, 1.5f, 0.5f})));
        const auto floor = glm::ivec3(glm::floor(buildingPoint(building, {2.5f, 0.5f, 2.5f})));
        if (world.get(corner) != Block::Timber || world.get(floor) != Block::Planks) continue;
        const float width = static_cast<float>(building.width), depth = static_cast<float>(building.depth);
        const bool hall = building.style == HubBuildingStyle::Hall;
        const bool workshop = building.style == HubBuildingStyle::Workshop;
        const bool storehouse = building.style == HubBuildingStyle::Storehouse;
        const float height = hall ? 7.0f : storehouse ? 5.0f : 4.0f;
        const auto detail = [&](HubAssetKind kind, glm::vec3 local, int facing = 0) {
            add(kind, buildingPoint(building, local), (building.facing + facing) % 4, tag);
        };
        const float window = storehouse ? 3.0f : 2.0f;
        detail(HubAssetKind::WindowBox, {-0.04f, window - 0.4f, 2.0f}, 3);
        detail(HubAssetKind::Shutters, {-0.06f, window, 2.0f}, 3);
        detail(HubAssetKind::WindowBox, {width + 0.04f, window - 0.4f, float(building.depth / 2 + 2)}, 1);
        detail(HubAssetKind::Shutters, {width + 0.06f, window, float(building.depth / 2 + 2)}, 1);
        for (const float side : {0.5f, width - 0.5f})
            detail(HubAssetKind::RoofBrace, {side, height + 0.75f, depth + 0.03f});
        if (hall || storehouse)
            detail(HubAssetKind::Banner, {0.8f, height + 0.3f, depth + 0.10f});
        if (workshop || (hall && tag % 2 == 0) || tag == 3) {
            detail(HubAssetKind::Awning, {width * 0.5f, workshop ? 4.95f : 5.2f, depth + 0.02f});
            // Glass stays above head height; chains meet the awning supports.
            for (const float side : {-1.65f, 1.65f}) {
                const auto point = buildingPoint(building, {width * 0.5f + side, 3.75f, depth + 0.65f});
                if (air(world, point)) add(HubAssetKind::HangingLamp, point, building.facing, tag);
            }
        }
        // Start just inside the roof underside so the stems meet the eave.
        const auto eave = buildingPoint(building, {width + 0.07f, height + 1.02f, depth - 0.5f});
        if (isSolid(world.get(glm::ivec3(glm::floor(eave)))))
            add(HubAssetKind::Vines, eave, (building.facing + 1) % 4, tag);
    }

    // Soft planting hugs grass borders; paved routes remain visually open.
    for (int z = -50; z <= 62; z += 2) {
        for (int x = -52; x <= 66; x += 2) {
            const auto seed = variation(x, z, world.seed());
            if (seed % 4 != 0 || world.get({x, 8, z}) != Block::Grass
                || world.get({x, 9, z}) != Block::Air || world.get({x, 10, z}) != Block::Air) continue;
            const float jitterX = float((seed >> 8) % 5) * 0.08f;
            const float jitterZ = float((seed >> 12) % 5) * 0.08f;
            const glm::vec3 position(x + 0.3f + jitterX, 9, z + 0.3f + jitterZ);
            bool shore = false;
            for (const auto side : {glm::ivec3(3,0,0), {-3,0,0}, {0,0,3}, {0,0,-3}})
                shore |= world.get(glm::ivec3(x, 6, z) + side) == Block::Water;
            const auto kind = shore ? HubAssetKind::Reeds
                : seed % 7 == 0 ? HubAssetKind::Mushrooms
                : seed % 3 == 0 ? HubAssetKind::Flowers
                : seed % 3 == 1 ? HubAssetKind::Fern : HubAssetKind::Shrub;
            add(kind, position, (seed >> 16) % 4, seed);
        }
    }
    for (int z = -19; z <= 32; z += 3) {
        for (int x = -36; x <= 47; x += 3) {
            const auto seed = variation(x, z, world.seed());
            if (seed % 5 != 0 || world.get({x, 6, z}) != Block::Water || world.get({x, 7, z}) != Block::Air) continue;
            // All four pad edges stay over water and clear of bridge supports.
            bool open = true;
            for (const auto side : {glm::ivec3(1,0,0), {-1,0,0}, {0,0,1}, {0,0,-1}})
                open &= world.get(glm::ivec3(x, 6, z) + side) == Block::Water
                     && world.get(glm::ivec3(x, 7, z) + side) == Block::Air;
            if (open) add(HubAssetKind::LilyPads, {x + 0.5f, 7.025f, z + 0.5f}, seed % 4, seed);
        }
    }

    // Fine trailing leaves replace the old solid hanging columns in the crown.
    for (int z = -16; z <= 34; z += 3) {
        for (int x = -18; x <= 33; x += 3) {
            const auto seed = variation(x, z, world.seed());
            if (seed % 3 != 0) continue;
            for (int y = 30; y <= 48; ++y) {
                const auto block = world.get({x, y, z});
                if (block != Block::Leaves && block != Block::GlowLeaves) continue;
                if (world.get({x, y-1, z}) == Block::Air && world.get({x, y-2, z}) == Block::Air
                    && world.get({x, y-3, z}) == Block::Air && world.get({x, y-4, z}) == Block::Air)
                    add(HubAssetKind::Vines, {x + 0.5f, float(y) + 0.02f, z + 0.5f}, seed % 4, seed);
                break;
            }
        }
    }
    for (const auto column : {glm::ivec2(-34,-20), {48,-18}, {-40,28}, {43,43}, {-17,51}}) {
        for (int offset = -2; offset <= 2; offset += 2) {
            const int x = column.x + offset, z = column.y;
            for (int y = 59; y >= 40; --y) {
                if (!isSolid(world.get({x,y,z})) || !air(world, {x + .5f, y - .5f, z + .5f})) continue;
                add(HubAssetKind::Vines, {x + .5f, float(y), z + .5f}, 0, variation(x,z,world.seed()));
                break;
            }
        }
    }
    for (const auto column : {glm::ivec2(1,6), {16,13}, {5,20}, {12,0}}) {
        for (int y = 20; y <= 36; ++y) {
            if (world.get({column.x,y,column.y}) != Block::Timber) continue;
            const glm::vec3 point(column.x + .5f, y - 1.5f, column.y + .5f);
            if (air(world,point) && air(world,point - glm::vec3(0,1,0)) && air(world,point + glm::vec3(0,1,0)))
                add(HubAssetKind::HangingLamp, point);
            break;
        }
    }
    for (const int x : {-1,17}) {
        if (world.get({x,29,-45}) == Block::RoofTiles)
            add(HubAssetKind::CrystalBeacon, {x+.5f,30,-44.5f});
    }
    for (const int x : {0,15}) {
        if (world.get({x,9,-37}) == Block::Timber)
            add(HubAssetKind::CrateStack, {x+.5f,11,-36.5f}, 0, static_cast<std::uint32_t>(x));
    }
    return assets;
}

MeshData buildHubAsset(const HubAsset& asset)
{
    MeshData mesh;
    if (asset.kind >= HubAssetKind::Fern) appendHubFoliage(mesh, asset);
    else appendHubProp(mesh, asset);
    return mesh;
}

HubDressing buildHubDressing(const World& world)
{
    HubDressing dressing;
    for (const auto& asset : planHubAssets(world)) {
        const auto local = glm::ivec3(glm::floor(asset.position)) - World::Min;
        const auto index = static_cast<std::size_t>(local.x / Chunk::Size
            + World::Width * (local.z / Chunk::Size + World::Width * (local.y / Chunk::Size)));
        auto mesh = buildHubAsset(asset);
        auto& destination = dressing.meshes[index];
        const auto start = static_cast<std::uint32_t>(destination.vertices.size());
        destination.vertices.insert(destination.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
        for (const auto vertex : mesh.indices) destination.indices.push_back(start + vertex);
        if (asset.kind == HubAssetKind::HangingLamp)
            dressing.lights.push_back({asset.position, {1.05f,0.57f,0.23f},8.0f});
        if (asset.kind == HubAssetKind::CrystalBeacon)
            dressing.lights.push_back({asset.position + glm::vec3(0,1.4f,0), {0.12f,0.66f,0.88f},7.0f});
    }
    return dressing;
}

} // namespace voxel
