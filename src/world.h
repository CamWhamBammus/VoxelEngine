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
    Water = 16, Leaves = 17, GlowLeaves = 18, RoofTiles = 19, Path = 20,
    MossStone = 21, MossBrick = 22, RootedDirt = 23, Clay = 24, Silt = 25,
    RootWood = 26, AgedPlanks = 27, Calcite = 28, Moss = 29, BlossomLeaves = 30,
    AmberCrystal = 31, GlowMoss = 32,
    PlankStairs = 33, PlankStairsWest, PlankStairsSouth, PlankStairsEast,
    StoneStairs = 37, StoneStairsWest, StoneStairsSouth, StoneStairsEast,
    BrickStairs = 41, BrickStairsWest, BrickStairsSouth, BrickStairsEast,
    LimestoneStairs = 45, LimestoneStairsWest, LimestoneStairsSouth, LimestoneStairsEast,
    SlateStairs = 49, SlateStairsWest, SlateStairsSouth, SlateStairsEast,
    MossStoneStairs = 53, MossStoneStairsWest, MossStoneStairsSouth, MossStoneStairsEast,
    PathStairs = 57, PathStairsWest, PathStairsSouth, PathStairsEast,
    AgedPlankStairs = 61, AgedPlankStairsWest, AgedPlankStairsSouth, AgedPlankStairsEast,
    TinOre = 65, SilverOre, GoldOre, QuartzOre, JadeOre, AmethystOre, RubyOre,
    SapphireOre, EmeraldOre, CobaltOre, TitaniumOre, MithrilOre, Obsidian
};

inline constexpr std::size_t BlockCount = 78;
inline constexpr std::array PlaceableBlocks{
    Block::Grass, Block::Dirt, Block::Stone, Block::Sand,
    Block::Limestone, Block::Slate, Block::CoalOre, Block::CopperOre, Block::IronOre,
    Block::Crystal, Block::Timber, Block::Planks, Block::Brick, Block::Lantern,
    Block::Water, Block::Leaves, Block::GlowLeaves, Block::RoofTiles, Block::Path,
    Block::MossStone, Block::MossBrick, Block::RootedDirt, Block::Clay, Block::Silt,
    Block::RootWood, Block::AgedPlanks, Block::Calcite, Block::Moss, Block::BlossomLeaves,
    Block::AmberCrystal, Block::GlowMoss,
    Block::TinOre, Block::SilverOre, Block::GoldOre, Block::QuartzOre, Block::JadeOre,
    Block::AmethystOre, Block::RubyOre, Block::SapphireOre, Block::EmeraldOre,
    Block::CobaltOre, Block::TitaniumOre, Block::MithrilOre, Block::Obsidian,
    Block::PlankStairs, Block::StoneStairs, Block::BrickStairs, Block::LimestoneStairs,
    Block::SlateStairs, Block::MossStoneStairs, Block::PathStairs, Block::AgedPlankStairs
};

constexpr bool isValidBlock(Block block) noexcept
{
    return static_cast<std::uint8_t>(block) < BlockCount;
}

constexpr bool isStair(Block block) noexcept
{
    return block >= Block::PlankStairs && block <= Block::AgedPlankStairsEast;
}

// Facing zero rises toward -Z. Each quarter turn maps +Z to +X.
constexpr int stairFacing(Block block) noexcept
{
    return isStair(block) ? (static_cast<int>(block) - static_cast<int>(Block::PlankStairs)) % 4 : 0;
}

constexpr Block stairMaterial(Block block) noexcept
{
    constexpr std::array materials{Block::Planks, Block::Stone, Block::Brick, Block::Limestone,
        Block::Slate, Block::MossStone, Block::Path, Block::AgedPlanks};
    return isStair(block)
        ? materials[(static_cast<int>(block) - static_cast<int>(Block::PlankStairs)) / 4] : block;
}

constexpr Block stairBlock(Block material, int facing) noexcept
{
    material = stairMaterial(material);
    constexpr std::array materials{Block::Planks, Block::Stone, Block::Brick, Block::Limestone,
        Block::Slate, Block::MossStone, Block::Path, Block::AgedPlanks};
    for (std::size_t i = 0; i < materials.size(); ++i) {
        if (materials[i] == material) {
            return static_cast<Block>(static_cast<int>(Block::PlankStairs) + static_cast<int>(i) * 4
                + (facing % 4 + 4) % 4);
        }
    }
    return material;
}

constexpr Block rotateBlock(Block block, int quarterTurns) noexcept
{
    return isStair(block) ? stairBlock(block, stairFacing(block) + quarterTurns % 4) : block;
}

constexpr glm::ivec3 stairUpDirection(Block block) noexcept
{
    constexpr std::array directions{glm::ivec3(0, 0, -1), glm::ivec3(-1, 0, 0),
        glm::ivec3(0, 0, 1), glm::ivec3(1, 0, 0)};
    return isStair(block) ? directions[stairFacing(block)] : glm::ivec3(0);
}

constexpr std::string_view blockName(Block block) noexcept
{
    if (isStair(block)) {
        constexpr std::array<std::string_view, 8> names{"Plank stairs", "Stone stairs", "Brick stairs",
            "Limestone stairs", "Slate stairs", "Mossy stone stairs", "Paving stairs", "Weathered plank stairs"};
        return names[(static_cast<int>(block) - static_cast<int>(Block::PlankStairs)) / 4];
    }
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
    case Block::MossStone: return "Mossy stone";
    case Block::MossBrick: return "Mossy ruins";
    case Block::RootedDirt: return "Rooted earth";
    case Block::Clay: return "Terracotta clay";
    case Block::Silt: return "River silt";
    case Block::RootWood: return "Ancient root wood";
    case Block::AgedPlanks: return "Weathered planks";
    case Block::Calcite: return "Pale calcite";
    case Block::Moss: return "Velvet moss";
    case Block::BlossomLeaves: return "Blossom leaves";
    case Block::AmberCrystal: return "Amber crystal";
    case Block::GlowMoss: return "Glow moss";
    case Block::TinOre: return "Tin ore";
    case Block::SilverOre: return "Silver ore";
    case Block::GoldOre: return "Gold ore";
    case Block::QuartzOre: return "Quartz ore";
    case Block::JadeOre: return "Jade ore";
    case Block::AmethystOre: return "Amethyst ore";
    case Block::RubyOre: return "Ruby ore";
    case Block::SapphireOre: return "Sapphire ore";
    case Block::EmeraldOre: return "Emerald ore";
    case Block::CobaltOre: return "Cobalt ore";
    case Block::TitaniumOre: return "Titanium ore";
    case Block::MithrilOre: return "Mithril ore";
    case Block::Obsidian: return "Obsidian";
    default: break;
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

constexpr bool isLeafBlock(Block block) noexcept
{
    return block == Block::Leaves || block == Block::GlowLeaves || block == Block::BlossomLeaves;
}

// Collision and visual opacity are separate: cutout foliage is still walkable,
// but cannot hide an adjacent trunk or cast a completely solid cube shadow.
constexpr bool isOpaque(Block block) noexcept
{
    return isSolid(block) && !isLeafBlock(block) && !isStair(block);
}

// Keep solid surfaces behind leaf gaps and water. Adjacent leaf cubes share
// one outer canopy shell rather than filling it with expensive internal faces.
constexpr bool occludes(Block adjacent, Block faceMaterial) noexcept
{
    return isOpaque(adjacent) || (isLeafBlock(adjacent) && isLeafBlock(faceMaterial))
        || (adjacent == Block::Water && faceMaterial == Block::Water);
}

inline glm::vec3 blockColor(Block block) noexcept
{
    switch (stairMaterial(block)) {
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
    case Block::MossStone: return {0.36f, 0.48f, 0.34f};
    case Block::MossBrick: return {0.43f, 0.48f, 0.35f};
    case Block::RootedDirt: return {0.42f, 0.29f, 0.17f};
    case Block::Clay: return {0.68f, 0.39f, 0.25f};
    case Block::Silt: return {0.37f, 0.40f, 0.34f};
    case Block::RootWood: return {0.39f, 0.29f, 0.18f};
    case Block::AgedPlanks: return {0.48f, 0.46f, 0.34f};
    case Block::Calcite: return {0.83f, 0.81f, 0.66f};
    case Block::Moss: return {0.32f, 0.57f, 0.19f};
    case Block::BlossomLeaves: return {0.70f, 0.31f, 0.50f};
    case Block::AmberCrystal: return {0.94f, 0.52f, 0.11f};
    case Block::GlowMoss: return {0.13f, 0.62f, 0.39f};
    case Block::TinOre: return {.58f, .65f, .63f};
    case Block::SilverOre: return {.68f, .73f, .80f};
    case Block::GoldOre: return {.78f, .58f, .22f};
    case Block::QuartzOre: return {.79f, .77f, .81f};
    case Block::JadeOre: return {.28f, .57f, .38f};
    case Block::AmethystOre: return {.52f, .31f, .69f};
    case Block::RubyOre: return {.70f, .20f, .28f};
    case Block::SapphireOre: return {.24f, .38f, .72f};
    case Block::EmeraldOre: return {.16f, .64f, .39f};
    case Block::CobaltOre: return {.25f, .43f, .62f};
    case Block::TitaniumOre: return {.47f, .60f, .70f};
    case Block::MithrilOre: return {.26f, .72f, .65f};
    case Block::Obsidian: return {.19f, .13f, .28f};
    case Block::Air: return {0.0f, 0.0f, 0.0f};
    default: break;
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
