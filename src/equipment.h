#pragma once

#include "inventory.h"
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace voxel {

enum class Tool { Pickaxe, Lantern, Compass, Sword };
inline constexpr int ToolCount = 4;
std::string_view toolName(Tool tool);

enum class GearSlot { Pickaxe, Sword, Helmet, Chestplate, Leggings, Boots, Count };
inline constexpr int GearSlotCount = static_cast<int>(GearSlot::Count);
inline constexpr int GearTierCount = 18;

struct GearMaterial {
    int tier;
    ItemId ore;
    std::string_view name;
    glm::vec3 color;
};

std::span<const GearMaterial> gearMaterials();
// Drawing code may pass an empty slot; use the first material as its fallback.
const GearMaterial& gearMaterial(int tier);
std::string_view gearSlotName(GearSlot slot);

struct LootProgress {
    bool unlocked = false;
    std::array<int, ChestSlotCount> taken{};
    bool operator==(const LootProgress&) const = default;
};

struct Equipment {
    int selectedSlot = 0;
    int pickaxeTier = 1;
    int swordTier = 1;
    // Helmet, chestplate, leggings, boots. Zero means no piece equipped.
    std::array<int, 4> armorTiers{};
    Inventory inventory;
    std::map<std::string, LootProgress> lootProgress;

    Equipment();
    Tool selectedTool() const;
    bool operator==(const Equipment&) const = default;
};

int gearTier(const Equipment& equipment, GearSlot slot);
std::string gearName(const Equipment& equipment, GearSlot slot);
struct GearUpgradeCost { ItemId item; int count; int nextTier; };
std::optional<GearUpgradeCost> gearUpgradeCost(const Equipment& equipment, GearSlot slot);
enum class UpgradeResult { Upgraded, InsufficientResources, MaxTier };
UpgradeResult upgradeGear(Equipment& equipment, GearSlot slot);
UpgradeResult upgradePickaxe(Equipment& equipment);
int swordDamage(const Equipment& equipment);
int armorProtection(const Equipment& equipment);
int armoredDamage(const Equipment& equipment, int rawDamage);

// A versioned profile stores gear, ordinary inventory items and each chest's
// persistent loot state. Cave keys stay in the current section and are not saved.
// Legacy ore balances and inventory profiles migrate without losing ore.
Equipment loadEquipment(const std::filesystem::path& path);
// A unique sibling temporary file is flushed, then replaces the destination.
// Failure preserves the previous profile. Tool selection is session-only.
void saveEquipment(const Equipment& equipment, const std::filesystem::path& path);

} // namespace voxel
