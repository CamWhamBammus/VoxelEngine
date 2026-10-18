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

bool soil(Block block)
{
    return block == Block::Grass || block == Block::Dirt;
}

// Check the whole model footprint, not just its origin. This keeps wide leaves
// on their bank and prevents a model from bridging air above a lower basin.
bool plantedFootprint(const World& world, glm::vec3 position, float radius, bool water = false)
{
    const auto low = glm::ivec3(glm::floor(position - glm::vec3(radius, 0, radius)));
    const auto high = glm::ivec3(glm::floor(position + glm::vec3(radius, 0, radius)));
    for (int z = low.z; z <= high.z; ++z) {
        for (int x = low.x; x <= high.x; ++x) {
            const auto support = world.get({x, low.y - 1, z});
            if (water ? support != Block::Water : !soil(support)) return false;
            for (int y = low.y; y <= low.y + 1; ++y)
                if (!World::contains({x,y,z}) || world.get({x,y,z}) != Block::Air) return false;
        }
    }
    return true;
}

bool hangingClearance(const World& world, glm::ivec3 anchor)
{
    // Vines descend at most 3.3 blocks and spread less than 0.6 from their center.
    for (int y = anchor.y - 4; y < anchor.y; ++y)
        for (int z = anchor.z - 1; z <= anchor.z + 1; ++z)
            for (int x = anchor.x - 1; x <= anchor.x + 1; ++x)
                if (!World::contains({x,y,z}) || world.get({x,y,z}) != Block::Air) return false;
    return true;
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

    // Building profiles supply dimensions; real openings and supports decide
    // which fittings can be attached to each architectural style.
    std::size_t number = 0;
    for (const auto& building : sceneBuildingPlan()) {
        const auto tag = static_cast<std::uint32_t>(number++);
        const auto seed = variation(building.origin.x, building.origin.z, world.seed());
        const auto solid = [&](glm::vec3 local) {
            return isSolid(world.get(glm::ivec3(glm::floor(buildingPoint(building, local)))));
        };
        if (!solid({0.5f, 1.5f, 0.5f}) || !solid({2.5f, 0.5f, 2.5f})) continue;
        const float width = static_cast<float>(building.width), depth = static_cast<float>(building.depth);
        const float height = static_cast<float>(hubBuildingWallHeight(building));
        const auto detail = [&](HubAssetKind kind, glm::vec3 local, int facing = 0) {
            add(kind, buildingPoint(building, local), (building.facing + facing) % 4, seed);
        };
        const bool tower = building.style == HubBuildingStyle::Tower;
        const bool greenhouse = building.style == HubBuildingStyle::Greenhouse;
        const bool storehouse = building.style == HubBuildingStyle::Storehouse;
        const bool workshop = building.style == HubBuildingStyle::Workshop;
        const bool hall = building.style == HubBuildingStyle::Hall;
        const bool inn = building.style == HubBuildingStyle::Inn;
        if (!tower && !greenhouse) {
            const float bottom = storehouse ? 3.0f : 2.0f;
            for (const int side : {0, 1}) {
                const float wallX = side ? width - 0.5f : 0.5f;
                for (int along = 1; along + 1 < building.depth - 1; along += 4) {
                    if ((along / 4 + tag + side) % 2 != 0) continue;
                    bool opening = true;
                    for (int y = 0; y < 2; ++y)
                        for (int z = 0; z < 2; ++z)
                            opening &= air(world, buildingPoint(building, {wallX, bottom + y + 0.5f, along + z + 0.5f}));
                    if (!opening) continue;
                    const float center = along + 1.0f;
                    const float surface = side ? width - 0.02f : 0.02f;
                    const int facing = side ? 1 : 3;
                    if (solid({wallX, bottom - 0.5f, center - 0.5f})
                        && solid({wallX, bottom - 0.5f, center + 0.5f}))
                        detail(HubAssetKind::WindowBox, {surface, bottom - 0.02f, center}, facing);
                    if (solid({wallX, bottom + 0.5f, center - 1.35f})
                        && solid({wallX, bottom + 0.5f, center + 1.35f}))
                        detail(HubAssetKind::Shutters, {surface, bottom, center}, facing);
                }
            }
            for (const float side : {0.5f, width - 0.5f}) {
                if (solid({side, height + 1.02f, depth + 0.5f}) && solid({side, height - 0.5f, depth - 0.5f}))
                    detail(HubAssetKind::RoofBrace, {side, height + 0.92f, depth - 0.06f});
            }
        }
        if ((hall || storehouse || inn || tower) && solid({0.8f, height - 0.3f, depth - 0.5f}))
            detail(HubAssetKind::Banner, {0.8f, height - 0.4f, depth - 0.04f});

        if (workshop || (storehouse && tag % 2 == 0)
            || (building.style == HubBuildingStyle::Cottage && tag % 7 == 0)) {
            for (int row = 4; row < static_cast<int>(height); ++row) {
                bool supported = true;
                for (const float side : {-2.35f, 2.35f})
                    supported &= solid({width * 0.5f + side, row + 0.2f, depth - 0.5f})
                              && solid({width * 0.5f + side, row + 0.8f, depth - 0.5f});
                if (!supported) continue;
                const float awningY = row + 1.0f;
                detail(HubAssetKind::Awning, {width * 0.5f, awningY, depth - 0.04f});
                for (const float side : {-2.35f, 2.35f}) {
                    // Chain tops meet the wooden awning braces, and glass stays
                    // above head height rather than hanging across a doorway.
                    const auto point = buildingPoint(building, {width * 0.5f + side, awningY - 1.77f, depth + 0.61f});
                    if (air(world, point) && air(world, point - glm::vec3(0, 0.44f, 0))
                        && air(world, point + glm::vec3(0, 1.5f, 0)))
                        add(HubAssetKind::HangingLamp, point, building.facing, seed);
                }
                break;
            }
        }
        if (!tower && !greenhouse) {
            // Half-cell Z avoids choosing the wrong roof cell after rotation.
            const auto eave = buildingPoint(building, {width + 0.07f, height + 1.02f, depth - 0.5f});
            if (isSolid(world.get(glm::ivec3(glm::floor(eave)))))
                add(HubAssetKind::Vines, eave, (building.facing + 1) % 4, seed);
        }
        if (storehouse) {
            const auto point = buildingPoint(building, {width + 2.5f, 1.0f, depth - 2.5f});
            if (plantedFootprint(world, point, 0.75f))
                add(HubAssetKind::CrateStack, point, building.facing, seed);
        }
    }

    // Use actual terrain heights throughout the cavern. Seeded spacing leaves
    // readable groups of plants without adding a model to every grass block.
    for (int z = World::Min.z + 3; z < World::MaxExclusive.z - 3; z += 2) {
        for (int x = World::Min.x + 3; x < World::MaxExclusive.x - 3; x += 2) {
            const auto seed = variation(x, z, world.seed());
            if (seed % 4 != 0) continue;
            for (int y = World::MaxExclusive.y - 3; y >= 8; --y) {
                if (!soil(world.get({x,y,z}))) continue;
                const float jitterX = float((seed >> 8) % 5) * 0.06f;
                const float jitterZ = float((seed >> 12) % 5) * 0.06f;
                const glm::vec3 position(x + 0.38f + jitterX, y + 1.0f, z + 0.38f + jitterZ);
                if (!plantedFootprint(world, position, 0.9f)) continue;
                bool shore = false;
                for (const auto side : {glm::ivec3(4,0,0), {-4,0,0}, {0,0,4}, {0,0,-4}})
                    for (int depth = 0; depth <= 3; ++depth)
                        shore |= world.get(glm::ivec3(x,y-depth,z) + side) == Block::Water;
                const auto kind = shore ? HubAssetKind::Reeds
                    : seed % 7 == 0 ? HubAssetKind::Mushrooms
                    : (seed >> 4) % 3 == 0 ? HubAssetKind::Flowers
                    : (seed >> 4) % 3 == 1 ? HubAssetKind::Fern : HubAssetKind::Shrub;
                add(kind, position, (seed >> 16) % 4, seed);
                break;
            }
        }
    }
    for (int z = World::Min.z + 3; z < World::MaxExclusive.z - 3; z += 5) {
        for (int x = World::Min.x + 3; x < World::MaxExclusive.x - 3; x += 5) {
            const auto seed = variation(x, z, world.seed());
            if (seed % 3 != 0) continue;
            for (int y = World::MaxExclusive.y - 3; y >= 1; --y) {
                if (world.get({x,y,z}) != Block::Water) continue;
                const glm::vec3 position(x + 0.5f, y + 1.0f, z + 0.5f);
                // A broad, level surface excludes waterfalls and narrow channels.
                if (!plantedFootprint(world, position, 1.5f, true)) continue;
                add(HubAssetKind::LilyPads, position, seed % 4, seed);
                break;
            }
        }
    }

    // Follow real canopy and cavern undersides rather than fixed height lists.
    for (int z = World::Min.z + 3; z < World::MaxExclusive.z - 3; z += 6) {
        for (int x = World::Min.x + 3; x < World::MaxExclusive.x - 3; x += 6) {
            const auto seed = variation(x, z, world.seed());
            if (seed % 3 != 0) continue;
            for (int y = World::MaxExclusive.y - 2; y >= 16; --y) {
                const auto block = world.get({x, y, z});
                const bool canopy = block == Block::Leaves || block == Block::GlowLeaves;
                const bool ceiling = y > World::MaxExclusive.y / 2 && seed % 11 == 0
                    && (block == Block::Stone || block == Block::Slate || block == Block::Limestone);
                if ((!canopy && !ceiling) || !hangingClearance(world, {x,y,z})) continue;
                add(HubAssetKind::Vines, {x + 0.5f, y + 0.02f, z + 0.5f}, seed % 4, seed);
                break;
            }
        }
    }
    for (const auto position : ExpeditionBeacons) {
        const auto base = glm::ivec3(glm::floor(position));
        bool supported = true;
        for (int z = base.z - 1; z <= base.z + 1; ++z) {
            for (int x = base.x - 1; x <= base.x + 1; ++x) {
                supported &= isSolid(world.get({x,base.y - 1,z}));
                for (int y = base.y; y <= base.y + 2; ++y)
                    supported &= World::contains({x,y,z}) && world.get({x,y,z}) == Block::Air;
            }
        }
        if (supported) add(HubAssetKind::CrystalBeacon, position);
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
