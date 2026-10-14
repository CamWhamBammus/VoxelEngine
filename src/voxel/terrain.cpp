#include "voxel/terrain.h"

#include <cmath>

namespace voxel {
namespace {

std::uint32_t hash(std::int64_t x, std::int64_t z, std::uint32_t seed)
{
    std::uint32_t value = seed ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u)
                               ^ (static_cast<std::uint32_t>(z) * 0x85ebca6bu);
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

std::int64_t floorDivide(int coordinate, int scale)
{
    const auto value = static_cast<std::int64_t>(coordinate);
    return value / scale - (value % scale < 0 ? 1 : 0);
}

double interpolate(double first, double second, double amount)
{
    return first + (second - first) * amount;
}

double noise(int x, int z, int scale, std::uint32_t seed)
{
    const auto cellX = floorDivide(x, scale);
    const auto cellZ = floorDivide(z, scale);
    double tx = static_cast<double>(static_cast<std::int64_t>(x) - cellX * scale) / scale;
    double tz = static_cast<double>(static_cast<std::int64_t>(z) - cellZ * scale) / scale;
    tx = tx * tx * (3.0 - 2.0 * tx);
    tz = tz * tz * (3.0 - 2.0 * tz);
    const auto sample = [seed](std::int64_t sx, std::int64_t sz) {
        return static_cast<double>(hash(sx, sz, seed)) / 4294967295.0;
    };
    return interpolate(interpolate(sample(cellX, cellZ), sample(cellX + 1, cellZ), tx),
                       interpolate(sample(cellX, cellZ + 1), sample(cellX + 1, cellZ + 1), tx), tz);
}

} // namespace

int terrainHeight(int worldX, int worldZ, std::uint32_t seed)
{
    const double elevation = 0.75 * noise(worldX, worldZ, 24, seed)
                           + 0.25 * noise(worldX, worldZ, 9, seed ^ 0xa511e9b3u);
    return 4 + static_cast<int>(std::lround(9.0 * elevation));
}

void generateTerrain(Chunk& chunk, glm::ivec3 origin, std::uint32_t seed)
{
    for (int z = 0; z < Chunk::Size; ++z) {
        for (int x = 0; x < Chunk::Size; ++x) {
            const int height = terrainHeight(origin.x + x, origin.z + z, seed);
            for (int y = 0; y < Chunk::Size; ++y) {
                const auto worldY = static_cast<std::int64_t>(origin.y) + y;
                Block block = Block::Air;
                if (worldY == height) {
                    block = height <= 6 ? Block::Sand : Block::Grass;
                } else if (worldY < height) {
                    block = worldY >= height - 2 ? Block::Dirt : Block::Stone;
                }
                chunk.set(x, y, z, block);
            }
        }
    }
}

} // namespace voxel
