#include "items.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace voxel {
namespace {
const std::array<ItemInfo, ItemCount> Items{{
    {ItemId::Coal, "Coal", "A dense black mineral used to fuel a forge.", {0.27f, 0.31f, 0.37f}, "Common"},
    {ItemId::CopperOre, "Copper Ore", "Warm orange metal for early tool upgrades.", {0.91f, 0.49f, 0.24f}, "Common"},
    {ItemId::TinOre, "Tin Ore", "A pale, soft metal found in shallow caves.", {0.70f, 0.79f, 0.79f}, "Common"},
    {ItemId::IronOre, "Iron Ore", "A sturdy metal for stronger mining tools.", {0.64f, 0.67f, 0.75f}, "Common"},
    {ItemId::SilverOre, "Silver Ore", "Bright silver seams hidden in cool stone.", {0.83f, 0.91f, 0.97f}, "Uncommon"},
    {ItemId::GoldOre, "Gold Ore", "Heavy golden nuggets with a warm gleam.", {1.00f, 0.78f, 0.20f}, "Uncommon"},
    {ItemId::Quartz, "Quartz", "Clear crystals growing around underground streams.", {0.91f, 0.94f, 1.00f}, "Common"},
    {ItemId::Amber, "Amber", "Ancient tree resin hardened into a honey-colored gem.", {1.00f, 0.58f, 0.12f}, "Uncommon"},
    {ItemId::Jade, "Jade", "Smooth green stone found among old roots.", {0.26f, 0.78f, 0.54f}, "Uncommon"},
    {ItemId::Amethyst, "Amethyst", "Violet crystals lining quiet cave pockets.", {0.71f, 0.40f, 0.98f}, "Uncommon"},
    {ItemId::Ruby, "Ruby", "A vivid crimson gemstone.", {0.96f, 0.22f, 0.36f}, "Rare"},
    {ItemId::Sapphire, "Sapphire", "A deep blue gemstone with a cold sparkle.", {0.25f, 0.48f, 1.00f}, "Rare"},
    {ItemId::Emerald, "Emerald", "A brilliant green gemstone from lush depths.", {0.18f, 0.95f, 0.49f}, "Rare"},
    {ItemId::CobaltOre, "Cobalt Ore", "Blue metal resting beneath the shallow seams.", {0.22f, 0.51f, 0.89f}, "Rare"},
    {ItemId::TitaniumOre, "Titanium Ore", "A light, tough metal with a cool silver sheen.", {0.59f, 0.78f, 0.91f}, "Rare"},
    {ItemId::MithrilOre, "Mithril Ore", "A rare turquoise metal from the deepest veins.", {0.31f, 1.00f, 0.88f}, "Epic"},
    {ItemId::Obsidian, "Obsidian", "Dark volcanic glass with violet edges.", {0.36f, 0.25f, 0.52f}, "Uncommon"},
    {ItemId::GlowCrystal, "Glow Crystal", "A luminous crystal carrying the cave's blue light.", {0.29f, 0.91f, 1.00f}, "Rare"},
    {ItemId::Key, "Cave key", "Opens one ironbound chest in this section. Unused keys stay behind when you leave.", {0.96f, 0.75f, 0.26f}, "Common"}
}};

std::uint32_t nextRandom(std::uint32_t& state)
{
    // Fixed arithmetic keeps a chest's contents stable across platforms.
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
} // namespace

bool validItem(ItemId item)
{
    return static_cast<int>(item) >= 0 && static_cast<int>(item) < ItemCount;
}

std::span<const ItemInfo> itemCatalog() { return Items; }

const ItemInfo& itemInfo(ItemId item)
{
    if (!validItem(item)) throw std::invalid_argument("Unknown item");
    return Items[static_cast<std::size_t>(item)];
}

bool validChestItems(std::span<const ItemStack> items)
{
    return items.size() <= ChestSlotCount
        && std::all_of(items.begin(), items.end(), [](const ItemStack& stack) {
            return validItem(stack.item) && stack.count >= 1 && stack.count <= ChestStackLimit;
        });
}

std::vector<ItemStack> legacyChestLoot(std::uint32_t variation, std::uint32_t seed)
{
    std::vector<ItemId> pool;
    switch (variation % 3) {
    case 0:
        pool = {ItemId::Coal, ItemId::CopperOre, ItemId::TinOre, ItemId::IronOre,
                ItemId::SilverOre, ItemId::GoldOre, ItemId::Quartz};
        break;
    case 1:
        pool = {ItemId::Quartz, ItemId::Amber, ItemId::Jade, ItemId::Amethyst,
                ItemId::Ruby, ItemId::Sapphire, ItemId::Emerald, ItemId::GlowCrystal};
        break;
    default:
        pool = {ItemId::IronOre, ItemId::GoldOre, ItemId::CobaltOre, ItemId::TitaniumOre,
                ItemId::MithrilOre, ItemId::Obsidian, ItemId::GlowCrystal};
        break;
    }
    std::uint32_t state = seed ^ (variation * 0x9e3779b9u) ^ 0xa511e9b3u;
    if (state == 0) state = 1;
    for (std::size_t i = pool.size(); i > 1; --i)
        std::swap(pool[i - 1], pool[nextRandom(state) % i]);
    const auto stackCount = 4 + nextRandom(state) % 3;
    std::vector<ItemStack> loot;
    loot.reserve(stackCount);
    for (std::uint32_t i = 0; i < stackCount; ++i) {
        const auto& info = itemInfo(pool[i]);
        const int count = info.rarity == "Epic" ? 1 + nextRandom(state) % 2
                        : info.rarity == "Rare" ? 1 + nextRandom(state) % 4
                        : 3 + nextRandom(state) % 8;
        loot.push_back({pool[i], count});
    }
    return loot;
}

} // namespace voxel
