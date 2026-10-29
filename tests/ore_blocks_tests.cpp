#include "ores.h"
#include "block_shapes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main()
{
    using namespace voxel;
    // These are persisted values from before mining was added.
    static_assert(static_cast<int>(Block::CoalOre) == 7);
    static_assert(static_cast<int>(Block::Crystal) == 10);
    static_assert(static_cast<int>(Block::AmberCrystal) == 31);
    static_assert(static_cast<int>(Block::PlankStairs) == 33);
    static_assert(static_cast<int>(Block::AgedPlankStairsEast) == 64);
    static_assert(static_cast<int>(Block::TinOre) == 65);
    static_assert(static_cast<int>(Block::Obsidian) == 77);
    constexpr std::array expectedBlocks{
        Block::CoalOre, Block::CopperOre, Block::TinOre, Block::IronOre,
        Block::SilverOre, Block::GoldOre, Block::QuartzOre, Block::AmberCrystal,
        Block::JadeOre, Block::AmethystOre, Block::RubyOre, Block::SapphireOre,
        Block::EmeraldOre, Block::CobaltOre, Block::TitaniumOre, Block::MithrilOre,
        Block::Obsidian, Block::Crystal
    };
    constexpr std::array expectedTiers{1, 2, 3, 4, 7, 8, 5, 6, 9, 10, 14, 15, 16, 12, 13, 17, 11, 18};
    check(oreCatalog().size() == expectedBlocks.size(), "all eighteen resources have mineable source blocks");
    std::array<int, ItemCount> itemSources{};
    std::array<int, BlockCount> blockSources{};
    for (const auto& ore : oreCatalog()) {
        check(isValidBlock(ore.block) && validItem(ore.item), "ore entries use valid block and item IDs");
        if (!isValidBlock(ore.block) || !validItem(ore.item)) continue;
        const auto itemIndex = static_cast<std::size_t>(ore.item);
        ++itemSources[itemIndex];
        ++blockSources[static_cast<std::size_t>(ore.block)];
        check(ore.item != ItemId::Key, "keys cannot be mined from blocks");
        if (itemIndex < expectedBlocks.size()) {
            check(ore.block == expectedBlocks[itemIndex], "each block drops its named resource rather than a neighboring enum value");
            check(ore.requiredTier == expectedTiers[itemIndex], "resource depths use the intended pickaxe tier");
        }
        check(oreInfo(ore.block) == &ore, "lookup returns the stable catalog entry");
        check(std::isfinite(ore.mineSeconds) && ore.mineSeconds >= .5f && ore.mineSeconds <= 2.0f,
              "mining durations are positive and short enough for one ore block");
        check(std::count(PlaceableBlocks.begin(), PlaceableBlocks.end(), ore.block) == 1,
              "every ore appears exactly once in the creative block palette");
        check(blockName(ore.block) != "Unknown" && blockName(ore.block) != "Air", "all ore blocks have readable names");
        const auto color = blockColor(ore.block);
        check(color != glm::vec3(1, 0, 1) && color != glm::vec3(0), "all ore blocks have real preview colors");
        for (int axis = 0; axis < 3; ++axis)
            check(std::isfinite(color[axis]) && color[axis] >= 0 && color[axis] <= 1, "ore colors are finite normalized RGB");
        const auto shape = blockShape(ore.block);
        check(isSolid(ore.block) && isOpaque(ore.block) && !isStair(ore.block), "appended ores remain solid full blocks, never stairs");
        check(shape.count == 1 && shape.boxes[0].minimum == glm::vec3(0)
            && shape.boxes[0].maximum == glm::vec3(1), "ore collision fills precisely one block cell");
        check(stairMaterial(ore.block) == ore.block && rotateBlock(ore.block, 3) == ore.block,
              "stair helpers preserve ore material IDs");
    }
    for (int i = 0; i < ItemCount; ++i)
        check(itemSources[i] == (i == static_cast<int>(ItemId::Key) ? 0 : 1), "every resource has exactly one source and keys have none");
    for (std::size_t i = 0; i < BlockCount; ++i) {
        check(blockSources[i] <= 1, "no block drops ambiguous resources");
        if (!blockSources[i]) check(oreInfo(static_cast<Block>(i)) == nullptr, "ordinary terrain and stairs are not mineable ores");
    }
    check(oreInfo(static_cast<Block>(255)) == nullptr, "unknown IDs are rejected safely");
    check(!isValidBlock(static_cast<Block>(BlockCount)), "the byte immediately after appended ores remains invalid");
    return failures ? 1 : 0;
}
