#include "voxel/world.h"

#include "voxel/terrain.h"

#include <optional>
#include <stdexcept>

namespace voxel {
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

World::World(std::uint32_t seed)
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

MeshData World::mesh(std::size_t index) const
{
    return buildMesh(chunk(index), origin(index), [this](glm::ivec3 position) {
        return get(position);
    });
}

} // namespace voxel
