#pragma once

#include "voxel/chunk.h"
#include "voxel/mesh.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace voxel {

class World {
public:
    static constexpr int Radius = 2;
    static constexpr int Width = 2 * Radius + 1;
    static constexpr std::size_t ChunkCount = Width * Width;

    explicit World(std::uint32_t seed = 1337);

    std::uint32_t seed() const noexcept { return seed_; }

    const Chunk& chunk(std::size_t index) const;
    glm::ivec3 origin(std::size_t index) const;
    Block get(glm::ivec3 worldPosition) const;
    // Returns true only when an existing voxel actually changes.
    bool set(glm::ivec3 worldPosition, Block block);
    std::vector<std::size_t> takeDirtyChunks();
    MeshData mesh(std::size_t index) const;

private:
    std::uint32_t seed_;
    std::array<Chunk, ChunkCount> chunks_;
    std::array<bool, ChunkCount> dirty_{};
};

} // namespace voxel
