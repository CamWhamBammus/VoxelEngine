#include "hub_buildings.h"
#include "world.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace voxel {
namespace {
using Style = HubBuildingStyle;

void validateShape(const HubBuilding& building)
{
    const auto validSize = [](int value) { return value >= 7 && value <= 25 && value % 2 == 1; };
    if (!validSize(building.width) || !validSize(building.depth)
        || building.facing < 0 || building.facing > 3 || building.variant < 0
        || building.style < Style::Cottage || building.style > Style::Greenhouse) {
        throw std::invalid_argument("Invalid village building shape");
    }
}

bool sidewaysRoof(const HubBuilding& building)
{
    return building.style == Style::Workshop || building.style == Style::Inn
        || (building.style == Style::Cottage && building.variant % 3 == 1);
}

int hallAisleWidth(const HubBuilding& building)
{
    return building.style == Style::Hall && building.width >= 15 ? 3 : 0;
}

int roofRise(const HubBuilding& building, int distanceFromEdge)
{
    const int distance = std::max(0, distanceFromEdge);
    if (building.style == Style::Workshop || building.style == Style::Greenhouse) return (distance + 1) / 2;
    if (building.style == Style::Storehouse) return std::min(distance, 3) + std::max(0, distance - 3) / 2;
    return distance;
}

void validateBounds(const HubBuilding& building)
{
    const auto size = hubBuildingFootprint(building);
    const int height = hubBuildingHeight(building);
    if (building.origin.x <= World::Min.x || building.origin.x >= World::MaxExclusive.x - size.x
        || building.origin.z <= World::Min.z || building.origin.z >= World::MaxExclusive.z - size.y
        || building.origin.y < World::Min.y || building.origin.y >= World::MaxExclusive.y - height) {
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

// Construction uses local coordinates and inclusive corners, then rotates once.
void box(World& world, const HubBuilding& building, glm::ivec3 from, glm::ivec3 to, Block block)
{
    for (int y = from.y; y <= to.y; ++y)
        for (int z = from.z; z <= to.z; ++z)
            for (int x = from.x; x <= to.x; ++x) world.set(worldCell(building, {x, y, z}), block);
}

void wallRing(World& world, const HubBuilding& building, int bottom, int top, Block material)
{
    const int w = building.width, d = building.depth;
    box(world, building, {0, bottom, 0}, {w-1, top, 0}, material);
    box(world, building, {0, bottom, d-1}, {w-1, top, d-1}, material);
    box(world, building, {0, bottom, 0}, {0, top, d-1}, material);
    box(world, building, {w-1, bottom, 0}, {w-1, top, d-1}, material);
}

void windowRow(World& world, const HubBuilding& building, int bottom, int top)
{
    const int w = building.width, d = building.depth;
    // Repeated two-block bays leave a column every four blocks.
    for (int along = 1; along + 1 < d - 1; along += 4) {
        box(world, building, {0, bottom, along}, {0, top, along+1}, Block::Air);
        box(world, building, {w-1, bottom, along}, {w-1, top, along+1}, Block::Air);
    }
    for (int along = 1; along + 1 < w - 1; along += 4) {
        box(world, building, {along, bottom, 0}, {along+1, top, 0}, Block::Air);
        box(world, building, {along, bottom, d-1}, {along+1, top, d-1}, Block::Air);
    }
}

void room(World& world, const HubBuilding& building)
{
    const int w = building.width, d = building.depth, height = hubBuildingWallHeight(building);
    const bool stone = building.style == Style::Hall || building.style == Style::Tower;
    const bool plaster = building.style == Style::Inn || (building.style == Style::Cottage && building.variant % 2 == 1);
    const Block wall = stone || plaster ? Block::Limestone : Block::Planks;
    const Block frame = stone ? Block::Slate : Block::Timber;
    box(world, building, {0, 1, 0}, {w-1, hubBuildingHeight(building), d-1}, Block::Air);
    box(world, building, {0, 0, 0}, {w-1, 0, d-1}, Block::Limestone);
    box(world, building, {1, 0, 1}, {w-2, 0, d-2}, Block::Planks);
    wallRing(world, building, 1, height, wall);
    wallRing(world, building, 1, building.style == Style::Workshop ? 2 : 1,
             building.style == Style::Workshop ? Block::Brick : Block::Slate);
    for (int x = 0; x < w; ++x) {
        if (x % 4 != 0 && x != w - 1) continue;
        for (const int z : {0, d-1}) box(world, building, {x, 1, z}, {x, height, z}, frame);
    }
    for (int z = 0; z < d; ++z) {
        if (z % 4 != 0 && z != d - 1) continue;
        for (const int x : {0, w-1}) box(world, building, {x, 1, z}, {x, height, z}, frame);
    }
    wallRing(world, building, height, height, frame);
    if (building.style != Style::Tower && building.style != Style::Greenhouse)
        windowRow(world, building, building.style == Style::Storehouse ? 3 : 2,
                  building.style == Style::Storehouse ? 4 : 3);
}

// A gable can run along either axis. Barn roofs change pitch partway up;
// the open greenhouse keeps only ribs and a ridge, rather than solid panels.
void mainRoof(World& world, const HubBuilding& building)
{
    const bool sideways = sidewaysRoof(building);
    const bool open = building.style == Style::Greenhouse;
    const int inset = hallAisleWidth(building);
    const int acrossSize = sideways ? building.depth : building.width - 2 * inset;
    const int alongSize = sideways ? building.width : building.depth;
    const int base = hubBuildingWallHeight(building) + 1;
    const Block roof = building.style == Style::Workshop ? Block::Brick
        : building.style == Style::Storehouse && building.variant % 2 == 1 ? Block::Slate : Block::RoofTiles;
    const Block trim = building.style == Style::Hall ? Block::Slate : Block::Timber;
    const auto cell = [sideways, inset](int across, int y, int along) {
        return sideways ? glm::ivec3(along, y, across) : glm::ivec3(across + inset, y, along);
    };
    for (int across = -1; across <= acrossSize; ++across) {
        const int distance = std::min(across, acrossSize-1-across);
        const int height = base + roofRise(building, distance);
        for (int along = -1; along <= alongSize; ++along) {
            const bool endWall = along == 0 || along == alongSize-1;
            if (across >= 0 && across < acrossSize && endWall && !open) {
                const Block gable = building.style == Style::Hall || building.style == Style::Inn ? Block::Limestone : Block::Planks;
                box(world, building, cell(across, base, along), cell(across, height-1, along),
                    across == acrossSize/2 ? trim : gable);
            }
            const bool endTrim = along == -1 || along == alongSize;
            const bool rib = endTrim || along % 4 == 0 || across == acrossSize/2;
            if (open && !rib) continue;
            const bool edge = endTrim || across == -1 || across == acrossSize;
            // Courses overlap at each step; attic windows must not leave a roof
            // strip supported only by a diagonal contact with its neighbor.
            const int underside = std::max(base, height - 1);
            box(world, building, cell(across, underside, along), cell(across, height, along), open || edge ? trim : roof);
            if (open && endWall && across % 4 == 1)
                box(world, building, cell(across, height-1, along), cell(across, height-1, along), Block::Leaves);
        }
    }
    const int peak = base + roofRise(building, acrossSize / 2);
    for (const int along : {-1, alongSize})
        box(world, building, cell(acrossSize/2, peak+1, along), cell(acrossSize/2, peak+1, along), trim);
    if (open) return;

    // A timber tie, short rake braces and a small attic opening give each gable
    // a human scale instead of leaving a large uninterrupted plaster triangle.
    for (const int along : {0, alongSize - 1}) {
        for (int across = 1; across < acrossSize - 1; ++across) {
            const int top = base + roofRise(building, std::min(across, acrossSize - 1 - across));
            if (top > base)
                box(world, building, cell(across, base, along), cell(across, base, along), Block::Timber);
            if (top > base + 2)
                box(world, building, cell(across, top - 2, along), cell(across, top - 2, along), Block::Timber);
        }
        box(world, building, cell(acrossSize/2, base, along), cell(acrossSize/2, peak-1, along), Block::Timber);
        if (peak - base >= 4) {
            const int middle = acrossSize / 2;
            for (int offset = -1; offset <= 1; ++offset) {
                const int bottom = base + 1 + std::abs(offset);
                const int top = base + 3 - std::abs(offset);
                box(world, building, cell(middle + offset, bottom, along), cell(middle + offset, top, along), Block::Air);
            }
        }
    }
}

void monumentalHall(World& world, const HubBuilding& b)
{
    const int w = b.width, d = b.depth, height = hubBuildingWallHeight(b);
    const int aisle = hallAisleWidth(b);
    if (aisle > 0) {
        // Lower attached aisles make a stepped silhouette around the central hall.
        box(world, b, {0, 7, 0}, {aisle-1, height, d-1}, Block::Air);
        box(world, b, {w-aisle, 7, 0}, {w-1, height, d-1}, Block::Air);
        for (const int x : {aisle, w-1-aisle})
            box(world, b, {x, 7, 0}, {x, height, d-1}, Block::Limestone);
        for (int step = 0; step <= aisle; ++step) {
            const int top = 7 + step;
            for (const int x : {step-1, w-step}) {
                box(world, b, {x, top-1, -1}, {x, top, d}, Block::RoofTiles);
                for (const int z : {0, d-1})
                    box(world, b, {x, 7, z}, {x, top-2, z}, Block::Limestone);
            }
        }
        for (const int x : {aisle, w-1-aisle})
            for (int z = 3; z < d-2; z += 4)
                box(world, b, {x, height-3, z}, {x, height-2, z}, Block::Air);
    }
    wallRing(world, b, 6, 6, Block::Slate);
    for (int x = aisle+2; x < w-aisle-1; x += 4)
        for (const int z : {0, d-1})
            box(world, b, {x, 8, z}, {x, 10, z}, Block::Air);
    // Narrow piers frame the taller nave, with low buttresses beside the aisles.
    for (const int x : {aisle, w-1-aisle}) {
        box(world, b, {x, 1, d}, {x, height+2, d}, Block::Limestone);
        box(world, b, {x, height+3, d}, {x, height+3, d}, Block::Slate);
    }
    for (const int z : {3, d-4}) {
        box(world, b, {-1, 1, z}, {-1, aisle > 0 ? 7 : height-1, z}, Block::Limestone);
        box(world, b, {w, 1, z}, {w, aisle > 0 ? 7 : height-1, z}, Block::Limestone);
    }
    mainRoof(world, b);
}

void inn(World& world, const HubBuilding& b)
{
    const int w = b.width, d = b.depth;
    wallRing(world, b, 5, 5, Block::Timber);
    // Alternate compact windows with braced panels rather than repeating tall
    // openings across every bay of the upper facade.
    for (int start = 1; start + 2 < w; start += 4) {
        for (const int z : {0, d-1}) {
            if ((start / 4 + b.variant) % 3 == 0) {
                for (int step = 0; step < 3; ++step)
                    box(world, b, {start+step, 6+step, z}, {start+step, 6+step, z}, Block::Timber);
            } else {
                box(world, b, {start+1, 6, z}, {start+1, 7, z}, Block::Air);
                box(world, b, {start, 8, z}, {start+2, 8, z}, Block::Timber);
            }
        }
    }
    for (int z = 2; z < d-2; z += 4)
        for (const int x : {0, w-1})
            box(world, b, {x, 6, z}, {x, 7, z+1}, Block::Air);
    // A recessed entrance sits under the upper facade, forming a real porch.
    box(world, b, {1, 1, d-1}, {w-2, 4, d-1}, Block::Air);
    box(world, b, {1, 1, d-3}, {w-2, 4, d-3}, Block::Limestone);
    box(world, b, {0, 5, d-3}, {w-1, 5, d-1}, Block::Planks);
    for (const int x : {1, w-2}) box(world, b, {x, 1, d-1}, {x, 5, d-1}, Block::Timber);
    // The front cross-gable breaks the long roof; flanking dormers are smaller.
    const int center = w/2 + (w >= 17 ? (b.variant % 2 == 0 ? -2 : 2) : 0);
    const auto dormer = [&](int middle, int half, int base, int back) {
        for (int x=middle-half;x<=middle+half;++x) {
            const int top=base+half-std::abs(x-middle);
            box(world, b, {x, 11, d-1}, {x, top-1, d-1}, Block::Limestone);
            box(world, b, {x, top, back}, {x, top, d}, Block::RoofTiles);
            box(world, b, {x, top, d}, {x, top, d}, Block::Timber);
        }
        box(world, b, {middle-1, 11, d-1}, {middle+1, 11, d-1}, Block::Timber);
        box(world, b, {middle, 12, d-1}, {middle, base, d-1}, Block::Air);
    };
    dormer(center, std::min(3, (w-3)/2), 13, d/2);
    if (w>=17) dormer(center < w/2 ? w-4 : 3, 2, 12, d-3);
}

void cottagePorch(World& world, const HubBuilding& b)
{
    const int middle = b.width / 2, half = b.width >= 11 ? 2 : 1;
    for (int x = middle-half; x <= middle+half; ++x) {
        const int top = 5 + half - std::abs(x-middle);
        box(world, b, {x, top, b.depth-2}, {x, top, b.depth}, Block::RoofTiles);
        box(world, b, {x, top, b.depth}, {x, top, b.depth}, Block::Timber);
    }
    for (const int x : {middle-half, middle+half})
        box(world, b, {x, 4, b.depth}, {x, 5, b.depth}, Block::Timber);
}

void tower(World& world, const HubBuilding& b)
{
    const int w = b.width, d = b.depth, height = hubBuildingWallHeight(b);
    for (const int y:{5, 12, 18}) wallRing(world, b, y, y, Block::Limestone);
    for (const int bottom:{6, 13}) {
        for (const int x:{0, w-1}) box(world, b, {x, bottom, d/2}, {x, bottom+2, d/2}, Block::Air);
        for (const int z:{0, d-1}) box(world, b, {w/2, bottom, z}, {w/2, bottom+2, z}, Block::Air);
    }
    // A timber belfry lightens the top of the solid stone landmark.
    wallRing(world, b, 15, 19, Block::Timber);
    for (int x = 2; x < w-2; x += 3)
        for (const int z : {0, d-1}) box(world, b, {x, 16, z}, {x+1, 18, z}, Block::Air);
    for (int z = 2; z < d-2; z += 3)
        for (const int x : {0, w-1}) box(world, b, {x, 16, z}, {x, 18, z+1}, Block::Air);
    // The square landmark has a four-sided roof and capped corner merlons.
    for (int z=-1;z<=d;++z) for (int x=-1;x<=w;++x) {
        const int inset=std::max(0, std::min({x+1, w-x, z+1, d-z, 10}));
        const int top=height+1+inset;
        const bool edge=x==-1||x==w||z==-1||z==d;
        box(world, b, {x, top-1, z}, {x, top, z}, edge?Block::Limestone:Block::RoofTiles);
    }
    for (const int x:{0, w-1}) for (const int z:{0, d-1})
        box(world, b, {x, height+2, z}, {x, height+3, z}, Block::Limestone);
    box(world, b, {w/2, hubBuildingHeight(b), d/2}, {w/2, hubBuildingHeight(b), d/2}, Block::Lantern);
}

void greenhouse(World& world, const HubBuilding& b)
{
    const int w = b.width, d = b.depth;
    const auto arches = [&](bool side, int size) {
        for (int start=1;start<size-1;start+=4) {
            const int end=std::min(start+2, size-2);
            for (const int edge:{0, side?w-1:d-1}) {
                if (side) {
                    box(world, b, {edge, 2, start}, {edge, 4, end}, Block::Air);
                    box(world, b, {edge, 5, (start+end)/2}, {edge, 5, (start+end)/2}, Block::Air);
                } else {
                    box(world, b, {start, 2, edge}, {end, 4, edge}, Block::Air);
                    box(world, b, {(start+end)/2, 5, edge}, {(start+end)/2, 5, edge}, Block::Air);
                }
            }
        }
    };
    arches(true, d);arches(false, w);
    // Planted sill beds occupy the perimeter, leaving the full interior walkable.
    for (int z=2;z<d-2;z+=4) for (const int x:{0, w-1}) {
        box(world, b, {x, 1, z}, {x, 1, z+1}, Block::Brick);
        box(world, b, {x, 2, z}, {x, 2, z+1}, Block::Leaves);
    }
}

void entrance(World& world, const HubBuilding& b)
{
    const int w = b.width, d = b.depth;
    int width=w>=15?5:3, height=4;
    if (b.style==Style::Workshop||b.style==Style::Greenhouse) width=w-2;
    if (b.style==Style::Cottage) {width=w>=11?3:2;height=3;}
    if (b.style==Style::Hall) {width=std::min(w-4, 7);height=6;}
    if (b.style==Style::Storehouse) {width=w>=19?7:w>=11?5:3;height=5;}
    const int left=(w-width)/2, right=left+width-1;
    const Block frame=b.style==Style::Hall||b.style==Style::Tower?Block::Limestone:Block::Timber;
    for (const int x:{left-1, right+1}) box(world, b, {x, 1, d-1}, {x, height+1, d-1}, frame);
    box(world, b, {left-1, height+1, d-1}, {right+1, height+1, d-1}, frame);
    box(world, b, {left, 1, b.style==Style::Inn?d-3:d-1}, {right, height, d}, Block::Air);
    box(world, b, {left, 0, d}, {right, 0, d}, Block::Limestone);
    box(world, b, {right+1, 3, d-1}, {right+1, 3, d-1}, Block::Lantern);
    if (b.style!=Style::Cottage) box(world, b, {left-1, 3, d-1}, {left-1, 3, d-1}, Block::Lantern);
    // Suspension starts above usable headroom, even in the smallest building.
    const int lamp=std::min(hubBuildingWallHeight(b), 6);
    int ceiling=lamp+1;
    while (ceiling<hubBuildingHeight(b)
        && !isSolid(world.get(worldCell(b, {w/2, ceiling, d/2})))) ++ceiling;
    box(world, b, {w/2, lamp+1, d/2}, {w/2, ceiling-1, d/2}, Block::Timber);
    box(world, b, {w/2, lamp, d/2}, {w/2, lamp, d/2}, Block::Lantern);
}

} // namespace

glm::ivec2 hubBuildingFootprint(const HubBuilding& b)
{
    validateShape(b);
    return b.facing%2==0?glm::ivec2(b.width, b.depth):glm::ivec2(b.depth, b.width);
}

int hubBuildingWallHeight(const HubBuilding& b)
{
    validateShape(b);
    switch (b.style) {
    case Style::Cottage: return 5+b.variant%2;
    case Style::Hall: return b.width>=17?14:12;
    case Style::Workshop: return 6;
    case Style::Storehouse: return 8;
    case Style::Inn: return 10;
    case Style::Tower: return 20;
    case Style::Greenhouse: return 6;
    }
    return 0;
}

int hubBuildingHeight(const HubBuilding& b)
{
    const int wall = hubBuildingWallHeight(b);
    if (b.style==Style::Tower) return wall+2+std::min({(b.width+1)/2, (b.depth+1)/2, 10});
    const int across=sidewaysRoof(b)?b.depth:b.width-2*hallAisleWidth(b);
    int top=wall+2+roofRise(b, across/2);
    if (b.style==Style::Cottage) top=std::max(top, wall+6);
    if (b.style==Style::Workshop) top=std::max(top, wall+b.depth/4+7);
    if (b.style==Style::Hall) top=std::max(top, wall+3);
    if (b.style==Style::Inn) top=std::max(top, 13+std::min(3, (b.width-3)/2));
    return top;
}

glm::ivec3 hubBuildingDoor(const HubBuilding& b)
{
    validateBounds(b);
    return worldCell(b, {b.width/2, 1, b.depth-1});
}

void buildHubBuilding(World& world, const HubBuilding& b)
{
    validateBounds(b);
    room(world, b);
    if (b.style==Style::Tower) tower(world, b);
    else if (b.style==Style::Hall) monumentalHall(world, b);
    else mainRoof(world, b);
    switch (b.style) {
    case Style::Inn: inn(world, b); break;
    case Style::Greenhouse: greenhouse(world, b); break;
    default: break;
    }
    if (b.style==Style::Workshop) {
        const int top=hubBuildingHeight(b);
        // Side doors make the covered work yard open from the adjoining lane.
        for (const int x : {0, b.width-1}) {
            box(world, b, {x, 1, b.depth-4}, {x, 4, b.depth-2}, Block::Air);
            box(world, b, {x, 5, b.depth-4}, {x, 5, b.depth-2}, Block::Timber);
        }
        box(world, b, {b.width-4, 5, 0}, {b.width-2, 7, 2}, Block::Brick);
        box(world, b, {b.width-3, 6, 0}, {b.width-2, top-1, 1}, Block::Brick);
        box(world, b, {b.width-3, top, 0}, {b.width-2, top, 1}, Block::Slate);
        box(world, b, {b.width-4, 2, 0}, {b.width-2, 3, 0}, Block::Slate);
        box(world, b, {b.width-3, 2, 0}, {b.width-3, 2, 0}, Block::Lantern);
    }
    if (b.style==Style::Cottage) {
        cottagePorch(world, b);
        const int top=hubBuildingWallHeight(b)+6;
        box(world, b, {b.width-2, hubBuildingWallHeight(b), 1}, {b.width-2, top-1, 1}, Block::Brick);
        box(world, b, {b.width-2, top, 1}, {b.width-2, top, 1}, Block::Slate);
    }
    if (b.style==Style::Storehouse) {
        const int middle = b.width / 2;
        box(world, b, {middle-1, 6, b.depth-1}, {middle+1, 6, b.depth-1}, Block::Timber);
        box(world, b, {middle, 7, b.depth-1}, {middle+1, 8, b.depth-1}, Block::Air);
    }
    entrance(world, b);
}

} // namespace voxel
