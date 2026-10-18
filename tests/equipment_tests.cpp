#include "equipment.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory()
    {
        auto pattern = (std::filesystem::temp_directory_path() / "voxel-equipment-XXXXXX").string();
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back('\0');
        if (!mkdtemp(name.data())) throw std::runtime_error("Cannot create test directory");
        path = name.data();
    }
    ~TemporaryDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
};
std::string contents(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool passed, const char* message) {
        if (!passed) { std::cerr << message << '\n'; ++failures; }
    };
    const auto rejects = [&](auto operation, const char* message) {
        bool rejected = false;
        try { operation(); } catch (const std::exception&) { rejected = true; }
        check(rejected, message);
    };
    try {
        Equipment equipment;
        check(equipment.selectedTool() == Tool::Pickaxe && equipment.pickaxeTier == 1
              && equipment.copper == 12 && equipment.iron == 0 && equipment.crystal == 0,
              "new players receive the three-tool kit and twelve starter copper");
        for (int slot = 0; slot < ToolCount; ++slot) {
            equipment.selectedSlot = slot;
            check(static_cast<int>(equipment.selectedTool()) == slot && !toolName(equipment.selectedTool()).empty(),
                  "each hotbar slot selects a named tool");
        }
        equipment.selectedSlot = 0;
        check(pickaxeUpgradeCost(1)->copper == 8 && pickaxeUpgradeCost(1)->iron == 0
              && pickaxeUpgradeCost(2)->copper == 0 && pickaxeUpgradeCost(2)->iron == 8
              && !pickaxeUpgradeCost(3), "pickaxe tiers have explicit copper and iron costs and a cap");
        check(upgradePickaxe(equipment) == UpgradeResult::Upgraded
              && equipment.pickaxeTier == 2 && equipment.copper == 4,
              "first upgrade charges exactly eight copper once");
        const auto unchanged = equipment;
        check(upgradePickaxe(equipment) == UpgradeResult::InsufficientResources && equipment == unchanged,
              "insufficient iron leaves the whole equipment state unchanged");
        equipment.iron = 8;
        check(upgradePickaxe(equipment) == UpgradeResult::Upgraded
              && equipment.pickaxeTier == 3 && equipment.iron == 0 && equipment.copper == 4,
              "second upgrade charges iron without changing copper");
        const auto maximum = equipment;
        check(upgradePickaxe(equipment) == UpgradeResult::MaxTier && equipment == maximum,
              "maximum-tier upgrade never spends resources");
        Equipment poor;
        poor.copper = 7;
        const auto before = poor;
        check(upgradePickaxe(poor) == UpgradeResult::InsufficientResources && poor == before,
              "a partial copper payment is never consumed");

        TemporaryDirectory directory;
        const auto path = directory.path / "progress.vxp";
        equipment.copper = 4; equipment.iron = 12; equipment.crystal = 27;
        equipment.selectedSlot = 2;
        saveEquipment(equipment, path);
        auto expected = equipment;
        expected.selectedSlot = 0;
        check(loadEquipment(path) == expected, "profile restores tier and ore balances, leaving hotbar selection session-only");
        const auto saved = contents(path);
        equipment.copper = -1;
        rejects([&] { saveEquipment(equipment, path); }, "invalid balances cannot be saved");
        check(contents(path) == saved, "validation failure preserves the previous save bytes");
        rejects([&] { saveEquipment(expected, directory.path / "missing" / "progress.vxp"); },
                "missing parent folder fails explicitly");
        const auto destination = directory.path / "occupied";
        std::filesystem::create_directory(destination);
        { std::ofstream file(destination / "keep"); file << "untouched"; }
        rejects([&] { saveEquipment(expected, destination); }, "failed atomic replacement reports failure");
        check(contents(destination / "keep") == "untouched", "failed replacement preserves the existing destination");
        bool temporaryRemains = false;
        for (const auto& entry : std::filesystem::directory_iterator(directory.path))
            temporaryRemains |= entry.path().filename().string().starts_with(".voxel-progress-");
        check(!temporaryRemains, "failed profile writes clean up their sibling temporary files");

        for (const auto& malformed : std::vector<std::string>{
            "", "VOXEL_PROGRESS 1\nTIER 1\nORE 12 0", "VOXEL_PROGRESS 2\nTIER 1\nORE 12 0 0\n",
            "VOXEL_PROGRESS 1\nTIER 0\nORE 12 0 0\n", "VOXEL_PROGRESS 1\nTIER 4\nORE 12 0 0\n",
            "VOXEL_PROGRESS 1\nTIER 1\nORE -1 0 0\n", "VOXEL_PROGRESS 1\nTIER 1\nORE 0 1000000 0\n",
            "VOXEL_PROGRESS 1\nTIER 1\nORE 0 0 99999999999999999\n",
            "VOXEL_PROGRESS 1\nTIER 1\nORE 12 0 0\nextra", std::string(300, 'x')}) {
            { std::ofstream file(path, std::ios::binary); file << malformed; }
            rejects([&] { static_cast<void>(loadEquipment(path)); }, "malformed progress must be rejected");
            check(contents(path) == malformed, "profile loading never overwrites malformed input");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    return failures ? 1 : 0;
}
