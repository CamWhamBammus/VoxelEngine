#include "world.h"

#include <cmath>
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
    constexpr std::uint32_t seed = 12345;
    bool seedChangesTerrain = false;
    bool variedHeight = false;
    const int firstHeight = voxel::terrainHeight(0, 0, seed);
    for (int z = -64; z <= 64; ++z) {
        for (int x = -64; x <= 64; ++x) {
            const int height = voxel::terrainHeight(x, z, seed);
            check(height >= 4 && height <= 13, "terrain height must stay in the documented range");
            check(height == voxel::terrainHeight(x, z, seed), "same seed must reproduce heights");
            check(std::abs(height - voxel::terrainHeight(x + 1, z, seed)) <= 2,
                  "height field must remain smooth across positive and negative coordinates");
            seedChangesTerrain |= height != voxel::terrainHeight(x, z, seed + 1);
            variedHeight |= height != firstHeight;
        }
    }
    check(seedChangesTerrain, "changing the seed must change the terrain");
    check(variedHeight, "terrain must have elevation variation");
    for (const int x : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        for (const int z : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
            const int height = voxel::terrainHeight(x, z, seed);
            check(height >= 4 && height <= 13, "height sampling must handle integer boundaries");
        }
    }

    voxel::Chunk chunk;
    const glm::ivec3 origin(-8, 0, -8);
    voxel::generateTerrain(chunk, origin, seed);
    for (int z = 0; z < voxel::Chunk::Size; ++z) {
        for (int x = 0; x < voxel::Chunk::Size; ++x) {
            const int height = voxel::terrainHeight(origin.x + x, origin.z + z, seed);
            const auto surface = height <= 6 ? voxel::Block::Sand : voxel::Block::Grass;
            check(chunk.get(x, height, z) == surface, "terrain must apply its surface material");
            check(chunk.get(x, height - 1, z) == voxel::Block::Dirt, "soil must lie below surface");
            check(chunk.get(x, height - 2, z) == voxel::Block::Dirt, "soil must be two voxels deep");
            check(chunk.get(x, height - 3, z) == voxel::Block::Stone, "stone must lie below soil");
            for (int y = 0; y < voxel::Chunk::Size; ++y) {
                check(voxel::isSolid(chunk.get(x, y, z)) == (y <= height),
                      "column occupancy must match absolute terrain height");
            }
        }
    }

    voxel::Chunk shifted;
    const glm::ivec3 shift(3, 2, 4);
    voxel::generateTerrain(shifted, origin + shift, seed);
    for (int y = 0; y < voxel::Chunk::Size - shift.y; ++y) {
        for (int z = 0; z < voxel::Chunk::Size - shift.z; ++z) {
            for (int x = 0; x < voxel::Chunk::Size - shift.x; ++x) {
                check(shifted.get(x, y, z) == chunk.get(x + shift.x, y + shift.y, z + shift.z),
                      "overlapping chunks must agree at identical absolute coordinates");
            }
        }
    }

    // Regeneration must clear old solids above the new surface.
    voxel::generateTerrain(chunk, glm::ivec3(0, -16, 0), seed);
    check(chunk.get(0, 15, 0) == voxel::Block::Stone, "underground chunks must fill with stone");
    voxel::generateTerrain(chunk, glm::ivec3(0, 16, 0), seed);
    for (int y = 0; y < voxel::Chunk::Size; ++y) {
        for (int z = 0; z < voxel::Chunk::Size; ++z) {
            for (int x = 0; x < voxel::Chunk::Size; ++x) {
                check(chunk.get(x, y, z) == voxel::Block::Air,
                      "regenerating an above-ground chunk must clear every old voxel");
            }
        }
    }
    return failures == 0 ? 0 : 1;
}
