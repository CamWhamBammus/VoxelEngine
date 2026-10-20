#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <glm/vec3.hpp>

namespace voxel {

// Values are saved in player profiles. Append new items before Count.
enum class ItemId {
    Coal, CopperOre, TinOre, IronOre, SilverOre, GoldOre, Quartz, Amber, Jade,
    Amethyst, Ruby, Sapphire, Emerald, CobaltOre, TitaniumOre, MithrilOre,
    Obsidian, GlowCrystal, Key, Count
};
inline constexpr int ItemCount = static_cast<int>(ItemId::Count);

struct ItemInfo {
    ItemId id;
    std::string_view name;
    std::string_view description;
    glm::vec3 color;
    std::string_view rarity;
};

struct ItemStack {
    ItemId item = ItemId::Coal;
    int count = 0;
    bool operator==(const ItemStack&) const = default;
};

bool validItem(ItemId item);
std::span<const ItemInfo> itemCatalog();
const ItemInfo& itemInfo(ItemId item);

inline constexpr int ChestSlotCount = 20;
inline constexpr int ChestStackLimit = 99;
bool validChestItems(std::span<const ItemStack> items);

// Only used when importing a version 1 section that had randomized presets.
// New sections store exact authored contents and never roll loot at runtime.
std::vector<ItemStack> legacyChestLoot(std::uint32_t variation, std::uint32_t seed);

} // namespace voxel
