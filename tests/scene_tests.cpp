#include "scene.h"
#include "player.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <queue>
#include <string>
#include <glm/geometric.hpp>

namespace {
int failures = 0;
constexpr int Width = voxel::World::MaxExclusive.x - voxel::World::Min.x;
constexpr int FloorY = 9;
constexpr std::array<glm::ivec3, 4> Steps{{{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
using FloorMap = std::array<bool, Width * Width>;

void check(bool condition, const std::string& message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

std::size_t index(int x, int z)
{
    return static_cast<std::size_t>((z - voxel::World::Min.z) * Width + x - voxel::World::Min.x);
}

std::string coordinate(glm::ivec3 p)
{
    return "(" + std::to_string(p.x) + "," + std::to_string(p.y) + "," + std::to_string(p.z) + ")";
}

bool supported(const voxel::World& world, glm::ivec3 feet)
{
    voxel::Player player;
    return player.setFeetPosition(world, glm::vec3(feet) + glm::vec3(0.5f, 0, 0.5f)) && player.grounded();
}

FloorMap walkableRoutes(const voxel::World& world)
{
    FloorMap reached{};
    const glm::ivec3 start(glm::floor(voxel::VillageSpawn));
    if (!supported(world, start)) return reached;
    std::queue<glm::ivec3> frontier;
    frontier.push(start);
    reached[index(start.x, start.z)] = true;
    while (!frontier.empty()) {
        const auto current = frontier.front();
        frontier.pop();
        for (const auto step : Steps) {
            const auto next = current + step;
            if (!voxel::World::contains(next) || reached[index(next.x, next.z)]) continue;
            voxel::Player player;
            if (!player.setFeetPosition(world, glm::vec3(current) + glm::vec3(0.5f, 0, 0.5f))) continue;
            // Each route edge uses the actual swept body, without flying or jumping.
            player.move(world, glm::vec3(step));
            const auto destination = glm::vec3(next) + glm::vec3(0.5f, 0, 0.5f);
            if (glm::distance(player.feetPosition(), destination) > 0.001f || !player.grounded()) continue;
            reached[index(next.x, next.z)] = true;
            frontier.push(next);
        }
    }
    return reached;
}

void checkRoute(const voxel::World& world, const FloorMap& routes, glm::ivec3 feet,
                const std::string& label)
{
    const bool reached = voxel::World::contains(feet) && feet.y == FloorY
        && routes[index(feet.x, feet.z)] && supported(world, feet);
    if (!reached) {
        glm::ivec3 nearest{};
        int distance = Width * 2;
        for (int z = voxel::World::Min.z; z < voxel::World::MaxExclusive.z; ++z) {
            for (int x = voxel::World::Min.x; x < voxel::World::MaxExclusive.x; ++x) {
                const int candidate = std::abs(x - feet.x) + std::abs(z - feet.z);
                if (routes[index(x, z)] && candidate < distance) { distance = candidate; nearest = {x, FloorY, z}; }
            }
        }
        std::cerr << "seed " << world.seed() << ' ' << label << " at " << coordinate(feet)
                  << "; nearest reachable " << coordinate(nearest)
                  << "; floor/body/head " << voxel::blockName(world.get(feet - glm::ivec3(0, 1, 0)))
                  << '/' << voxel::blockName(world.get(feet))
                  << '/' << voxel::blockName(world.get(feet + glm::ivec3(0, 1, 0))) << '\n';
    }
    check(reached, label + " has a grounded route from spawn");
}

// Find a real three-step bank: basin feet6 -> shelf7 -> shelf8 -> village9.
// Validate the body on every landing, then replay jumps on one such exit.
bool waterExit(const voxel::World& world, const FloorMap& routes)
{
    for (int z = voxel::World::Min.z + 3; z < voxel::World::MaxExclusive.z - 3; ++z) {
        for (int x = voxel::World::Min.x + 3; x < voxel::World::MaxExclusive.x - 3; ++x) {
            const glm::ivec3 bank{x, FloorY, z};
            if (!routes[index(x, z)]) continue;
            for (const auto side : Steps) {
                const auto upper = bank + side - glm::ivec3(0, 1, 0);
                const auto lower = bank + side * 2 - glm::ivec3(0, 2, 0);
                const auto basin = bank + side * 3 - glm::ivec3(0, 3, 0);
                if (world.get(basin) != voxel::Block::Water || !supported(world, basin)
                    || !supported(world, lower) || !supported(world, upper)) continue;
                voxel::Player player;
                player.setFeetPosition(world, glm::vec3(basin) + glm::vec3(0.5f, 0, 0.5f));
                bool climbed = true;
                for (const auto landing : {lower, upper, bank}) {
                    const auto target = glm::vec3(landing) + glm::vec3(0.5f, 0, 0.5f);
                    player.update(world, {}, glm::vec3(-side), false, false, 1.0f / 120.0f);
                    for (int frame = 0; frame < 100; ++frame) {
                        const auto difference = target - player.feetPosition();
                        const float horizontal = std::abs(difference.x) + std::abs(difference.z);
                        const float move = std::min(1.0f, horizontal * 120.0f / 4.5f);
                        player.update(world, {0, move}, glm::vec3(-side), frame == 0, false, 1.0f / 120.0f);
                        if (player.grounded() && glm::distance(player.feetPosition(), target) < 0.01f) break;
                    }
                    climbed &= player.grounded() && glm::distance(player.feetPosition(), target) < 0.01f;
                    if (!climbed) break;
                }
                if (climbed) return true;
            }
        }
    }
    return false;
}
} // namespace

int main()
{
    using voxel::Block;
    using voxel::World;
    std::optional<World> previous;
    bool seedChangesGeometry = false;
    for (const std::uint32_t seed : {0u, 42u, 1337u, 4294967295u}) {
        World world(seed);
        voxel::buildUndergroundScene(world);
        World rebuilt(seed);
        rebuilt.set({2, 40, 2}, Block::Sand);
        rebuilt.set(World::Min, Block::Air);
        voxel::buildUndergroundScene(rebuilt);
        check(world.sameContents(rebuilt), "scene deterministically replaces prior edits, including the upper layers and shell");
        voxel::buildUndergroundScene(rebuilt);
        check(world.sameContents(rebuilt), "rebuilding the designed hub is idempotent");
        if (previous) {
            for (std::size_t chunk = 0; chunk < World::ChunkCount; ++chunk)
                seedChangesGeometry |= world.chunk(chunk) != previous->chunk(chunk);
        }
        previous = world;

        bool closed = true;
        std::array<int, voxel::BlockCount> counts{}, exposed{};
        int highestWater = 0, highestWood = 0, highestCanopy = 0, crownLeaves = 0;
        glm::ivec3 canopyLow(World::MaxExclusive), canopyHigh(World::Min);
        std::array<int, 4> branches{};
        for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
            for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
                for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
                    const glm::ivec3 position{x, y, z};
                    const auto block = world.get(position);
                    ++counts[static_cast<std::size_t>(block)];
                    if (x == World::Min.x || x == World::MaxExclusive.x - 1
                        || z == World::Min.z || z == World::MaxExclusive.z - 1
                        || y == World::Min.y || y == World::MaxExclusive.y - 1) closed &= block == Block::Bedrock;
                    if (block == Block::Water) highestWater = std::max(highestWater, y);
                    if (block == Block::CoalOre || block == Block::CopperOre || block == Block::IronOre || block == Block::Crystal) {
                        bool visible = false;
                        for (const auto side : {glm::ivec3(1, 0, 0), {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}})
                            visible |= world.get(position + side) == Block::Air;
                        exposed[static_cast<std::size_t>(block)] += visible;
                    }
                    if (x < -20 || x > 37 || z < -20 || z > 37) continue;
                    if (block == Block::Timber && y >= 20) {
                        highestWood = std::max(highestWood, y);
                        if (y >= 24) {
                            branches[0] += x < 0; branches[1] += x > 18;
                            branches[2] += z < 0; branches[3] += z > 18;
                        }
                    }
                    if ((block == Block::Leaves || block == Block::GlowLeaves) && y >= 29) {
                        ++crownLeaves;
                        highestCanopy = std::max(highestCanopy, y);
                        canopyLow = glm::min(canopyLow, position);
                        canopyHigh = glm::max(canopyHigh, position);
                    }
                }
            }
        }
        check(closed, "bedrock seals all six outer faces across the complete sixty-four-block height");
        const auto substantial = [&](Block block, int minimum) {
            check(counts[static_cast<std::size_t>(block)] >= minimum,
                  std::string(voxel::blockName(block)) + " forms substantial hub features (count "
                  + std::to_string(counts[static_cast<std::size_t>(block)]) + ")");
        };
        substantial(Block::Water, 1000); substantial(Block::Leaves, 5000); substantial(Block::GlowLeaves, 150);
        substantial(Block::Timber, 1000); substantial(Block::Planks, 2500); substantial(Block::RoofTiles, 1000);
        substantial(Block::Brick, 120); substantial(Block::Lantern, 70); substantial(Block::Grass, 500);
        check(highestWater >= 25, "falling water reaches far above the lake surface");
        check(world.get({8, 15, 8}) == Block::Timber && highestWood >= 34 && highestCanopy >= 44,
              "the heart tree has a substantial central trunk and a tall crown");
        check(crownLeaves >= 5000 && canopyHigh.x - canopyLow.x >= 40 && canopyHigh.z - canopyLow.z >= 40,
              "the central canopy spreads broadly above the village");
        check(std::all_of(branches.begin(), branches.end(), [](int count) { return count >= 10; }),
              "large tree branches spread in all four directions");
        for (const auto ore : {Block::CoalOre, Block::CopperOre, Block::IronOre, Block::Crystal})
            check(exposed[static_cast<std::size_t>(ore)] >= 5, std::string(voxel::blockName(ore)) + " remains visibly exposed");

        voxel::Player player;
        check(player.setFeetPosition(world, voxel::VillageSpawn) && player.grounded(),
              "arrival spawn has full player clearance and ground support");
        const auto routes = walkableRoutes(world);
        check(std::count(routes.begin(), routes.end(), true) >= 5000, "hub has a substantial connected walking area");
        const auto buildings = voxel::sceneBuildingPlan();
        check(buildings.size() == 28, "the hub contains twenty-eight designed buildings");
        for (std::size_t i = 0; i < buildings.size(); ++i) {
            const auto& building = buildings[i];
            const auto door = voxel::hubBuildingDoor(building);
            const auto footprint = voxel::hubBuildingFootprint(building);
            const auto room = building.origin + glm::ivec3(footprint.x / 2, 1, footprint.y / 2);
            checkRoute(world, routes, door, "building " + std::to_string(i + 1) + " door");
            checkRoute(world, routes, room, "building " + std::to_string(i + 1) + " interior");
            bool roof = false;
            for (int y = room.y + 2; y <= building.origin.y + 14; ++y)
                roof |= world.get({room.x, y, room.z}) == Block::RoofTiles;
            check(world.get(room - glm::ivec3(0, 1, 0)) == Block::Planks && roof,
                  "building " + std::to_string(i + 1) + " has a finished interior floor and tiled roof");
        }
        checkRoute(world, routes, glm::ivec3(glm::floor(voxel::ExpeditionDeparture)), "expedition staging hall");
        // Sample the entire center line of each cardinal bridge, not just its ends.
        for (int coordinate = -20; coordinate <= 36; ++coordinate) {
            if (coordinate <= -2 || coordinate >= 18) checkRoute(world, routes, {coordinate, FloorY, 8}, "east/west bridge");
        }
        for (int coordinate = -19; coordinate <= 34; ++coordinate) {
            if (coordinate <= -2 || coordinate >= 18) checkRoute(world, routes, {8, FloorY, coordinate}, "north/south bridge");
        }
        check(waterExit(world, routes), "players can jump from the basin up shallow shelves to connected village ground");
    }
    check(seedChangesGeometry, "different seeds change actual geology and foliage rather than only metadata");
    return failures ? 1 : 0;
}
