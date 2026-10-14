#include "voxel/chunk.h"

#include <iostream>
#include <limits>

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            ++failures;
        }
    };

    voxel::Chunk chunk;
    for (int y = 0; y < voxel::Chunk::Size; ++y) {
        for (int z = 0; z < voxel::Chunk::Size; ++z) {
            for (int x = 0; x < voxel::Chunk::Size; ++x) {
                check(chunk.get(x, y, z) == voxel::Block::Air, "new chunk must be empty");
                const auto block = static_cast<voxel::Block>(1 + (x + 2 * y + 3 * z) % 4);
                check(chunk.set(x, y, z, block), "valid write must succeed");
            }
        }
    }
    for (int y = 0; y < voxel::Chunk::Size; ++y) {
        for (int z = 0; z < voxel::Chunk::Size; ++z) {
            for (int x = 0; x < voxel::Chunk::Size; ++x) {
                const auto block = static_cast<voxel::Block>(1 + (x + 2 * y + 3 * z) % 4);
                check(chunk.get(x, y, z) == block, "voxel coordinates must not alias");
            }
        }
    }
    for (const int coordinate : {-1, voxel::Chunk::Size, std::numeric_limits<int>::min(),
                                  std::numeric_limits<int>::max()}) {
        check(chunk.get(coordinate, 0, 0) == voxel::Block::Air, "out-of-range x read");
        check(chunk.get(0, coordinate, 0) == voxel::Block::Air, "out-of-range y read");
        check(chunk.get(0, 0, coordinate) == voxel::Block::Air, "out-of-range z read");
        check(!chunk.set(coordinate, 0, 0, voxel::Block::Air), "out-of-range x write");
        check(!chunk.set(0, coordinate, 0, voxel::Block::Air), "out-of-range y write");
        check(!chunk.set(0, 0, coordinate, voxel::Block::Air), "out-of-range z write");
    }
    check(chunk.get(0, 0, 0) == voxel::Block::Grass, "invalid writes must preserve storage");
    check(chunk.set(15, 15, 15, voxel::Block::Air), "last voxel must be writable");
    check(chunk.get(15, 15, 15) == voxel::Block::Air, "voxel removal must work");
    check(!voxel::isSolid(voxel::Block::Air), "air must not be solid");
    for (const auto block : {voxel::Block::Grass, voxel::Block::Dirt,
                             voxel::Block::Stone, voxel::Block::Sand}) {
        check(voxel::isSolid(block), "materials must be solid");
        const auto color = voxel::blockColor(block);
        check(color.x >= 0 && color.x <= 1 && color.y >= 0 && color.y <= 1
              && color.z >= 0 && color.z <= 1, "material color must be normalized");
    }
    return failures == 0 ? 0 : 1;
}
