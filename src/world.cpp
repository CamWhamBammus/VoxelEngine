#include "world.h"

#include <cmath>
#include <optional>
#include <stdexcept>

namespace voxel {

Block Chunk::get(int x, int y, int z) const noexcept
{
    return inBounds(x, y, z) ? blocks_[index(x, y, z)] : Block::Air;
}

bool Chunk::set(int x, int y, int z, Block block) noexcept
{
    if (!inBounds(x, y, z) || !isValidBlock(block)) {
        return false;
    }
    blocks_[index(x, y, z)] = block;
    return true;
}

namespace {

std::uint32_t hash(std::int64_t x, std::int64_t z, std::uint32_t seed)
{
    // Mix coordinates and seed so the same location always gets the same value.
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
    // Blend the four surrounding grid samples to make smoothly varying hills.
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

namespace {

int chunkCoordinate(int coordinate)
{
    // Integer division truncates toward zero; chunks need floor division.
    return coordinate / Chunk::Size - (coordinate % Chunk::Size < 0 ? 1 : 0);
}

std::optional<std::size_t> chunkIndex(glm::ivec3 position)
{
    if (position.y < 0 || position.y >= Chunk::Size) {
        return std::nullopt;
    }
    const int x = chunkCoordinate(position.x);
    const int z = chunkCoordinate(position.z);
    if (x < -World::Radius || x > World::Radius
        || z < -World::Radius || z > World::Radius) {
        return std::nullopt;
    }
    return static_cast<std::size_t>((z + World::Radius) * World::Width + x + World::Radius);
}

} // namespace

World::World(std::uint32_t seed) : seed_(seed)
{
    for (std::size_t index = 0; index < ChunkCount; ++index) {
        generateTerrain(chunks_[index], origin(index), seed);
    }
    dirty_.fill(true);
}

const Chunk& World::chunk(std::size_t index) const
{
    return chunks_.at(index);
}

glm::ivec3 World::origin(std::size_t index) const
{
    if (index >= ChunkCount) {
        throw std::out_of_range("World chunk index is out of range");
    }
    const int x = static_cast<int>(index % Width) - Radius;
    const int z = static_cast<int>(index / Width) - Radius;
    return {x * Chunk::Size, 0, z * Chunk::Size};
}

Block World::get(glm::ivec3 worldPosition) const
{
    const auto index = chunkIndex(worldPosition);
    if (!index) {
        return Block::Air;
    }
    const glm::ivec3 local = worldPosition - origin(*index);
    return chunks_[*index].get(local.x, local.y, local.z);
}

bool World::set(glm::ivec3 worldPosition, Block block)
{
    if (!isValidBlock(block)) {
        return false;
    }
    const auto index = chunkIndex(worldPosition);
    if (!index) {
        return false;
    }
    const glm::ivec3 local = worldPosition - origin(*index);
    Chunk& target = chunks_[*index];
    if (target.get(local.x, local.y, local.z) == block) {
        return false;
    }
    target.set(local.x, local.y, local.z, block);
    dirty_[*index] = true;

    // A boundary edit can expose or hide a face in a neighboring chunk too.
    for (const glm::ivec3 direction : {glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
             glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0),
             glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)}) {
        const glm::ivec3 neighborLocal = local + direction;
        if (!Chunk::inBounds(neighborLocal.x, neighborLocal.y, neighborLocal.z)) {
            if (const auto neighbor = chunkIndex(worldPosition + direction)) {
                dirty_[*neighbor] = true;
            }
        }
    }
    return true;
}

std::vector<std::size_t> World::takeDirtyChunks()
{
    std::vector<std::size_t> dirty;
    for (std::size_t index = 0; index < ChunkCount; ++index) {
        if (dirty_[index]) {
            dirty.push_back(index);
            dirty_[index] = false;
        }
    }
    return dirty;
}

} // namespace voxel
