#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
#include <glm/vec3.hpp>

namespace voxel {

enum class Block : std::uint8_t { Air, Grass, Dirt, Stone, Sand };

inline constexpr std::array PlaceableBlocks{Block::Grass, Block::Dirt, Block::Stone, Block::Sand};

constexpr bool isValidBlock(Block block) noexcept
{
    return static_cast<std::uint8_t>(block) <= static_cast<std::uint8_t>(Block::Sand);
}

constexpr std::string_view blockName(Block block) noexcept
{
    switch (block) {
    case Block::Air: return "Air";
    case Block::Grass: return "Grass";
    case Block::Dirt: return "Dirt";
    case Block::Stone: return "Stone";
    case Block::Sand: return "Sand";
    }
    return "Unknown";
}

constexpr bool isSolid(Block block) noexcept
{
    return isValidBlock(block) && block != Block::Air;
}

inline glm::vec3 blockColor(Block block) noexcept
{
    switch (block) {
    case Block::Grass: return {0.32f, 0.66f, 0.22f};
    case Block::Dirt: return {0.46f, 0.29f, 0.16f};
    case Block::Stone: return {0.51f, 0.53f, 0.57f};
    case Block::Sand: return {0.85f, 0.77f, 0.50f};
    case Block::Air: return {0.0f, 0.0f, 0.0f};
    }
    return {1.0f, 0.0f, 1.0f};
}

// A chunk stores local coordinates [0, 15] on each axis. World coordinates
// locate chunks and voxels across the entire terrain.
class Chunk {
public:
    static constexpr int Size = 16;
    bool operator==(const Chunk&) const = default;

    static constexpr bool inBounds(int x, int y, int z) noexcept
    {
        return x >= 0 && x < Size && y >= 0 && y < Size && z >= 0 && z < Size;
    }

    Block get(int x, int y, int z) const noexcept;
    bool set(int x, int y, int z, Block block) noexcept;

private:
    static constexpr std::size_t index(int x, int y, int z) noexcept
    {
        // Flatten the cube into one array: x varies fastest, then z, then y.
        return static_cast<std::size_t>(x + Size * (z + Size * y));
    }

    std::array<Block, Size * Size * Size> blocks_{};
};

// Highest solid voxel's world Y coordinate, in the inclusive range [4, 13].
int terrainHeight(int worldX, int worldZ, std::uint32_t seed);

// Replaces every voxel, using world coordinates so adjacent chunks agree.
// Throws std::out_of_range before changing the chunk if any voxel coordinate
// from origin through origin + Chunk::Size - 1 cannot be represented by an int.
void generateTerrain(Chunk& chunk, glm::ivec3 origin, std::uint32_t seed);

// A fixed 5-by-5 grid of chunks, one chunk high.
class World {
public:
    static constexpr int Radius = 2;
    static constexpr int Width = 2 * Radius + 1;
    static constexpr std::size_t ChunkCount = Width * Width;
    static constexpr glm::ivec3 Min{-Radius * Chunk::Size, 0, -Radius * Chunk::Size};
    static constexpr glm::ivec3 MaxExclusive{(Radius + 1) * Chunk::Size, Chunk::Size,
                                            (Radius + 1) * Chunk::Size};

    // Bounds describe editable cells, including air. The upper edge is excluded.
    static constexpr bool contains(glm::ivec3 position) noexcept
    {
        return position.x >= Min.x && position.x < MaxExclusive.x
            && position.y >= Min.y && position.y < MaxExclusive.y
            && position.z >= Min.z && position.z < MaxExclusive.z;
    }

    explicit World(std::uint32_t seed = 1337);
    // Renderer dirty flags are not part of the saved world contents.
    bool sameContents(const World& other) const
    {
        return seed_ == other.seed_ && chunks_ == other.chunks_;
    }

    std::uint32_t seed() const noexcept { return seed_; }

    const Chunk& chunk(std::size_t index) const;
    glm::ivec3 origin(std::size_t index) const;
    Block get(glm::ivec3 worldPosition) const;
    // Returns true only when an existing voxel actually changes.
    bool set(glm::ivec3 worldPosition, Block block);
    // Returns and clears the chunks whose meshes must be rebuilt after edits.
    std::vector<std::size_t> takeDirtyChunks();

private:
    std::uint32_t seed_;
    std::array<Chunk, ChunkCount> chunks_;
    std::array<bool, ChunkCount> dirty_{};
};

} // namespace voxel
