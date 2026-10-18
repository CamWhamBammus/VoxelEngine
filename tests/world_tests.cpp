#include "mesh.h"
#include "world.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

voxel::Block differentBlock(voxel::Block block)
{
    return block == voxel::Block::Stone ? voxel::Block::Air : voxel::Block::Stone;
}
} // namespace

int main()
{
    using voxel::Block;
    using voxel::Chunk;
    using voxel::World;
    World world;

    check(World::Min == glm::ivec3(-64, 0, -64)
          && World::MaxExclusive == glm::ivec3(80, 64, 80), "world bounds describe the entire chunk grid");
    for (const int x : {World::Min.x, World::MaxExclusive.x - 1}) {
        for (const int y : {World::Min.y, World::MaxExclusive.y - 1}) {
            for (const int z : {World::Min.z, World::MaxExclusive.z - 1}) {
                check(World::contains({x, y, z}), "all eight boundary cells are editable");
            }
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        glm::ivec3 outside(0);
        outside[axis] = World::Min[axis] - 1;
        check(!World::contains(outside), "cell below each minimum is outside");
        outside[axis] = World::MaxExclusive[axis];
        check(!World::contains(outside), "cell at each exclusive maximum is outside");
    }
    check(World::contains({1, 15, 1}) && world.get({1, 15, 1}) == Block::Air,
          "bounds distinguish editable air from cells outside the world");

    const auto initiallyDirty = world.takeDirtyChunks();
    check(initiallyDirty.size() == World::ChunkCount, "new world marks every chunk dirty");
    for (std::size_t index = 0; index < initiallyDirty.size(); ++index) {
        check(initiallyDirty[index] == index, "dirty chunks use stable ascending indices");
    }
    check(world.takeDirtyChunks().empty(), "taking dirty chunks clears them");
    check(world.origin(0) == glm::ivec3(-64, 0, -64), "first chunk origin");
    check(world.origin(40) == glm::ivec3(0), "center chunk origin");
    check(world.origin(World::ChunkCount - 1) == glm::ivec3(64, 48, 64), "last chunk origin");

    check(sizeof(World) < 4096, "world voxel storage lives on the heap rather than the stack");
    check(world.origin(81) == glm::ivec3(-64, 16, -64), "vertical layers follow each complete horizontal grid");
    check(world.get({0, 16, 0}) == Block::Air && world.get({0, 63, 0}) == Block::Air,
          "new upper layers start above generated terrain");

    World repeated;
    bool identical = true;
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        for (int y = 0; y < Chunk::Size; ++y) {
            for (int z = 0; z < Chunk::Size; ++z) {
                for (int x = 0; x < Chunk::Size; ++x) {
                    identical &= world.chunk(index).get(x, y, z)
                        == repeated.chunk(index).get(x, y, z);
                }
            }
        }
    }
    check(identical, "same seed generates the same world");

    // Each pair straddles a chunk boundary on the negative side of the world.
    for (const auto position : {glm::ivec3(-17, 7, -1), glm::ivec3(-16, 7, -1),
             glm::ivec3(-1, 7, -17), glm::ivec3(-1, 7, -16), glm::ivec3(0, 7, 0)}) {
        const Block replacement = differentBlock(world.get(position));
        check(world.set(position, replacement), "negative and zero coordinate writes succeed");
        check(world.get(position) == replacement, "world coordinates round trip across negative boundaries");
    }
    check(world.chunk(29).get(15, 7, 15) == world.get({-17, 7, -1}),
        "minus seventeen maps to chunk minus two");
    check(world.chunk(30).get(0, 7, 15) == world.get({-16, 7, -1}),
        "minus sixteen maps to chunk minus one");
    check(world.chunk(21).get(15, 7, 15) == world.get({-1, 7, -17}),
        "negative z uses floor division");
    world.takeDirtyChunks();

    const auto edit = [&world](glm::ivec3 position) {
        return world.set(position, differentBlock(world.get(position)));
    };
    edit({1, 7, 1});
    check(world.takeDirtyChunks() == std::vector<std::size_t>{40},
        "interior edit dirties only its chunk");
    edit({0, 7, 1});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({39, 40}),
        "negative x boundary dirties both neighbors");
    edit({15, 7, 1});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({40, 41}),
        "positive x boundary dirties both neighbors");
    edit({1, 7, 0});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({31, 40}),
        "negative z boundary dirties both neighbors");
    edit({1, 7, 15});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({40, 49}),
        "positive z boundary dirties both neighbors");
    edit({1, 15, 1});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({40, 121}),
        "upper y boundary dirties the chunk above");
    edit({1, 16, 1});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({40, 121}),
        "lower y boundary dirties the chunk below");
    edit({0, 16, 0});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({40, 112, 120, 121}),
        "three-dimensional corner dirties only its three face neighbors");
    edit({0, 7, 0});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({31, 39, 40}),
        "corner edit does not dirty diagonal neighbor");
    edit(World::Min);
    check(world.takeDirtyChunks() == std::vector<std::size_t>{0},
        "outer and vertical boundaries skip missing chunks");
    edit(World::MaxExclusive - glm::ivec3(1));
    edit(World::MaxExclusive - glm::ivec3(1));
    check(world.takeDirtyChunks() == std::vector<std::size_t>{World::ChunkCount - 1},
        "dirty indices are unique across repeated edits");

    check(!world.set({1, 7, 1}, world.get({1, 7, 1})), "no-op write returns false");
    for (const auto position : {glm::ivec3(-65, 0, 0), glm::ivec3(80, 0, 0),
             glm::ivec3(0, -1, 0), glm::ivec3(0, 64, 0),
             glm::ivec3(0, 0, -65), glm::ivec3(0, 0, 80),
             glm::ivec3(std::numeric_limits<int>::min()),
             glm::ivec3(std::numeric_limits<int>::max())}) {
        check(world.get(position) == Block::Air, "outside world reads return air");
        check(!World::contains(position), "bounds reject outside and extreme coordinates without overflow");
        check(!world.set(position, Block::Stone), "outside world writes return false");
    }
    check(world.takeDirtyChunks().empty(), "no-op and outside writes do not dirty chunks");

    // Clear the generated world to isolate exactly two solids on a chunk seam.
    for (int y = 0; y < World::MaxExclusive.y; ++y) {
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
                world.set({x, y, z}, Block::Air);
            }
        }
    }
    world.set({2, 15, 2}, Block::Stone);
    world.set({2, 16, 2}, Block::Stone);
    check(voxel::buildMesh(world, 40).indices.size() == 30 && voxel::buildMesh(world, 121).indices.size() == 30,
          "world mesh culls both faces at a vertical chunk seam");
    world.set({2, 15, 2}, Block::Air);
    world.set({2, 16, 2}, Block::Air);
    world.set({-1, 5, 1}, Block::Stone);
    world.set({0, 5, 1}, Block::Stone);
    const auto leftMesh = voxel::buildMesh(world, 39);
    const auto rightMesh = voxel::buildMesh(world, 40);
    check(leftMesh.indices.size() == 30 && rightMesh.indices.size() == 30,
        "world mesh culls both faces at negative chunk seam");
    check(voxel::buildMesh(world.chunk(39), world.origin(39)).indices.size() == 36,
        "isolated meshing retains the seam face");
    world.set({0, 5, 1}, Block::Air);
    check(voxel::buildMesh(world, 39).indices.size() == 36 && voxel::buildMesh(world, 40).indices.empty(),
        "removing a seam voxel exposes its neighbor face");

    bool rejectedInvalidIndex = false;
    try {
        world.origin(World::ChunkCount);
    } catch (const std::out_of_range&) {
        rejectedInvalidIndex = true;
    }
    check(rejectedInvalidIndex, "invalid chunk indices are rejected");
    if (failures == 0) {
        std::cout << "World tests passed\n";
    }
    voxel::World baseline;
    voxel::World changedWorld = baseline;
    changedWorld.takeDirtyChunks();
    check(changedWorld.sameContents(baseline), "renderer dirty flags do not affect saved contents");
    const auto originalBlock = changedWorld.get({0, 15, 0});
    changedWorld.set({0, 15, 0}, voxel::Block::Sand);
    check(!changedWorld.sameContents(baseline), "edits differ from saved contents");
    changedWorld.set({0, 15, 0}, originalBlock);
    check(changedWorld.sameContents(baseline), "undo back to saved contents clears dirty status");
    return failures == 0 ? 0 : 1;
}
