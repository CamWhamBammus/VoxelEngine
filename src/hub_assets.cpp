#include "hub_assets.h"
#include "hub_lush.h"
#include "scene.h"
#include "graphics/asset_builder.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
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
    return block == Block::Grass || block == Block::Dirt || block == Block::RootedDirt || block == Block::Moss;
}

// Check the whole model footprint, not just its origin. This keeps wide leaves
// on their bank and prevents a model from bridging air above a lower basin.
bool plantedFootprint(const World& world, glm::vec3 position, float radius, bool water = false, int height = 2)
{
    const auto low = glm::ivec3(glm::floor(position - glm::vec3(radius, 0, radius)));
    const auto high = glm::ivec3(glm::floor(position + glm::vec3(radius, 0, radius)));
    for (int z = low.z; z <= high.z; ++z) {
        for (int x = low.x; x <= high.x; ++x) {
            const auto support = world.get({x, low.y - 1, z});
            if (water ? support != Block::Water : !soil(support)) return false;
            for (int y = low.y; y < low.y + height; ++y)
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

bool retainingFace(const World& world, glm::ivec3 anchor, glm::ivec3 outward)
{
    const glm::ivec3 across(outward.z, 0, -outward.x);
    for (int drop = 1; drop <= 4; ++drop)
        if (!isSolid(world.get(anchor - glm::ivec3(0,drop,0)))) return false;
    // The lowest leaves stay above walking headroom. The full width must be
    // clear, so ivy cannot spread over stairs, bridge rails, or nearby walls.
    for (int drop = 0; drop <= 5; ++drop) {
        for (int side = -1; side <= 1; ++side) {
            const auto cell = anchor + outward + across * side - glm::ivec3(0,drop,0);
            if (!World::contains(cell) || world.get(cell) != Block::Air) return false;
        }
    }
    return true;
}

bool vineFits(const World& world, const HubAsset& asset)
{
    // An eave may border a higher terrace. Test the small model itself so its
    // leaves cannot disappear into the hill; only the stem root embeds.
    for (const auto& vertex : buildHubAsset(asset).vertices) {
        const auto offset = vertex.position - asset.position;
        if (glm::dot(offset, offset) > 0.0036f && !air(world, vertex.position)) return false;
    }
    return true;
}

bool facadeFits(const World& world, const HubAsset& asset, float inset = 0.07f)
{
    constexpr glm::vec3 outward[4]{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
    for (const auto& vertex : buildHubAsset(asset).vertices) {
        if (!World::contains(glm::ivec3(glm::floor(vertex.position)))) return false;
        // Brackets meet the wall; projecting faces must have room outside it.
        if (glm::dot(vertex.position - asset.position, outward[asset.facing]) > inset
            && !air(world, vertex.position)) return false;
    }
    return true;
}

} // namespace

std::vector<HubAsset> planHubAssets(const World& world)
{
    std::vector<HubAsset> assets;
    std::vector<HubAsset> planting;
    std::vector<HubAsset> paths;
    const auto add = [&](HubAssetKind kind, glm::vec3 position, int facing = 0, std::uint32_t seed = 0) {
        if (World::contains(glm::ivec3(glm::floor(position)))) assets.push_back({kind, position, facing, seed});
    };
    const auto plant = [&](HubAssetKind kind, glm::vec3 position, int facing, std::uint32_t seed) {
        if (World::contains(glm::ivec3(glm::floor(position)))) planting.push_back({kind, position, facing, seed});
    };

    // Lantern fittings are anchored to actual light blocks, including edited
    // worlds. Existing lanterns remain the source of their warm illumination.
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        const auto origin = world.origin(index);
        const auto& chunk = world.chunk(index);
        for (int y = 0; y < Chunk::Size; ++y) {
            for (int z = 0; z < Chunk::Size; ++z) {
                for (int x = 0; x < Chunk::Size; ++x) {
                    if (chunk.get(x, y, z) == Block::Path) {
                        const auto cell = origin + glm::ivec3(x,y,z);
                        const auto top = glm::vec3(cell) + glm::vec3(0.5f,1.0f,0.5f);
                        if (!air(world, top)) continue;
                        const auto seed = variation(cell.x, cell.z, world.seed());
                        paths.push_back({HubAssetKind::PathPaving, top, int(seed % 4), seed});
                        constexpr glm::ivec3 sides[4]{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
                        for (int side = 0; side < 4; ++side) {
                            const auto next = cell + sides[side];
                            const auto neighbor = world.get(next);
                            // A tread cap faces downhill, never into an adjoining
                            // tread or doorway. Models stay attached to this tile.
                            if (neighbor == Block::Air && world.get(next - glm::ivec3(0,1,0)) == Block::Path) {
                                paths.push_back({HubAssetKind::PathStep, top, side, seed});
                            } else if ((neighbor == Block::Air || neighbor == Block::Water || soil(neighbor))
                                       && world.get(next + glm::ivec3(0,1,0)) == Block::Air) {
                                paths.push_back({HubAssetKind::PathEdging, top, side, seed});
                            }
                        }
                        continue;
                    }
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
        const auto fitting = [&](HubAssetKind kind, glm::vec3 local, int facing = 0, float inset = 0.07f) {
            const HubAsset asset{kind, buildingPoint(building, local), (building.facing + facing) % 4, seed};
            if (!facadeFits(world, asset, inset)) return false;
            assets.push_back(asset);
            return true;
        };
        const bool tower = building.style == HubBuildingStyle::Tower;
        const bool greenhouse = building.style == HubBuildingStyle::Greenhouse;
        const bool storehouse = building.style == HubBuildingStyle::Storehouse;
        const bool workshop = building.style == HubBuildingStyle::Workshop;
        const bool hall = building.style == HubBuildingStyle::Hall;
        const bool inn = building.style == HubBuildingStyle::Inn;
        const bool cottage = building.style == HubBuildingStyle::Cottage;
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
                    // A few projecting bays replace the flat window dressing.
                    // Their lower corbels land on the existing window sill.
                    if ((cottage || inn) && (along / 4 + tag) % 3 == 0
                        && solid({wallX, bottom - 0.5f, center - 0.5f})
                        && solid({wallX, bottom - 0.5f, center + 0.5f})
                        && fitting(HubAssetKind::BayWindow, {surface, bottom - 0.5f, center}, facing)) continue;
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
            || (cottage && building.variant % 3 != 1 && tag % 7 == 0)) {
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
        if ((cottage && building.variant % 3 == 1) || (storehouse && tag % 2 != 0)) {
            const float canopyY = cottage ? 5.0f : 7.2f;
            if (solid({width * 0.5f - 1.7f, canopyY - 0.4f, depth - 0.5f})
                && solid({width * 0.5f + 1.7f, canopyY - 0.4f, depth - 0.5f}))
                fitting(HubAssetKind::PorchCanopy, {width * 0.5f, canopyY, depth - 0.01f});
        }
        if (inn) {
            float previous = -10.0f;
            for (int start = 1; start + 2 < building.width; start += 4) {
                const float center = start + 1.5f;
                if (center - previous < 8.0f
                    || !air(world, buildingPoint(building, {center, 6.5f, depth - 0.5f}))
                    || !air(world, buildingPoint(building, {center, 7.5f, depth - 0.5f}))) continue;
                if (solid({center - 1.3f, 5.5f, depth - 0.5f})
                    && solid({center + 1.3f, 5.5f, depth - 0.5f}))
                    if (fitting(HubAssetKind::Balcony, {center, 6.0f, depth - 0.01f})) previous = center;
            }
        }
        if (workshop || (cottage && tag % 3 == 0)) {
            const float cap = workshop ? hubBuildingHeight(building) : height + 6.0f;
            const glm::vec3 chimney(workshop ? width - 2.0f : width - 1.5f, cap + 1.0f, workshop ? 1.0f : 1.5f);
            if (solid(chimney - glm::vec3(0,0.5f,0))) {
                const HubAsset asset{HubAssetKind::Chimney, buildingPoint(building, chimney), building.facing, seed};
                bool clear = true;
                for (const auto& vertex : buildHubAsset(asset).vertices)
                    clear &= air(world, vertex.position + glm::vec3(0,0.001f,0));
                if (clear) assets.push_back(asset);
            }
        }
        if (workshop || (cottage && building.variant % 3 == 1)) {
            // The front rests on the lowest roof course; the rear embeds in
            // the rising roof, so a dormer never floats above its slope.
            const glm::vec3 dormer(3.5f, height + 2.0f, depth - 1.35f);
            if (solid(dormer + glm::vec3(-0.9f,-0.1f,0.8f))
                && solid(dormer + glm::vec3(0.9f,-0.1f,0.8f)))
                fitting(HubAssetKind::Dormer, dormer, 0, 0.65f);
        }
        if (!tower && !greenhouse) {
            // Half-cell Z avoids choosing the wrong roof cell after rotation.
            const auto eave = buildingPoint(building, {width + 0.07f, height + 1.02f, depth - 0.5f});
            if (isSolid(world.get(glm::ivec3(glm::floor(eave))))
                && vineFits(world, {HubAssetKind::Vines, eave, (building.facing + 1) % 4, seed}))
                add(HubAssetKind::Vines, eave, (building.facing + 1) % 4, seed);
        }
        if (storehouse) {
            const auto point = buildingPoint(building, {width + 2.5f, 1.0f, depth - 2.5f});
            if (plantedFootprint(world, point, 0.75f))
                add(HubAssetKind::CrateStack, point, building.facing, seed);
        }
    }

    // The blacksmith is a real room in the arrival district. Each fixture
    // depends on its own voxel support, so editor removal cannot leave floating
    // tools or an orphaned forge light behind.
    const auto smith = sceneBlacksmithLayout();
    const auto smithSeed = variation(int(smith.anvil.x), int(smith.anvil.z), world.seed());
    const auto supported = [&](glm::vec3 center, int halfWidth, int halfDepth) {
        const auto cell = glm::ivec3(glm::floor(center));
        for (int z=-halfDepth;z<=halfDepth;++z) for (int x=-halfWidth;x<=halfWidth;++x) {
            if (!isSolid(world.get(cell+glm::ivec3(x,0,z)))
                || !isSolid(world.get(cell+glm::ivec3(x,-1,z)))) return false;
        }
        return true;
    };
    if (supported(smith.anvil,0,0) && air(world,smith.anvil+glm::vec3(0,1.2f,0)))
        add(HubAssetKind::Anvil,smith.anvil,0,smithSeed);
    if (supported(smith.forge,1,1) && air(world,smith.forge+glm::vec3(0,1.5f,0.6f)))
        add(HubAssetKind::Forge,smith.forge,0,smithSeed);
    if (supported(smith.workbench,2,0) && air(world,smith.workbench+glm::vec3(0,1.3f,0)))
        add(HubAssetKind::SmithWorkbench,smith.workbench,0,smithSeed);
    for (const auto& fixture : {HubAsset{HubAssetKind::ToolRack,smith.toolRack,0,smithSeed},
                               HubAsset{HubAssetKind::AnvilSign,smith.sign,0,smithSeed}}) {
        const auto support=glm::ivec3(glm::floor(fixture.position-glm::vec3(0,0,0.15f)));
        if (isSolid(world.get(support)) && facadeFits(world,fixture)) assets.push_back(fixture);
    }

    // A few complementary plants share each irregular garden pocket. Empty
    // space between pockets keeps the paths and larger terrain shapes legible.
    using Kind = HubAssetKind;
    constexpr Kind gardens[4][5]{
        {Kind::Shrub, Kind::Fern, Kind::Fern, Kind::Mushrooms, Kind::Fern},
        {Kind::Flowers, Kind::Flowers, Kind::Fern, Kind::Flowers, Kind::Shrub},
        {Kind::Mushrooms, Kind::Fern, Kind::Mushrooms, Kind::Shrub, Kind::Fern},
        {Kind::Shrub, Kind::Flowers, Kind::Fern, Kind::Flowers, Kind::Shrub},
    };
    constexpr glm::vec2 offsets[5]{{0,0}, {1.8f,0.4f}, {-1.3f,1.7f}, {0.6f,-1.8f}, {-1.7f,-1.4f}};
    for (int z = World::Min.z + 6; z < World::MaxExclusive.z - 6; z += 4) {
        const bool elevatedPocket = ((z - World::Min.z - 6) / 4) % 2 != 0;
        for (int x = World::Min.x + 6 + (elevatedPocket ? 4 : 0); x < World::MaxExclusive.x - 6; x += 8) {
            const auto patch = variation(x, z, world.seed());
            const float turn = float((patch >> 8) % 1024) * 0.006135923f;
            const glm::vec2 along(std::cos(turn), std::sin(turn)), across(-along.y, along.x);
            const glm::vec2 center(x + 0.5f + float(int((patch >> 18) % 7) - 3) * 0.25f,
                                   z + 0.5f + float(int((patch >> 22) % 7) - 3) * 0.25f);
            const int count = 3 + int((patch >> 4) % 3);
            for (int slot = 0; slot < count; ++slot) {
                const auto spot = center + along * offsets[slot].x + across * offsets[slot].y;
                const int columnX = int(std::floor(spot.x)), columnZ = int(std::floor(spot.y));
                const auto seed = variation(columnX, columnZ, patch);
                for (int y = World::MaxExclusive.y - 3; y >= (elevatedPocket ? 12 : 8); --y) {
                    if (!soil(world.get({columnX,y,columnZ}))) continue;
                    const glm::vec3 position(spot.x, y + 1.0f, spot.y);
                    // Extra pockets fill broad upper terraces; narrow banks
                    // and stair shoulders keep their open edges.
                    if (!plantedFootprint(world, position, elevatedPocket ? 1.25f : 0.9f)) continue;
                    bool shore = false;
                    for (const auto side : {glm::ivec3(4,0,0), {-4,0,0}, {0,0,4}, {0,0,-4}})
                        for (int depth = 0; depth <= 3; ++depth)
                            shore |= world.get(glm::ivec3(columnX,y-depth,columnZ) + side) == Block::Water;
                    auto kind = shore && slot % 2 == 0 ? Kind::Reeds : gardens[patch % 4][slot];
                    if (!shore && slot == 0 && plantedFootprint(world, position, 1.35f, false, 3)) {
                        constexpr Kind dense[]{Kind::BroadleafClump, Kind::FernBed, Kind::MossGarden};
                        kind = dense[(patch >> 12) % 3];
                    }
                    // Nearby blossoms share a palette, while their shapes vary.
                    const auto modelSeed = seed - seed % 3 + patch % 3;
                    plant(kind, position, (seed >> 16) % 4, modelSeed);
                    break;
                }
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
                plant(HubAssetKind::LilyPads, position, seed % 4, seed);
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
                plant(HubAssetKind::Vines, {x + 0.5f, y + 0.02f, z + 0.5f}, seed % 4, seed);
                break;
            }
        }
    }
    constexpr glm::ivec3 faces[4]{{0,0,1}, {1,0,0}, {0,0,-1}, {-1,0,0}};
    for (int z = World::Min.z + 4; z < World::MaxExclusive.z - 4; z += 4) {
        for (int x = World::Min.x + 4; x < World::MaxExclusive.x - 4; x += 4) {
            const auto seed = variation(x, z, world.seed());
            if (seed % 3 != 0) continue;
            for (int y = World::MaxExclusive.y - 3; y >= 13; --y) {
                const glm::ivec3 anchor(x,y,z);
                if (!soil(world.get(anchor)) || world.get(anchor + glm::ivec3(0,1,0)) != Block::Air) continue;
                for (int side = 0; side < 4; ++side) {
                    const int facing = (side + int(seed % 4)) % 4;
                    if (!retainingFace(world, anchor, faces[facing])) continue;
                    const auto position = glm::vec3(anchor) + glm::vec3(0.5f,0.75f,0.5f)
                        + glm::vec3(faces[facing]) * 0.49f;
                    plant(HubAssetKind::Vines, position, facing, seed);
                    break;
                }
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
    // If a custom world has unusually much plantable land, choose plants
    // throughout it instead of spending the budget on the first scanned edge.
    const std::size_t budget = std::min<std::size_t>(900, assets.size() < 1500 ? 1500 - assets.size() : 0);
    if (planting.size() > budget) {
        std::stable_sort(planting.begin(), planting.end(), [](const HubAsset& a, const HubAsset& b) {
            return variation(int(std::floor(a.position.x)), int(std::floor(a.position.z)), a.variation)
                 < variation(int(std::floor(b.position.x)), int(std::floor(b.position.z)), b.variation);
        });
        planting.resize(budget);
    }
    const auto landscape = planLandscapeAssets(world);
    // Reserve space for the larger silhouettes before scattering small plants.
    for (const auto& feature : landscape) {
        glm::vec3 low = feature.position, high = feature.position;
        for (const auto& vertex : buildHubAsset(feature).vertices) {
            low = glm::min(low, vertex.position); high = glm::max(high, vertex.position);
        }
        std::erase_if(planting, [&](const HubAsset& plant) {
            const auto p = plant.position;
            return p.x + 1.35f > low.x && p.x - 1.35f < high.x
                && p.z + 1.35f > low.z && p.z - 1.35f < high.z
                && p.y + 2.3f > low.y && p.y - (plant.kind == Kind::Vines ? 4.0f : 0.0f) < high.y;
        });
    }
    assets.insert(assets.end(), planting.begin(), planting.end());
    assets.insert(assets.end(), landscape.begin(), landscape.end());
    // Paving is part of the street surface, so its coverage does not compete
    // with the decorative plant budget. Each exposed Path tile owns its kit.
    assets.insert(assets.end(), paths.begin(), paths.end());
    blendLushAssets(world, assets);
    return assets;
}

MeshData buildHubAsset(const HubAsset& asset)
{
    MeshData mesh;
    if (isLushAsset(asset.kind)) appendHubLush(mesh, asset);
    else if (isBlacksmithAsset(asset.kind)) appendHubBlacksmith(mesh, asset);
    else if (asset.kind >= HubAssetKind::Dormer) appendHubArchitecture(mesh, asset);
    else if (asset.kind >= HubAssetKind::BroadleafClump) appendHubGarden(mesh, asset);
    else if (asset.kind >= HubAssetKind::Boulder) appendHubNature(mesh, asset);
    else if (isPathAsset(asset.kind)) appendHubPath(mesh, asset);
    else if (asset.kind >= HubAssetKind::Fern) appendHubFoliage(mesh, asset);
    else appendHubProp(mesh, asset);
    return mesh;
}

HubDressing buildHubDressing(const World& world)
{
    return buildAssetDressing(planHubAssets(world), false);
}

HubDressing buildAssetDressing(std::span<const HubAsset> assets, bool standaloneLights)
{
    HubDressing dressing;
    for (const auto& asset : assets) {
        if (!World::contains(glm::ivec3(glm::floor(asset.position)))) continue;
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
        if (standaloneLights && (asset.kind == HubAssetKind::LanternCap || asset.kind == HubAssetKind::WallBracket))
            dressing.lights.push_back({asset.position + glm::vec3(0,0.2f,0), {1.05f,0.57f,0.23f},9.0f});
        if (asset.kind == HubAssetKind::CrystalBeacon)
            dressing.lights.push_back({asset.position + glm::vec3(0,1.4f,0), {0.12f,0.66f,0.88f},7.0f});
        if (asset.kind == HubAssetKind::Forge)
            dressing.lights.push_back({asset.position + glm::vec3(0,1.55f,0.45f), {1.35f,0.43f,0.10f},8.0f});
        if (asset.kind == HubAssetKind::GlowMushroomColony)
            dressing.lights.push_back({asset.position + glm::vec3(0,0.55f,0), {0.12f,0.65f,0.64f},5.0f});
        if (asset.kind == HubAssetKind::GiantMushroom)
            dressing.lights.push_back({asset.position + glm::vec3(0,2.55f,0), {0.13f,0.57f,0.46f},6.5f});
        if (asset.kind == HubAssetKind::CrystalFlower)
            dressing.lights.push_back({asset.position + glm::vec3(0,0.8f,0), {0.84f,0.48f,0.13f},5.0f});
        if (asset.kind == HubAssetKind::SporeStalks)
            dressing.lights.push_back({asset.position + glm::vec3(0,1.5f,0), {0.19f,0.48f,0.68f},5.0f});
    }
    return dressing;
}

} // namespace voxel
