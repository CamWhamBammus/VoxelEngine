#include "ores.h"

#include <array>

namespace voxel {
namespace {
constexpr std::array Ores{
    OreInfo{Block::CoalOre, ItemId::Coal, 1, .70f},
    OreInfo{Block::CopperOre, ItemId::CopperOre, 2, .90f},
    OreInfo{Block::TinOre, ItemId::TinOre, 3, .80f},
    OreInfo{Block::IronOre, ItemId::IronOre, 4, 1.10f},
    OreInfo{Block::SilverOre, ItemId::SilverOre, 7, 1.20f},
    OreInfo{Block::GoldOre, ItemId::GoldOre, 8, 1.25f},
    OreInfo{Block::QuartzOre, ItemId::Quartz, 5, .90f},
    OreInfo{Block::AmberCrystal, ItemId::Amber, 6, .85f},
    OreInfo{Block::JadeOre, ItemId::Jade, 9, 1.15f},
    OreInfo{Block::AmethystOre, ItemId::Amethyst, 10, 1.25f},
    OreInfo{Block::RubyOre, ItemId::Ruby, 14, 1.45f},
    OreInfo{Block::SapphireOre, ItemId::Sapphire, 15, 1.45f},
    OreInfo{Block::EmeraldOre, ItemId::Emerald, 16, 1.45f},
    OreInfo{Block::CobaltOre, ItemId::CobaltOre, 12, 1.35f},
    OreInfo{Block::TitaniumOre, ItemId::TitaniumOre, 13, 1.55f},
    OreInfo{Block::MithrilOre, ItemId::MithrilOre, 17, 1.70f},
    OreInfo{Block::Obsidian, ItemId::Obsidian, 11, 1.50f},
    OreInfo{Block::Crystal, ItemId::GlowCrystal, 18, 1.55f}
};
} // namespace

std::span<const OreInfo> oreCatalog() { return Ores; }

const OreInfo* oreInfo(Block block)
{
    for (const auto& ore : Ores) if (ore.block == block) return &ore;
    return nullptr;
}

} // namespace voxel
