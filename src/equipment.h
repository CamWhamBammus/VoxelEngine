#pragma once

#include "inventory.h"
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace voxel {

enum class Tool { Pickaxe, Lantern, Compass };
inline constexpr int ToolCount = 3;
std::string_view toolName(Tool tool);

struct LootProgress {
    bool unlocked = false;
    std::array<int, ChestSlotCount> taken{};
    bool operator==(const LootProgress&) const = default;
};

struct Equipment {
    int selectedSlot = 0;
    int pickaxeTier = 1;
    Inventory inventory;
    std::map<std::string, LootProgress> lootProgress;

    Equipment();
    Tool selectedTool() const;
    bool operator==(const Equipment&) const = default;
};

struct UpgradeCost { int copper = 0; int iron = 0; };
std::optional<UpgradeCost> pickaxeUpgradeCost(int tier);
enum class UpgradeResult { Upgraded, InsufficientResources, MaxTier };
UpgradeResult upgradePickaxe(Equipment& equipment);

// A versioned profile stores tier, ordinary inventory items and each chest's
// persistent loot state. Cave keys stay in the current section and are not saved.
// Legacy ore balances and inventory profiles migrate without losing ore.
Equipment loadEquipment(const std::filesystem::path& path);
// A unique sibling temporary file is flushed, then replaces the destination.
// Failure preserves the previous profile. Tool selection is session-only.
void saveEquipment(const Equipment& equipment, const std::filesystem::path& path);

} // namespace voxel
