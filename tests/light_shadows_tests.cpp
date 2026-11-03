#include "graphics/light_shadows.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace voxel;
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

bool clear(const World& world, PointLight light)
{
    const auto result = unoccludedLightSources(world,std::span(&light,1));
    return result.size() == 1 && result[0] == 1;
}

void sourcesAndEdits(World& world)
{
    const PointLight light{{.75f,60.5f,.5f},{1,.5f,.2f},3};
    check(clear(world,light), "An empty influence sphere does not need voxel shadow traces");
    world.set({4,60,0},Block::Stone);
    check(!clear(world,light), "Receiver and area-light offsets expand the conservative shadow region");
    world.set({4,60,0},Block::Air);
    world.set({5,60,0},Block::Stone);
    check(clear(world,light), "A wall beyond the expanded influence radius cannot cast its shadows");
    world.set({5,60,0},Block::Air);
    world.set({0,60,0},Block::Lantern);
    check(!clear(world,light), "An opaque emitting voxel is never ignored without a proof");
    world.set({0,60,0},Block::Air);
    check(clear(world,light), "Removing an obstacle restores shadow clearance on the next rebuild");
    world.set({2,62,2},Block::Stone);
    const PointLight smaller{{.5f,60.5f,.5f},{1,1,1},2};
    check(clear(world,smaller), "A box-corner obstacle outside the actual sphere is harmless");
    world.set({2,62,2},Block::Air);
    world.set({2,61,1},Block::Stone);
    check(!clear(world,smaller), "A diagonal obstacle inside the sphere requires tracing");
    world.set({2,61,1},Block::Air);

    for (const auto block : {Block::Water,Block::Leaves,Block::GlowLeaves,Block::BlossomLeaves}) {
        world.set({0,60,0},block);
        check(clear(world,light), "Water and cutout foliage retain the shader's non-blocking occupancy");
    }
    for (int facing = 0; facing < 4; ++facing) {
        world.set({0,60,0},stairBlock(Block::Planks,facing));
        check(!clear(world,light), "Every stair facing remains a potential shadow blocker");
    }
    world.set({0,60,0},Block::Air);

    const std::array<PointLight,3> sources{{light,{{-5.5f,60.5f,0},{.2f,.5f,1},1},{{20,60,20},{1,1,1},2}}};
    world.set({-5,60,0},Block::Stone);
    check(unoccludedLightSources(world,sources) == std::vector<std::uint8_t>({1,0,1}),
          "Flags preserve global light order and each source's individual sphere");
    world.set({-5,60,0},Block::Air);
    check(unoccludedLightSources(world,sources) == std::vector<std::uint8_t>({1,1,1}),
          "World edits do not leave stale clear or blocked decisions inside the helper");
}

void worldEdges(World& world)
{
    for (int axis = 0; axis < 3; ++axis) {
        for (bool upper : {false,true}) {
            glm::ivec3 cell{0,80,0};
            cell[axis] = upper ? World::MaxExclusive[axis] - 1 : World::Min[axis];
            glm::vec3 position = glm::vec3(cell) + .5f;
            position[axis] = upper ? float(World::MaxExclusive[axis]) + .4f : float(World::Min[axis]) - .4f;
            const PointLight light{position,{1,1,1},.5f};
            check(clear(world,light), "Sources on every world boundary can be clear");
            world.set(cell,Block::Stone);
            check(!clear(world,light), "A source outside the world still sees edge voxels inside its sphere");
            auto farther = light;
            farther.position[axis] += upper ? 3 : -3;
            check(clear(world,farther), "A wholly outside sphere sees air, matching shader occupancy");
            world.set(cell,Block::Air);
        }
    }
    const PointLight negative{{-8.5f,60.5f,-4.5f},{1,1,1},1};
    world.set({-10,60,-5},Block::Stone);
    check(!clear(world,negative), "Negative coordinates use voxel floors instead of truncation");
    world.set({-10,60,-5},Block::Air);
    check(clear(world,negative), "Negative-coordinate sources recover after an edit");
}

void invalidSources(const World& world)
{
    check(unoccludedLightSources(world,{}).empty(), "An empty source list produces no flags");
    const PointLight base{{.5f,60.5f,.5f},{1,1,1},1};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (const float radius : {0.f,-1.f,nan,infinity,16.f,1e30f}) {
        auto light = base;
        light.radius = radius;
        check(!clear(world,light), "Malformed or out-of-DDA-range radii conservatively retain tracing");
    }
    for (int axis = 0; axis < 3; ++axis) {
        for (const float value : {nan,infinity,-infinity}) {
            auto light = base;
            light.position[axis] = value;
            check(!clear(world,light), "Invalid positions never produce a shadow-free flag");
        }
        for (const float value : {-1.f,nan,infinity}) {
            auto light = base;
            light.color[axis] = value;
            check(!clear(world,light), "Invalid colors follow the safe normal rendering path");
        }
    }
    auto distant = base;
    distant.position = glm::vec3(1e30f);
    check(clear(world,distant), "Large finite outside coordinates never overflow voxel conversion");
    auto nearLimit = base;
    nearLimit.radius = 15;
    check(clear(world,nearLimit), "The largest supported light still receives the exact optimization");
}

} // namespace

int main()
{
    World world;
    for (int z = World::Min.z; z < World::MaxExclusive.z; ++z)
        for (int y = World::Min.y; y < World::MaxExclusive.y; ++y)
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x)
                world.set({x,y,z},Block::Air);
    sourcesAndEdits(world);
    worldEdges(world);
    invalidSources(world);
    if (failures) return 1;
    std::cout << "Light shadows: exact clearance, offsets, materials, world edges and edits passed\n";
}
