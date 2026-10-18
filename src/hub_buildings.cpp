#include "hub_buildings.h"
#include "world.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace voxel {
namespace {

void validateShape(const HubBuilding& building)
{
    const auto validSize = [](int value) { return value >= 7 && value <= 11 && value % 2 == 1; };
    if (!validSize(building.width) || !validSize(building.depth)
        || building.facing < 0 || building.facing > 3
        || building.style < HubBuildingStyle::Cottage || building.style > HubBuildingStyle::Storehouse) {
        throw std::invalid_argument("Invalid village building shape");
    }
}

void validateBounds(const HubBuilding& building)
{
    const auto size = hubBuildingFootprint(building);
    if (building.origin.x <= World::Min.x || building.origin.x >= World::MaxExclusive.x - size.x
        || building.origin.z <= World::Min.z || building.origin.z >= World::MaxExclusive.z - size.y
        || building.origin.y < World::Min.y || building.origin.y >= World::MaxExclusive.y - 14) {
        throw std::out_of_range("Village building and roof must fit inside the world");
    }
}

glm::ivec3 worldCell(const HubBuilding& building, glm::ivec3 local)
{
    switch (building.facing) {
    case 1: return building.origin + glm::ivec3(local.z, local.y, building.width - 1 - local.x);
    case 2: return building.origin + glm::ivec3(building.width - 1 - local.x, local.y, building.depth - 1 - local.z);
    case 3: return building.origin + glm::ivec3(building.depth - 1 - local.z, local.y, local.x);
    default: return building.origin + local;
    }
}

// All construction below uses local coordinates, with inclusive box corners.
void box(World& world, const HubBuilding& building, glm::ivec3 from, glm::ivec3 to, Block block)
{
    for (int y = from.y; y <= to.y; ++y) {
        for (int z = from.z; z <= to.z; ++z) {
            for (int x = from.x; x <= to.x; ++x) world.set(worldCell(building, {x, y, z}), block);
        }
    }
}

void windows(World& world, const HubBuilding& building, int bottom, int top)
{
    const int width = building.width, depth = building.depth;
    // Two separate bays on each side leave the center timber post intact.
    for (const int z : {1, depth / 2 + 1}) {
        const int end = std::min(z + 1, depth - 2);
        box(world, building, {0, bottom, z}, {0, top, end}, Block::Air);
        box(world, building, {width - 1, bottom, z}, {width - 1, top, end}, Block::Air);
    }
    for (const int x : {1, width / 2 + 1}) {
        box(world, building, {x, bottom, 0}, {std::min(x + 1, width - 2), top, 0}, Block::Air);
    }
}

} // namespace

glm::ivec2 hubBuildingFootprint(const HubBuilding& building)
{
    validateShape(building);
    return building.facing % 2 == 0 ? glm::ivec2(building.width, building.depth)
                                   : glm::ivec2(building.depth, building.width);
}

glm::ivec3 hubBuildingDoor(const HubBuilding& building)
{
    validateBounds(building);
    return worldCell(building, {building.width / 2, 1, building.depth - 1});
}

void buildHubBuilding(World& world, const HubBuilding& building)
{
    validateBounds(building);
    const int width = building.width, depth = building.depth;
    const bool hall = building.style == HubBuildingStyle::Hall;
    const bool workshop = building.style == HubBuildingStyle::Workshop;
    const bool storehouse = building.style == HubBuildingStyle::Storehouse;
    const int wallHeight = hall ? 7 : (storehouse ? 5 : 4);
    const int roofBase = wallHeight + 1;

    // Clear the room before building its shell. Furniture never blocks the
    // usable floor: even the small cottage has a five-by-five interior.
    box(world, building, {0, 1, 0}, {width - 1, 14, depth - 1}, Block::Air);
    box(world, building, {0, 0, 0}, {width - 1, 0, depth - 1}, Block::Limestone);
    box(world, building, {1, 0, 1}, {width - 2, 0, depth - 2}, Block::Planks);
    box(world, building, {0, 1, 0}, {width - 1, wallHeight, depth - 1}, hall ? Block::Limestone : Block::Planks);
    box(world, building, {0, 1, 0}, {width - 1, 1, depth - 1}, workshop ? Block::Brick : Block::Slate);
    box(world, building, {1, 1, 1}, {width - 2, wallHeight, depth - 2}, Block::Air);

    for (const int x : {0, width / 2, width - 1}) {
        for (const int z : {0, depth - 1}) {
            box(world, building, {x, 1, z}, {x, wallHeight, z}, Block::Timber);
        }
    }
    for (const int x : {0, width - 1}) {
        box(world, building, {x, 1, depth / 2}, {x, wallHeight, depth / 2}, Block::Timber);
        box(world, building, {x, wallHeight, 0}, {x, wallHeight, depth - 1}, Block::Timber);
    }
    for (const int z : {0, depth - 1}) {
        box(world, building, {0, wallHeight, z}, {width - 1, wallHeight, z}, Block::Timber);
        if (hall) box(world, building, {0, 4, z}, {width - 1, 4, z}, Block::Timber);
    }

    windows(world, building, storehouse ? 3 : 2, 3);
    if (hall) windows(world, building, 5, 6);
    const int doorWidth = workshop ? width - 2 : (hall || storehouse ? 3 : 2);
    const int doorLeft = (width - doorWidth) / 2;
    const int doorRight = doorLeft + doorWidth - 1;
    for (const int x : {doorLeft - 1, doorRight + 1}) {
        box(world, building, {x, 1, depth - 1}, {x, 4, depth - 1}, Block::Timber);
    }
    box(world, building, {doorLeft - 1, 4, depth - 1}, {doorRight + 1, 4, depth - 1}, Block::Timber);
    box(world, building, {doorLeft, 1, depth - 1}, {doorRight, 3, depth}, Block::Air);
    box(world, building, {doorLeft, 0, depth}, {doorRight, 0, depth}, Block::Limestone);
    if (!workshop) {
        box(world, building, {1, 2, depth - 1}, {doorLeft - 2, 3, depth - 1}, Block::Air);
        box(world, building, {doorRight + 2, 2, depth - 1}, {width - 2, 3, depth - 1}, Block::Air);
    }

    // The workshop turns its gable sideways, showing a wide open work bay.
    // Other buildings present a tall gable to the street. Hall proportions
    // supply a larger landmark without adding an inaccessible second floor.
    const int acrossSize = workshop ? depth : width;
    const int alongSize = workshop ? width : depth;
    const auto roofCell = [workshop](int across, int y, int along) {
        return workshop ? glm::ivec3(along, y, across) : glm::ivec3(across, y, along);
    };
    for (int across = -1; across <= acrossSize; ++across) {
        const int height = roofBase + std::max(0, acrossSize / 2 - std::abs(across - acrossSize / 2));
        for (int along = -1; along <= alongSize; ++along) {
            if (across >= 0 && across < acrossSize && (along == 0 || along == alongSize - 1)) {
                box(world, building, roofCell(across, roofBase, along), roofCell(across, height - 1, along),
                    across == acrossSize / 2 ? Block::Timber : Block::Planks);
            }
            const bool trim = along == -1 || along == alongSize || across == -1 || across == acrossSize;
            const auto cell = roofCell(across, height, along);
            box(world, building, cell, cell, trim ? Block::Timber : Block::RoofTiles);
        }
    }
    const int peak = roofBase + acrossSize / 2;
    for (const int along : {-1, alongSize}) {
        const auto cap = roofCell(acrossSize / 2, peak + 1, along);
        box(world, building, cap, cap, Block::Timber);
    }
    if (acrossSize >= 9) {
        for (const int along : {0, alongSize - 1}) {
            box(world, building, roofCell(acrossSize / 2 - 1, roofBase + 1, along),
                roofCell(acrossSize / 2, roofBase + 1, along), Block::Air);
        }
    }

    // A warm doorway and one suspended interior light make entrances readable.
    const int lampX = workshop ? width - 1 : doorRight + 1;
    box(world, building, {lampX, 3, depth - 1}, {lampX, 3, depth - 1}, Block::Lantern);
    if (hall) box(world, building, {doorLeft - 1, 3, depth - 1}, {doorLeft - 1, 3, depth - 1}, Block::Lantern);
    box(world, building, {width / 2, wallHeight + 1, depth / 2}, {width / 2, peak - 1, depth / 2}, Block::Timber);
    box(world, building, {width / 2, wallHeight, depth / 2}, {width / 2, wallHeight, depth / 2}, Block::Lantern);

    if (building.style == HubBuildingStyle::Cottage || workshop) {
        const int chimneyTop = roofBase + (workshop ? 4 : 3);
        box(world, building, {width - 2, wallHeight, 1}, {width - 2, chimneyTop, 1}, Block::Brick);
        box(world, building, {width - 2, chimneyTop + 1, 1}, {width - 2, chimneyTop + 1, 1}, Block::Slate);
    }
}

} // namespace voxel
