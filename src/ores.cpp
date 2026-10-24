#include "ores.h"

#include <array>

namespace voxel {
namespace {
constexpr std::array Ores{
    OreInfo{Block::CoalOre, ItemId::Coal, 1, .70f},
    OreInfo{Block::CopperOre, ItemId::CopperOre, 1, .90f},
    OreInfo{Block::TinOre, ItemId::TinOre, 1, .80f},
    OreInfo{Block::IronOre, ItemId::IronOre, 1, 1.10f},
    OreInfo{Block::SilverOre, ItemId::SilverOre, 2, 1.20f},
    OreInfo{Block::GoldOre, ItemId::GoldOre, 2, 1.25f},
    OreInfo{Block::QuartzOre, ItemId::Quartz, 1, .90f},
    OreInfo{Block::AmberCrystal, ItemId::Amber, 1, .85f},
    OreInfo{Block::JadeOre, ItemId::Jade, 2, 1.15f},
    OreInfo{Block::AmethystOre, ItemId::Amethyst, 2, 1.25f},
    OreInfo{Block::RubyOre, ItemId::Ruby, 3, 1.45f},
    OreInfo{Block::SapphireOre, ItemId::Sapphire, 3, 1.45f},
    OreInfo{Block::EmeraldOre, ItemId::Emerald, 3, 1.45f},
    OreInfo{Block::CobaltOre, ItemId::CobaltOre, 3, 1.35f},
    OreInfo{Block::TitaniumOre, ItemId::TitaniumOre, 3, 1.55f},
    OreInfo{Block::MithrilOre, ItemId::MithrilOre, 3, 1.70f},
    OreInfo{Block::Obsidian, ItemId::Obsidian, 2, 1.50f},
    OreInfo{Block::Crystal, ItemId::GlowCrystal, 3, 1.55f}
};
} // namespace

std::span<const OreInfo> oreCatalog() { return Ores; }

const OreInfo* oreInfo(Block block)
{
    for (const auto& ore : Ores) if (ore.block == block) return &ore;
    return nullptr;
}

} // namespace voxel
