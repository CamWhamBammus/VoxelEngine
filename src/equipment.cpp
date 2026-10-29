#include "equipment.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <unistd.h>

namespace voxel {
namespace {
constexpr std::size_t MaxProfileBytes = 16 * 1024 * 1024;
constexpr std::size_t MaxLootRecords = 65536;
constexpr std::size_t MaxLootKeyBytes = 1024;

const std::array<GearMaterial, GearTierCount> Materials{{
    {1, ItemId::Coal, "Coal", {.27f, .31f, .37f}},
    {2, ItemId::CopperOre, "Copper", {.91f, .49f, .24f}},
    {3, ItemId::TinOre, "Tin", {.70f, .79f, .79f}},
    {4, ItemId::IronOre, "Iron", {.64f, .67f, .75f}},
    {5, ItemId::Quartz, "Quartz", {.91f, .94f, 1.00f}},
    {6, ItemId::Amber, "Amber", {1.00f, .58f, .12f}},
    {7, ItemId::SilverOre, "Silver", {.83f, .91f, .97f}},
    {8, ItemId::GoldOre, "Gold", {1.00f, .78f, .20f}},
    {9, ItemId::Jade, "Jade", {.26f, .78f, .54f}},
    {10, ItemId::Amethyst, "Amethyst", {.71f, .40f, .98f}},
    {11, ItemId::Obsidian, "Obsidian", {.36f, .25f, .52f}},
    {12, ItemId::CobaltOre, "Cobalt", {.22f, .51f, .89f}},
    {13, ItemId::TitaniumOre, "Titanium", {.59f, .78f, .91f}},
    {14, ItemId::Ruby, "Ruby", {.96f, .22f, .36f}},
    {15, ItemId::Sapphire, "Sapphire", {.25f, .48f, 1.00f}},
    {16, ItemId::Emerald, "Emerald", {.18f, .95f, .49f}},
    {17, ItemId::MithrilOre, "Mithril", {.31f, 1.00f, .88f}},
    {18, ItemId::GlowCrystal, "Glow Crystal", {.29f, .91f, 1.00f}}
}};

int& tierFor(Equipment& equipment, GearSlot slot)
{
    switch (slot) {
    case GearSlot::Pickaxe: return equipment.pickaxeTier;
    case GearSlot::Sword: return equipment.swordTier;
    case GearSlot::Helmet: return equipment.armorTiers[0];
    case GearSlot::Chestplate: return equipment.armorTiers[1];
    case GearSlot::Leggings: return equipment.armorTiers[2];
    case GearSlot::Boots: return equipment.armorTiers[3];
    case GearSlot::Count: break;
    }
    throw std::invalid_argument("Unknown equipment slot");
}

bool validLootKey(std::string_view key)
{
    if (key.empty() || key.size() > MaxLootKeyBytes) return false;
    for (const unsigned char character : key)
        if (character < 32 || character > 126) return false;
    return true;
}

void validate(const Equipment& equipment)
{
    if (equipment.pickaxeTier < 1 || equipment.pickaxeTier > GearTierCount
        || equipment.swordTier < 1 || equipment.swordTier > GearTierCount
        || std::any_of(equipment.armorTiers.begin(), equipment.armorTiers.end(),
                       [](int tier) { return tier < 0 || tier > GearTierCount; })
        || !equipment.inventory.valid()) {
        throw std::runtime_error("Invalid equipment tier or inventory");
    }
    if (equipment.lootProgress.size() > MaxLootRecords) throw std::runtime_error("Too many saved chest records");
    for (const auto& [key, progress] : equipment.lootProgress) {
        if (!validLootKey(key)) throw std::runtime_error("Invalid saved chest identifier");
        for (const int count : progress.taken)
            if (count < 0 || count > ChestStackLimit) throw std::runtime_error("Invalid saved chest count");
    }
}
} // namespace

Equipment::Equipment() { inventory.add(ItemId::Coal, 12); }

std::span<const GearMaterial> gearMaterials() { return Materials; }

const GearMaterial& gearMaterial(int tier)
{
    return Materials[std::clamp(tier, 1, GearTierCount) - 1];
}

std::string_view gearSlotName(GearSlot slot)
{
    switch (slot) {
    case GearSlot::Pickaxe: return "Pickaxe";
    case GearSlot::Sword: return "Sword";
    case GearSlot::Helmet: return "Helmet";
    case GearSlot::Chestplate: return "Chestplate";
    case GearSlot::Leggings: return "Leggings";
    case GearSlot::Boots: return "Boots";
    case GearSlot::Count: break;
    }
    throw std::invalid_argument("Unknown equipment slot");
}

int gearTier(const Equipment& equipment, GearSlot slot)
{
    switch (slot) {
    case GearSlot::Pickaxe: return equipment.pickaxeTier;
    case GearSlot::Sword: return equipment.swordTier;
    case GearSlot::Helmet: return equipment.armorTiers[0];
    case GearSlot::Chestplate: return equipment.armorTiers[1];
    case GearSlot::Leggings: return equipment.armorTiers[2];
    case GearSlot::Boots: return equipment.armorTiers[3];
    case GearSlot::Count: break;
    }
    throw std::invalid_argument("Unknown equipment slot");
}

std::string gearName(const Equipment& equipment, GearSlot slot)
{
    const int tier = gearTier(equipment, slot);
    if (tier == 0) return "No " + std::string(gearSlotName(slot));
    return std::string(gearMaterial(tier).name) + " " + std::string(gearSlotName(slot));
}

std::string_view toolName(Tool tool)
{
    switch (tool) {
    case Tool::Pickaxe: return "Pickaxe";
    case Tool::Lantern: return "Lantern";
    case Tool::Compass: return "Compass";
    case Tool::Sword: return "Sword";
    }
    return "Pickaxe";
}

Tool Equipment::selectedTool() const
{
    return selectedSlot >= 0 && selectedSlot < ToolCount ? static_cast<Tool>(selectedSlot) : Tool::Pickaxe;
}

std::optional<GearUpgradeCost> gearUpgradeCost(const Equipment& equipment, GearSlot slot)
{
    const int tier = gearTier(equipment, slot);
    if (tier >= GearTierCount) return {};
    return GearUpgradeCost{gearMaterial(tier).ore, 10, tier + 1};
}

UpgradeResult upgradeGear(Equipment& equipment, GearSlot slot)
{
    validate(equipment);
    const auto cost = gearUpgradeCost(equipment, slot);
    if (!cost) return UpgradeResult::MaxTier;
    if (equipment.inventory.count(cost->item) < cost->count)
        return UpgradeResult::InsufficientResources;
    equipment.inventory.remove(cost->item, cost->count);
    tierFor(equipment, slot) = cost->nextTier;
    return UpgradeResult::Upgraded;
}

UpgradeResult upgradePickaxe(Equipment& equipment) { return upgradeGear(equipment, GearSlot::Pickaxe); }

int swordDamage(const Equipment& equipment)
{
    return 25 + 2 * (std::clamp(equipment.swordTier, 1, GearTierCount) - 1);
}

int armorProtection(const Equipment& equipment)
{
    int protection = 0;
    for (const int tier : equipment.armorTiers)
        protection += std::clamp(tier, 0, GearTierCount);
    return protection;
}

int armoredDamage(const Equipment& equipment, int rawDamage)
{
    if (rawDamage <= 0) return 0;
    // Round up so small attacks still hurt. Use wide arithmetic for damage input.
    return static_cast<int>((static_cast<long long>(rawDamage) * (100 - armorProtection(equipment)) + 99) / 100);
}

Equipment loadEquipment(const std::filesystem::path& path)
{
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Progress file is not a regular file");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open progress file");
    const auto fileSize = std::filesystem::file_size(path);
    if (fileSize > MaxProfileBytes) throw std::runtime_error("Progress file is too large");
    // One extra byte also catches a file that grows after the size check.
    std::string bytes(static_cast<std::size_t>(fileSize) + 1, '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (input.bad() || input.gcount() == static_cast<std::streamsize>(bytes.size()))
        throw std::runtime_error("Progress file is unreadable or too large");
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    std::istringstream fields(std::move(bytes));
    std::string magic, tierLabel;
    int version = 0;
    Equipment equipment;
    equipment.inventory = {};
    if (!(fields >> magic >> version >> tierLabel >> equipment.pickaxeTier)
        || magic != "VOXEL_PROGRESS" || version < 1 || version > 4
        || tierLabel != (version == 4 ? "GEAR" : "TIER")) {
        throw std::runtime_error("Invalid progress format or version");
    }
    if (version == 4) {
        if (!(fields >> equipment.swordTier)) throw std::runtime_error("Invalid sword tier");
        for (int& tier : equipment.armorTiers)
            if (!(fields >> tier)) throw std::runtime_error("Invalid armor tier");
    } else {
        // Preserve access to every ore the old three-tier pickaxe could mine.
        constexpr std::array legacyTiers{6, 11, GearTierCount};
        if (equipment.pickaxeTier < 1 || equipment.pickaxeTier > 3)
            throw std::runtime_error("Invalid legacy pickaxe tier");
        equipment.pickaxeTier = legacyTiers[equipment.pickaxeTier - 1];
    }
    if (version == 1) {
        std::string oreLabel;
        std::array<int, 3> balances{};
        if (!(fields >> oreLabel >> balances[0] >> balances[1] >> balances[2]) || oreLabel != "ORE")
            throw std::runtime_error("Invalid legacy ore balances");
        constexpr std::array<ItemId, 3> legacyItems{ItemId::CopperOre, ItemId::IronOre, ItemId::GlowCrystal};
        for (std::size_t i = 0; i < balances.size(); ++i) {
            if (balances[i] < 0 || balances[i] > 999999)
                throw std::runtime_error("Invalid legacy ore balance");
            if (equipment.inventory.add(legacyItems[i], balances[i]) != 0)
                throw std::runtime_error("Legacy ore balances exceed the 20-slot inventory; original profile was preserved");
        }
    } else {
        std::string inventoryLabel;
        int slotCount = 0;
        if (!(fields >> inventoryLabel >> slotCount) || inventoryLabel != "INVENTORY" || slotCount != Inventory::SlotCount)
            throw std::runtime_error("Invalid inventory size");
        for (auto& slot : equipment.inventory.slots) {
            std::string slotLabel;
            int item = 0;
            if (!(fields >> slotLabel >> item >> slot.count) || slotLabel != "SLOT"
                || item < 0 || item >= (version == 2 ? static_cast<int>(ItemId::Key) : ItemCount)
                || (item == static_cast<int>(ItemId::Key) && slot.count != 0))
                throw std::runtime_error("Invalid inventory slot");
            slot.item = static_cast<ItemId>(item);
        }
    }
    if (version >= 3) {
        std::string label;
        int recordCount = 0;
        if (!(fields >> label >> recordCount) || label != "LOOT" || recordCount < 0
            || static_cast<std::size_t>(recordCount) > MaxLootRecords)
            throw std::runtime_error("Invalid saved chest record count");
        for (int record = 0; record < recordCount; ++record) {
            std::string key;
            LootProgress progress;
            int unlocked = 0;
            if (!(fields >> label) || label != "CHEST") throw std::runtime_error("Invalid saved chest record");
            fields >> std::ws;
            if (fields.peek() != '"' || !(fields >> std::quoted(key) >> unlocked)
                || !validLootKey(key) || (unlocked != 0 && unlocked != 1))
                throw std::runtime_error("Invalid saved chest identifier or lock state");
            progress.unlocked = unlocked == 1;
            for (int& count : progress.taken)
                if (!(fields >> count) || count < 0 || count > ChestStackLimit)
                    throw std::runtime_error("Invalid saved chest count");
            if (!equipment.lootProgress.emplace(std::move(key), progress).second)
                throw std::runtime_error("Duplicate saved chest identifier");
        }
    }
    fields >> std::ws;
    if (!fields.eof()) throw std::runtime_error("Unexpected data after progress profile");
    validate(equipment);
    return equipment;
}

void saveEquipment(const Equipment& equipment, const std::filesystem::path& path)
{
    validate(equipment);
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    if (!std::filesystem::is_directory(parent)) throw std::runtime_error("Progress folder does not exist");
    std::ostringstream output;
    output << "VOXEL_PROGRESS 4\nGEAR " << equipment.pickaxeTier << ' ' << equipment.swordTier;
    for (const int tier : equipment.armorTiers) output << ' ' << tier;
    output << "\nINVENTORY " << Inventory::SlotCount << '\n';
    for (const auto& slot : equipment.inventory.slots) {
        const auto persisted = slot.item == ItemId::Key ? ItemStack{} : slot;
        output << "SLOT " << static_cast<int>(persisted.item) << ' ' << persisted.count << '\n';
    }
    output << "LOOT " << equipment.lootProgress.size() << '\n';
    for (const auto& [key, progress] : equipment.lootProgress) {
        output << "CHEST " << std::quoted(key) << ' ' << (progress.unlocked ? 1 : 0);
        for (const int count : progress.taken) output << ' ' << count;
        output << '\n';
        if (output.tellp() > static_cast<std::streamoff>(MaxProfileBytes))
            throw std::runtime_error("Progress file is too large");
    }
    const auto bytes = output.str();
    const auto pattern = (parent / ".voxel-progress-XXXXXX").string();
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back('\0');
    int descriptor = ::mkstemp(temporary.data());
    if (descriptor < 0) throw std::system_error(errno, std::generic_category(), "Cannot create progress file");
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written < 0) throw std::system_error(errno, std::generic_category(), "Cannot write progress");
            if (written == 0) throw std::runtime_error("Progress write made no progress");
            offset += static_cast<std::size_t>(written);
        }
        if (::fsync(descriptor) != 0) throw std::system_error(errno, std::generic_category(), "Cannot flush progress");
        const int closed = ::close(descriptor);
        descriptor = -1;
        if (closed != 0) throw std::system_error(errno, std::generic_category(), "Cannot close progress file");
        std::filesystem::rename(std::filesystem::path(temporary.data()), path);
    } catch (...) {
        if (descriptor >= 0) ::close(descriptor);
        ::unlink(temporary.data());
        throw;
    }
}

} // namespace voxel
