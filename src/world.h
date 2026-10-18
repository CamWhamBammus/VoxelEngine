#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
#include <glm/vec3.hpp>

namespace voxel {

// IDs are stored directly in save files. Append materials without renumbering.
enum class Block : std::uint8_t {
    Air = 0, Grass = 1, Dirt = 2, Stone = 3, Sand = 4,
    Limestone = 5, Slate = 6, CoalOre = 7, CopperOre = 8, IronOre = 9,
    Crystal = 10, Timber = 11, Planks = 12, Brick = 13, Lantern = 14, Bedrock = 15,
    Water = 16, Leaves = 17, GlowLeaves = 18, RoofTiles = 19, Path = 20
};

inline constexpr std::size_t BlockCount = 21;
inline constexpr std::array PlaceableBlocks{
    Block::Grass, Block::Dirt, Block::Stone, Block::Sand,
    Block::Limestone, Block::Slate, Block::CoalOre, Block::CopperOre, Block::IronOre,
    Block::Crystal, Block::Timber, Block::Planks, Block::Brick, Block::Lantern,
    Block::Water, Block::Leaves, Block::GlowLeaves, Block::RoofTiles, Block::Path
};

constexpr bool isValidBlock(Block block) noexcept
{
    return static_cast<std::uint8_t>(block) < BlockCount;
}

constexpr std::string_view blockName(Block block) noexcept
{
    switch (block) {
    case Block::Air: return "Air";
    case Block::Grass: return "Grass";
    case Block::Dirt: return "Dirt";
    case Block::Stone: return "Stone";
    case Block::Sand: return "Sand";
    case Block::Limestone: return "Limestone";
    case Block::Slate: return "Slate";
    case Block::CoalOre: return "Coal ore";
    case Block::CopperOre: return "Copper ore";
    case Block::IronOre: return "Iron ore";
    case Block::Crystal: return "Crystal";
    case Block::Timber: return "Timber";
    case Block::Planks: return "Planks";
    case Block::Brick: return "Brick";
    case Block::Lantern: return "Lantern";
    case Block::Bedrock: return "Bedrock";
    case Block::Water: return "Water";
    case Block::Leaves: return "Leaves";
    case Block::GlowLeaves: return "Glow leaves";
    case Block::RoofTiles: return "Roof tiles";
    case Block::Path: return "Fitted paving";
    }
    return "Unknown";
}

constexpr bool isSolid(Block block) noexcept
{
    return isValidBlock(block) && block != Block::Air && block != Block::Water;
}

constexpr bool isRenderable(Block block) noexcept
{
    return isValidBlock(block) && block != Block::Air;
}

// Water has an opaque stylized surface but no collision. Solid banks remain
// visible beside it; water hides only other water faces, avoiding internal seams.
constexpr bool occludes(Block adjacent, Block faceMaterial) noexcept
{
    return isSolid(adjacent) || (adjacent == Block::Water && faceMaterial == Block::Water);
}

inline glm::vec3 blockColor(Block block) noexcept
{
    switch (block) {
    case Block::Grass: return {0.32f, 0.66f, 0.22f};
    case Block::Dirt: return {0.46f, 0.29f, 0.16f};
    case Block::Stone: return {0.51f, 0.53f, 0.57f};
    case Block::Sand: return {0.85f, 0.77f, 0.50f};
    case Block::Limestone: return {0.66f, 0.69f, 0.65f};
    case Block::Slate: return {0.29f, 0.36f, 0.43f};
    case Block::CoalOre: return {0.30f, 0.33f, 0.36f};
    case Block::CopperOre: return {0.57f, 0.38f, 0.27f};
    case Block::IronOre: return {0.48f, 0.46f, 0.42f};
    case Block::Crystal: return {0.35f, 0.83f, 0.90f};
    case Block::Timber: return {0.36f, 0.23f, 0.13f};
    case Block::Planks: return {0.67f, 0.46f, 0.25f};
    case Block::Brick: return {0.61f, 0.30f, 0.22f};
    case Block::Lantern: return {1.0f, 0.74f, 0.30f};
    case Block::Bedrock: return {0.16f, 0.20f, 0.24f};
    case Block::Water: return {0.08f, 0.45f, 0.61f};
    case Block::Leaves: return {0.22f, 0.52f, 0.23f};
    case Block::GlowLeaves: return {0.23f, 0.76f, 0.57f};
    case Block::RoofTiles: return {0.44f, 0.29f, 0.50f};
    case Block::Path: return {0.32f, 0.33f, 0.28f};
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

// A fixed grid of chunks, indexed with X fastest, then Z, then vertical Y.
class World {
public:
    static constexpr int Radius = 7;
    static constexpr int Width = 2 * Radius + 1;
    static constexpr int Height = 6;
    static constexpr std::size_t ChunkCount = Width * Width * Height;
    static constexpr glm::ivec3 Min{-Radius * Chunk::Size, 0, -Radius * Chunk::Size};
    static constexpr glm::ivec3 MaxExclusive{(Radius + 1) * Chunk::Size, Height * Chunk::Size,
                                            (Radius + 1) * Chunk::Size};

    // Bounds describe editable cells, including air. The upper edge is excluded.
    static constexpr bool contains(glm::ivec3 position) noexcept
    {
        return position.x >= Min.x && position.x < MaxExclusive.x
            && position.y >= Min.y && position.y < MaxExclusive.y
            && position.z >= Min.z && position.z < MaxExclusive.z;
    }

    explicit World(std::uint32_t seed = 1337);
    // Change-tracking flags are not part of the saved world contents.
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
    // Returns and clears changed chunks, including their affected seam neighbors.
    // Renderers track their own snapshots, so consuming this list cannot hide edits.
    std::vector<std::size_t> takeDirtyChunks();

private:
    std::uint32_t seed_;
    std::vector<Chunk> chunks_;
    std::array<bool, ChunkCount> dirty_{};
};

} // namespace voxel
