#include "equipment.h"

#include <array>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <unistd.h>

namespace voxel {
namespace {
void validate(const Equipment& equipment)
{
    if (equipment.pickaxeTier < 1 || equipment.pickaxeTier > 3
        || !equipment.inventory.valid()) {
        throw std::runtime_error("Invalid equipment tier or inventory");
    }
}
} // namespace

Equipment::Equipment() { inventory.add(ItemId::CopperOre, 12); }

std::string_view toolName(Tool tool)
{
    switch (tool) {
    case Tool::Pickaxe: return "Pickaxe";
    case Tool::Lantern: return "Lantern";
    case Tool::Compass: return "Compass";
    }
    return "Pickaxe";
}

Tool Equipment::selectedTool() const
{
    return selectedSlot >= 0 && selectedSlot < ToolCount ? static_cast<Tool>(selectedSlot) : Tool::Pickaxe;
}

std::optional<UpgradeCost> pickaxeUpgradeCost(int tier)
{
    if (tier == 1) return UpgradeCost{8, 0};
    if (tier == 2) return UpgradeCost{0, 8};
    return std::nullopt;
}

UpgradeResult upgradePickaxe(Equipment& equipment)
{
    validate(equipment);
    const auto cost = pickaxeUpgradeCost(equipment.pickaxeTier);
    if (!cost) return UpgradeResult::MaxTier;
    if (equipment.inventory.count(ItemId::CopperOre) < cost->copper
        || equipment.inventory.count(ItemId::IronOre) < cost->iron)
        return UpgradeResult::InsufficientResources;
    equipment.inventory.remove(ItemId::CopperOre, cost->copper);
    equipment.inventory.remove(ItemId::IronOre, cost->iron);
    ++equipment.pickaxeTier;
    return UpgradeResult::Upgraded;
}

Equipment loadEquipment(const std::filesystem::path& path)
{
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Progress file is not a regular file");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open progress file");
    std::array<char, 4097> bytes{};
    input.read(bytes.data(), bytes.size());
    if (input.bad() || input.gcount() == static_cast<std::streamsize>(bytes.size()))
        throw std::runtime_error("Progress file is unreadable or too large");
    std::istringstream fields(std::string(bytes.data(), static_cast<std::size_t>(input.gcount())));
    std::string magic, tierLabel;
    int version = 0;
    Equipment equipment;
    equipment.inventory = {};
    if (!(fields >> magic >> version >> tierLabel >> equipment.pickaxeTier)
        || magic != "VOXEL_PROGRESS" || (version != 1 && version != 2) || tierLabel != "TIER") {
        throw std::runtime_error("Invalid progress format or version");
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
                || item < 0 || item >= ItemCount)
                throw std::runtime_error("Invalid inventory slot");
            slot.item = static_cast<ItemId>(item);
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
    output << "VOXEL_PROGRESS 2\nTIER " << equipment.pickaxeTier
           << "\nINVENTORY " << Inventory::SlotCount << '\n';
    for (const auto& slot : equipment.inventory.slots)
        output << "SLOT " << static_cast<int>(slot.item) << ' ' << slot.count << '\n';
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
