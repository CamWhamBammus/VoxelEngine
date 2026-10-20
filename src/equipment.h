#pragma once

#include "inventory.h"
#include <filesystem>
#include <optional>
#include <string_view>

namespace voxel {

enum class Tool { Pickaxe, Lantern, Compass };
inline constexpr int ToolCount = 3;
std::string_view toolName(Tool tool);

struct Equipment {
    int selectedSlot = 0;
    int pickaxeTier = 1;
    Inventory inventory;

    Equipment();
    Tool selectedTool() const;
    bool operator==(const Equipment&) const = default;
};

struct UpgradeCost { int copper = 0; int iron = 0; };
std::optional<UpgradeCost> pickaxeUpgradeCost(int tier);
enum class UpgradeResult { Upgraded, InsufficientResources, MaxTier };
UpgradeResult upgradePickaxe(Equipment& equipment);

// A versioned profile stores pickaxe tier and all inventory slots independently
// of editor worlds. Legacy ore balances migrate without losing items.
Equipment loadEquipment(const std::filesystem::path& path);
// A unique sibling temporary file is flushed, then replaces the destination.
// Failure preserves the previous profile. Tool selection is session-only.
void saveEquipment(const Equipment& equipment, const std::filesystem::path& path);

} // namespace voxel
