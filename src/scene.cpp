#include "scene.h"
#include "world.h"

#include <algorithm>
#include <cstdint>

namespace voxel {
namespace {

std::uint32_t hash(int x, int z, std::uint32_t seed)
{
    auto value = seed ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u)
                      ^ (static_cast<std::uint32_t>(z) * 0x85ebca6bu);
    value ^= value >> 16;
    value *= 0x7feb352du;
    return value ^ (value >> 15);
}

// Box corners are inclusive, matching the coordinates used in the scene plan.
void box(World& world, glm::ivec3 from, glm::ivec3 to, Block block)
{
    for (int y = from.y; y <= to.y; ++y) {
        for (int z = from.z; z <= to.z; ++z) {
            for (int x = from.x; x <= to.x; ++x) world.set({x, y, z}, block);
        }
    }
}

void cavern(World& world, int centerX, int centerZ, int radiusX, int radiusZ, int ceiling,
            Block floor = Block::Limestone)
{
    for (int z = centerZ - radiusZ; z <= centerZ + radiusZ; ++z) {
        for (int x = centerX - radiusX; x <= centerX + radiusX; ++x) {
            const float dx = float(x - centerX) / radiusX;
            const float dz = float(z - centerZ) / radiusZ;
            const float distance = dx * dx + dz * dz;
            if (distance > 1.0f) continue;
            // Broad roof shelves and a lower rim read as rock rather than a box.
            const int shelf = static_cast<int>(hash(x / 4, z / 4, world.seed()) % 2);
            const int top = ceiling - (distance > 0.72f ? 2 : 0) + shelf;
            box(world, {x, 4, z}, {x, top, z}, Block::Air);
            world.set({x, 3, z}, floor);
        }
    }
}

void tunnel(World& world, int x1, int z1, int x2, int z2)
{
    // Each segment is axis-aligned; overlapping segments make generous corners.
    const int left = std::min(x1, x2) - 2;
    const int right = std::max(x1, x2) + 2;
    const int back = std::min(z1, z2) - 2;
    const int front = std::max(z1, z2) + 2;
    box(world, {left, 4, back}, {right, 8, front}, Block::Air);
    box(world, {left, 3, back}, {right, 3, front}, Block::Limestone);
    // Narrower upper opening rounds off the otherwise rectangular passage.
    box(world, {left + 1, 9, back + 1}, {right - 1, 9, front - 1}, Block::Air);
}

void lampPost(World& world, int x, int z)
{
    box(world, {x, 4, z}, {x, 5, z}, Block::Timber);
    world.set({x, 6, z}, Block::Lantern);
}

void supportFrame(World& world, int x, int z, bool passageRunsEastWest)
{
    // Posts occupy the outer passage edges, leaving three clear walking cells.
    const glm::ivec3 across = passageRunsEastWest ? glm::ivec3(0, 0, 1) : glm::ivec3(1, 0, 0);
    for (const int side : {-2, 2}) {
        const auto post = glm::ivec3(x, 4, z) + across * side;
        box(world, post, post + glm::ivec3(0, 3, 0), Block::Timber);
    }
    const glm::ivec3 beam{x, 8, z};
    box(world, beam - across * 2, beam + across * 2, Block::Timber);
}

void cottage(World& world, int x, int z, bool westDoor)
{
    box(world, {x, 3, z}, {x + 6, 3, z + 6}, Block::Planks);
    box(world, {x, 4, z}, {x + 6, 4, z + 6}, Block::Brick);
    box(world, {x, 5, z}, {x + 6, 7, z + 6}, Block::Planks);
    box(world, {x + 1, 4, z + 1}, {x + 5, 7, z + 5}, Block::Air);
    for (const int postX : {x, x + 6}) {
        for (const int postZ : {z, z + 6}) {
            box(world, {postX, 4, postZ}, {postX, 8, postZ}, Block::Timber);
        }
    }
    box(world, {x, 8, z}, {x + 6, 8, z + 6}, Block::Timber);
    box(world, {x + 1, 9, z + 1}, {x + 5, 9, z + 5}, Block::Planks);
    // Two-block doors and windows leave the interiors visible and accessible.
    if (westDoor) {
        box(world, {x, 4, z + 2}, {x, 6, z + 3}, Block::Air);
        world.set({x, 6, z + 4}, Block::Lantern);
    } else {
        box(world, {x + 2, 4, z + 6}, {x + 3, 6, z + 6}, Block::Air);
        world.set({x + 4, 6, z + 6}, Block::Lantern);
    }
    box(world, {x + 2, 6, z}, {x + 3, 6, z}, Block::Air);
    box(world, {x + 6, 6, z + 2}, {x + 6, 6, z + 3}, Block::Air);
    // A low workbench and a warm overhead light furnish each room.
    box(world, {x + 2, 4, z + 1}, {x + 4, 4, z + 1}, Block::Timber);
    world.set({x + 3, 7, z + 3}, Block::Lantern);
}

void vein(World& world, glm::ivec3 center, Block ore, int radius)
{
    for (int z = center.z - radius; z <= center.z + radius; ++z) {
        for (int x = center.x - radius; x <= center.x + radius; ++x) {
            for (int y = 4; y <= 7; ++y) {
                const auto offset = glm::ivec3(x, y, z) - center;
                if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z > radius * radius) continue;
                const Block old = world.get({x, y, z});
                if (old == Block::Limestone || old == Block::Stone || old == Block::Slate) {
                    world.set({x, y, z}, ore);
                }
            }
        }
    }
}

} // namespace

void buildUndergroundScene(World& world)
{
    // Start solid. The shell is never carved, so every view remains underground.
    for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
        for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
            const int layer = static_cast<int>(hash(x / 8, z / 8, world.seed()) % 3);
            for (int y = 0; y < World::MaxExclusive.y; ++y) {
                const bool shell = y == 0 || y == 15 || x == World::Min.x || z == World::Min.z
                    || x == World::MaxExclusive.x - 1 || z == World::MaxExclusive.z - 1;
                const Block rock = y < 3 + layer ? Block::Slate
                    : (y < 9 + layer ? Block::Limestone : Block::Stone);
                world.set({x, y, z}, shell ? Block::Bedrock : rock);
            }
        }
    }

    cavern(world, 2, 3, 17, 16, 12);
    cavern(world, -10, 3, 7, 10, 10);
    cavern(world, 13, 5, 8, 12, 11);
    cavern(world, -22, -15, 7, 7, 10, Block::Slate);
    cavern(world, 28, -13, 8, 7, 11);
    cavern(world, 27, 28, 8, 8, 12, Block::Slate);
    tunnel(world, -12, 5, -22, 5);
    tunnel(world, -22, 5, -22, -15);
    tunnel(world, 16, 3, 28, 3);
    tunnel(world, 28, 3, 28, -13);
    tunnel(world, 28, 3, 28, 28);
    supportFrame(world, -16, 5, true);
    supportFrame(world, -22, 1, false);
    supportFrame(world, 21, 3, true);
    supportFrame(world, 28, 7, false);

    // Brick paths join the square, cottage doors, and the mine entrances.
    box(world, {-3, 3, 2}, {4, 3, 8}, Block::Brick);
    box(world, {-14, 3, 4}, {19, 3, 6}, Block::Brick);
    box(world, {-3, 3, 7}, {-1, 3, 15}, Block::Brick);
    box(world, {-9, 3, 0}, {-7, 3, 5}, Block::Brick);
    box(world, {7, 3, -1}, {9, 3, 5}, Block::Brick);
    box(world, {5, 3, 5}, {7, 3, 12}, Block::Brick);
    cottage(world, -11, -5, false);
    cottage(world, 5, -6, false);
    cottage(world, 8, 8, true);

    // Square lantern, benches, and stacked supplies beside the workshop.
    lampPost(world, 2, 5);
    box(world, {0, 4, 7}, {1, 4, 7}, Block::Planks);
    box(world, {4, 4, 7}, {4, 4, 8}, Block::Planks);
    box(world, {-10, 4, 9}, {-8, 4, 11}, Block::Timber);
    box(world, {-10, 5, 9}, {-9, 5, 10}, Block::Planks);
    box(world, {-7, 4, 9}, {-7, 4, 10}, Block::Brick);
    lampPost(world, -11, 8);
    lampPost(world, 6, 9);
    lampPost(world, -13, 7);
    lampPost(world, 18, 5);
    for (const int z : {-8, 2}) lampPost(world, -24, z);
    for (const int z : {-8, 1, 12, 23}) lampPost(world, 30, z);

    // Clusters intersect chamber walls at eye height, never isolated speckles.
    const int shift = static_cast<int>(world.seed() % 2);
    vein(world, {-28, 5, -18 + shift}, Block::CoalOre, 3);
    vein(world, {-24, 6, -22}, Block::CoalOre, 3);
    vein(world, {-16, 5, -18}, Block::CoalOre, 2);
    vein(world, {34, 5, -17 + shift}, Block::CopperOre, 3);
    vein(world, {24, 5, -19}, Block::CopperOre, 3);
    vein(world, {35, 6, -10}, Block::CopperOre, 2);
    vein(world, {20, 5, 31}, Block::IronOre, 3);
    vein(world, {26, 6, 36 - shift}, Block::IronOre, 3);
    vein(world, {33, 5, 33}, Block::Crystal, 3);
    box(world, {31, 4, 31}, {32, 5, 32}, Block::Crystal);
    world.set({32, 6, 32}, Block::Crystal);
}

} // namespace voxel
