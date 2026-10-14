#include "voxel/world.h"

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

    const auto initiallyDirty = world.takeDirtyChunks();
    check(initiallyDirty.size() == World::ChunkCount, "new world marks every chunk dirty");
    for (std::size_t index = 0; index < initiallyDirty.size(); ++index) {
        check(initiallyDirty[index] == index, "dirty chunks use stable ascending indices");
    }
    check(world.takeDirtyChunks().empty(), "taking dirty chunks clears them");
    check(world.origin(0) == glm::ivec3(-32, 0, -32), "first chunk origin");
    check(world.origin(12) == glm::ivec3(0), "center chunk origin");
    check(world.origin(24) == glm::ivec3(32, 0, 32), "last chunk origin");

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
    check(world.chunk(5).get(15, 7, 15) == world.get({-17, 7, -1}),
        "minus seventeen maps to chunk minus two");
    check(world.chunk(6).get(0, 7, 15) == world.get({-16, 7, -1}),
        "minus sixteen maps to chunk minus one");
    check(world.chunk(1).get(15, 7, 15) == world.get({-1, 7, -17}),
        "negative z uses floor division");
    world.takeDirtyChunks();

    const auto edit = [&world](glm::ivec3 position) {
        return world.set(position, differentBlock(world.get(position)));
    };
    edit({1, 7, 1});
    check(world.takeDirtyChunks() == std::vector<std::size_t>{12},
        "interior edit dirties only its chunk");
    edit({0, 7, 1});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({11, 12}),
        "negative x boundary dirties both neighbors");
    edit({15, 7, 1});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({12, 13}),
        "positive x boundary dirties both neighbors");
    edit({1, 7, 0});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({7, 12}),
        "negative z boundary dirties both neighbors");
    edit({1, 7, 15});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({12, 17}),
        "positive z boundary dirties both neighbors");
    edit({0, 7, 0});
    check(world.takeDirtyChunks() == std::vector<std::size_t>({7, 11, 12}),
        "corner edit does not dirty diagonal neighbor");
    edit({-32, 0, -32});
    check(world.takeDirtyChunks() == std::vector<std::size_t>{0},
        "outer and vertical boundaries skip missing chunks");
    edit({47, 15, 47});
    edit({47, 15, 47});
    check(world.takeDirtyChunks() == std::vector<std::size_t>{24},
        "dirty indices are unique across repeated edits");

    check(!world.set({1, 7, 1}, world.get({1, 7, 1})), "no-op write returns false");
    for (const auto position : {glm::ivec3(-33, 0, 0), glm::ivec3(48, 0, 0),
             glm::ivec3(0, -1, 0), glm::ivec3(0, 16, 0),
             glm::ivec3(0, 0, -33), glm::ivec3(0, 0, 48),
             glm::ivec3(std::numeric_limits<int>::min()),
             glm::ivec3(std::numeric_limits<int>::max())}) {
        check(world.get(position) == Block::Air, "outside world reads return air");
        check(!world.set(position, Block::Stone), "outside world writes return false");
    }
    check(world.takeDirtyChunks().empty(), "no-op and outside writes do not dirty chunks");

    // Clear the generated world to isolate exactly two solids on a chunk seam.
    for (int y = 0; y < Chunk::Size; ++y) {
        for (int z = -32; z < 48; ++z) {
            for (int x = -32; x < 48; ++x) {
                world.set({x, y, z}, Block::Air);
            }
        }
    }
    world.set({-1, 5, 1}, Block::Stone);
    world.set({0, 5, 1}, Block::Stone);
    const auto leftMesh = world.mesh(11);
    const auto rightMesh = world.mesh(12);
    check(leftMesh.indices.size() == 30 && rightMesh.indices.size() == 30,
        "world mesh culls both faces at negative chunk seam");
    check(voxel::buildMesh(world.chunk(11), world.origin(11)).indices.size() == 36,
        "isolated meshing retains the seam face");
    world.set({0, 5, 1}, Block::Air);
    check(world.mesh(11).indices.size() == 36 && world.mesh(12).indices.empty(),
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
    return failures == 0 ? 0 : 1;
}
