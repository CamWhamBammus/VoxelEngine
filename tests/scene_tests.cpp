#include "scene.h"
#include "player.h"
#include "world.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <queue>
#include <glm/geometric.hpp>

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

constexpr int Width = voxel::World::MaxExclusive.x - voxel::World::Min.x;
using FloorMap = std::array<bool, Width * Width>;

std::size_t index(int x, int z)
{
    return static_cast<std::size_t>((z - voxel::World::Min.z) * Width + x - voxel::World::Min.x);
}

FloorMap walkableRoutes(const voxel::World& world)
{
    FloorMap reached{};
    std::queue<glm::ivec3> frontier;
    const glm::ivec3 start{static_cast<int>(std::floor(voxel::VillageSpawn.x)), 4,
                           static_cast<int>(std::floor(voxel::VillageSpawn.z))};
    frontier.push(start);
    reached[index(start.x, start.z)] = true;
    constexpr std::array<glm::ivec3, 4> steps{{{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
    while (!frontier.empty()) {
        const auto current = frontier.front();
        frontier.pop();
        for (const auto step : steps) {
            const auto next = current + step;
            if (!voxel::World::contains(next) || reached[index(next.x, next.z)]) continue;
            const glm::vec3 currentFeet{current.x + 0.5f, 4, current.z + 0.5f};
            const glm::vec3 nextFeet{next.x + 0.5f, 4, next.z + 0.5f};
            voxel::Player player;
            if (!player.setFeetPosition(world, currentFeet)) continue;
            // Exercise the actual swept player body along every discovered edge.
            player.move(world, glm::vec3(step));
            if (glm::distance(player.feetPosition(), nextFeet) > 0.001f || !player.grounded()) continue;
            reached[index(next.x, next.z)] = true;
            frontier.push(next);
        }
    }
    return reached;
}

} // namespace

int main()
{
    using voxel::Block;
    using voxel::World;
    for (const std::uint32_t seed : {0u, 42u, 1337u, 4294967295u}) {
        World world(seed);
        voxel::buildUndergroundScene(world);
        World rebuilt(seed);
        rebuilt.set({2, 5, 2}, Block::Sand);
        voxel::buildUndergroundScene(rebuilt);
        check(world.sameContents(rebuilt), "scene replaces prior terrain and edits deterministically");
        voxel::buildUndergroundScene(rebuilt);
        check(world.sameContents(rebuilt), "rebuilding the sample scene is idempotent");

        bool closed = true;
        std::array<int, voxel::BlockCount> counts{};
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
                for (int y = 0; y < World::MaxExclusive.y; ++y) {
                    const auto block = world.get({x, y, z});
                    ++counts[static_cast<std::size_t>(block)];
                    if (x == World::Min.x || x == World::MaxExclusive.x - 1
                        || z == World::Min.z || z == World::MaxExclusive.z - 1 || y == 0 || y == 15) {
                        closed &= block == Block::Bedrock;
                    }
                }
            }
        }
        check(closed, "bedrock closes every outer face, including floor and roof");
        check(counts[static_cast<int>(Block::Grass)] == 0 && counts[static_cast<int>(Block::Sand)] == 0,
              "surface terrain is completely replaced");
        for (const Block block : {Block::Limestone, Block::Stone, Block::Slate,
                                  Block::Timber, Block::Planks, Block::Brick}) {
            check(counts[static_cast<int>(block)] > 40, "rocks and village materials form substantial structures");
        }
        check(counts[static_cast<int>(Block::Lantern)] >= 12, "village and passages have warm lantern landmarks");

        voxel::Player player;
        check(player.setFeetPosition(world, voxel::VillageSpawn) && player.grounded(),
              "village spawn has full body clearance and solid ground");
        for (const glm::ivec3 frame : {glm::ivec3(-16, 4, 5), glm::ivec3(-22, 4, 1),
                                       glm::ivec3(21, 4, 3), glm::ivec3(28, 4, 7)}) {
            check(world.get(frame + glm::ivec3(0, 4, 0)) == Block::Timber
                  && player.setFeetPosition(world, glm::vec3(frame) + glm::vec3(0.5f, 0, 0.5f))
                  && player.grounded(), "mine frames have a visible overhead beam and clear grounded passage");
        }
        const auto routes = walkableRoutes(world);
        int reachableCells = 0;
        for (const bool reachable : routes) reachableCells += reachable;
        check(reachableCells > 900, "village and mines form a substantial connected walking area");
        for (const auto chamber : voxel::OreChambers) {
            check(routes[index(static_cast<int>(std::floor(chamber.x)), static_cast<int>(std::floor(chamber.z)))],
                  "every ore chamber is reachable with the player body without jumping");
        }
        for (const glm::ivec3 room : {glm::ivec3(-8, 4, -2), glm::ivec3(8, 4, -3), glm::ivec3(11, 4, 11)}) {
            check(routes[index(room.x, room.z)], "all three building interiors have usable doors");
        }

        std::array<int, voxel::BlockCount> reachableOre{};
        constexpr std::array<glm::ivec3, 4> neighbors{{{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
        for (int z = World::Min.z + 1; z < World::MaxExclusive.z - 1; ++z) {
            for (int x = World::Min.x + 1; x < World::MaxExclusive.x - 1; ++x) {
                for (int y = 4; y <= 6; ++y) {
                    const Block block = world.get({x, y, z});
                    if (block != Block::CoalOre && block != Block::CopperOre
                        && block != Block::IronOre && block != Block::Crystal) continue;
                    bool exposed = false;
                    for (const auto step : neighbors) {
                        const glm::ivec3 beside = glm::ivec3(x, y, z) + step;
                        exposed |= routes[index(beside.x, beside.z)] && world.get(beside) == Block::Air;
                    }
                    if (exposed) ++reachableOre[static_cast<int>(block)];
                }
            }
        }
        for (const Block ore : {Block::CoalOre, Block::CopperOre, Block::IronOre, Block::Crystal}) {
            check(reachableOre[static_cast<int>(ore)] >= 6, "clustered ore is exposed beside walkable ground at reachable height");
        }
    }

    World first(42), second(1337);
    voxel::buildUndergroundScene(first);
    voxel::buildUndergroundScene(second);
    bool different = false;
    for (std::size_t chunk = 0; chunk < World::ChunkCount; ++chunk) {
        different |= first.chunk(chunk) != second.chunk(chunk);
    }
    check(different, "different seeds change the actual geology, not only world metadata");
    return failures ? 1 : 0;
}
