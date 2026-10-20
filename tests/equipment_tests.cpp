#include "equipment.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
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
              && equipment.inventory.count(ItemId::CopperOre) == 12 && equipment.inventory.occupiedSlots() == 1,
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
              && equipment.pickaxeTier == 2 && equipment.inventory.count(ItemId::CopperOre) == 4,
              "first upgrade charges exactly eight copper once");
        const auto unchanged = equipment;
        check(upgradePickaxe(equipment) == UpgradeResult::InsufficientResources && equipment == unchanged,
              "insufficient iron leaves the whole equipment state unchanged");
        equipment.inventory.add(ItemId::IronOre, 8);
        check(upgradePickaxe(equipment) == UpgradeResult::Upgraded
              && equipment.pickaxeTier == 3 && equipment.inventory.count(ItemId::IronOre) == 0
              && equipment.inventory.count(ItemId::CopperOre) == 4,
              "second upgrade charges iron without changing copper");
        const auto maximum = equipment;
        check(upgradePickaxe(equipment) == UpgradeResult::MaxTier && equipment == maximum,
              "maximum-tier upgrade never spends resources");
        Equipment poor;
        poor.inventory.remove(ItemId::CopperOre, 5);
        const auto before = poor;
        check(upgradePickaxe(poor) == UpgradeResult::InsufficientResources && poor == before,
              "a partial copper payment is never consumed");

        TemporaryDirectory directory;
        const auto path = directory.path / "progress.vxp";
        equipment.inventory = {};
        for (int slot = 0; slot < Inventory::SlotCount; ++slot)
            equipment.inventory.slots[slot] = {static_cast<ItemId>(slot % ItemCount), slot * 4 + 1};
        equipment.selectedSlot = 2;
        LootProgress firstChest; firstChest.unlocked = true; firstChest.taken[0] = 7; firstChest.taken[19] = 99;
        LootProgress secondChest; secondChest.taken[4] = 12;
        equipment.lootProgress["lush-caves/treasure/old \"mine\".vxs/asset-3"] = firstChest;
        equipment.lootProgress["lush-caves/exploration/root\\path.vxs/asset-8"] = secondChest;
        const auto beforeSave = equipment;
        saveEquipment(equipment, path);
        check(equipment == beforeSave, "saving does not mutate transient key slots or session state");
        auto expected = equipment;
        expected.selectedSlot = 0;
        for (auto& slot : expected.inventory.slots) if (slot.item == ItemId::Key) slot = {};
        check(loadEquipment(path) == expected, "profile restores ordinary slots and chest ledger, leaving keys and hotbar selection session-only");
        const auto saved = contents(path);
        check(saved.starts_with("VOXEL_PROGRESS 3"), "new profiles use persistent chest format");
        check(loadEquipment(path).inventory.count(ItemId::Key) == 0 && equipment.inventory.count(ItemId::Key) > 0,
              "section keys serialize as empty slots and never return after restarting");
        equipment.inventory.slots[0].count = -1;
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

        auto versionTwo = saved.substr(0, saved.find("LOOT "));
        versionTwo.replace(versionTwo.find("VOXEL_PROGRESS 3"), 16, "VOXEL_PROGRESS 2");
        { std::ofstream file(path); file << versionTwo; }
        auto oldInventory = expected;
        oldInventory.lootProgress.clear();
        check(loadEquipment(path) == oldInventory, "version two preserves every ordinary slot and starts an empty chest ledger");
        check(contents(path) == versionTwo, "version two migration does not modify the original file");
        saveEquipment(oldInventory, path);
        check(loadEquipment(path) == oldInventory && contents(path).starts_with("VOXEL_PROGRESS 3"),
              "version two upgrades to version three on the next successful save");
        auto invalidVersionTwo = versionTwo;
        invalidVersionTwo.replace(invalidVersionTwo.find("SLOT 0 1\n"), 9, "SLOT 18 1\n");
        { std::ofstream file(path); file << invalidVersionTwo; }
        rejects([&] { static_cast<void>(loadEquipment(path)); }, "version two cannot use the newly added cave key item ID");

        saveEquipment(expected, path);
        for (int badCase = 0; badCase < 5; ++badCase) {
            auto bad = expected;
            if (badCase == 0) bad.lootProgress[""] = {};
            if (badCase == 1) bad.lootProgress[std::string(1025, 'a')] = {};
            if (badCase == 2) bad.lootProgress["bad\nsection"] = {};
            if (badCase == 3) bad.lootProgress["bad-count"].taken[0] = -1;
            if (badCase == 4) bad.lootProgress["bad-count"].taken[19] = 100;
            rejects([&] { saveEquipment(bad, path); }, "invalid chest ledgers cannot be saved");
            check(contents(path) == saved, "invalid chest ledger leaves the previous profile unchanged");
        }
        {
            Equipment tooMany;
            for (int i = 0; i < 65537; ++i) tooMany.lootProgress["chest-" + std::to_string(i)] = {};
            rejects([&] { saveEquipment(tooMany, path); }, "writer enforces the chest record bound");
            check(contents(path) == saved, "oversized record count preserves the previous profile");
        }
        {
            Equipment tooLarge;
            for (int i = 0; i < 16500; ++i) tooLarge.lootProgress[std::string(1000, 'a') + std::to_string(i)] = {};
            rejects([&] { saveEquipment(tooLarge, path); }, "writer enforces the serialized file byte bound");
            check(contents(path) == saved, "oversized serialized profile preserves the previous file");
        }
        const auto chestRecord = [](std::string key, int unlocked = 0, int firstCount = 0, int slots = 20) {
            std::ostringstream record;
            record << "CHEST " << std::quoted(key) << ' ' << unlocked;
            for (int slot = 0; slot < slots; ++slot) record << ' ' << (slot == 0 ? firstCount : 0);
            return record.str() + '\n';
        };
        const auto inventoryPrefix = saved.substr(0, saved.find("LOOT "));
        for (const auto& ledger : std::vector<std::string>{
            "", "LOOT -1\n", "LOOT 65537\n", "LOOT 1\n",
            "LOOT 1\n" + chestRecord("section", 2),
            "LOOT 1\n" + chestRecord("section", 0, -1),
            "LOOT 1\n" + chestRecord("section", 0, 100),
            "LOOT 1\n" + chestRecord("section", 0, 0, 19),
            "LOOT 1\n" + chestRecord(""),
            "LOOT 1\n" + chestRecord(std::string(1025, 'x')),
            "LOOT 1\n" + chestRecord("bad\nidentifier"),
            "LOOT 2\n" + chestRecord("section") + chestRecord("section"),
            "LOOT 1\nCHEST unquoted 0 0 0\n",
            "LOOT 0\nextra\n"}) {
            const auto malformed = inventoryPrefix + ledger;
            { std::ofstream file(path); file << malformed; }
            rejects([&] { static_cast<void>(loadEquipment(path)); }, "malformed chest ledger must be rejected");
            check(contents(path) == malformed, "invalid ledger loading never writes to the profile");
        }

        const std::string legacy = "VOXEL_PROGRESS 1\nTIER 2\nORE 205 103 7\n";
        { std::ofstream file(path); file << legacy; }
        auto migrated = loadEquipment(path);
        check(migrated.pickaxeTier == 2 && migrated.inventory.count(ItemId::CopperOre) == 205
              && migrated.inventory.count(ItemId::IronOre) == 103
              && migrated.inventory.count(ItemId::GlowCrystal) == 7
              && migrated.inventory.occupiedSlots() == 6 && migrated.inventory.valid(),
              "legacy balances split into stacks without dropping any ore or adding starter copper");
        check(contents(path) == legacy, "loading a legacy profile does not alter it on disk");
        saveEquipment(migrated, path);
        check(loadEquipment(path) == migrated && contents(path).starts_with("VOXEL_PROGRESS 3"),
              "the next successful save upgrades the profile format");
        { std::ofstream file(path); file << "VOXEL_PROGRESS 1\nTIER 1\nORE 1980 0 0\n"; }
        check(loadEquipment(path).inventory.count(ItemId::CopperOre) == 1980,
              "a legacy balance that exactly fills twenty stacks is preserved");
        migrated.inventory = {};
        migrated.inventory.slots[7] = {ItemId::Ruby, 4};
        migrated.inventory.slots[19] = {ItemId::CopperOre, 99};
        saveEquipment(migrated, path);
        check(loadEquipment(path) == migrated, "sparse inventory slot arrangement is retained across saves");

        auto invalidCount = saved;
        invalidCount.replace(invalidCount.find("SLOT 0 1\n"), 9, "SLOT 0 100\n");
        auto invalidId = saved;
        invalidId.replace(invalidId.find("SLOT 0 1\n"), 9, "SLOT 18 1\n");
        auto negativeId = saved;
        negativeId.replace(negativeId.find("SLOT 0 1\n"), 9, "SLOT -1 1\n");
        auto noncanonicalEmpty = saved;
        noncanonicalEmpty.replace(noncanonicalEmpty.find("SLOT 0 1\n"), 9, "SLOT 1 0\n");
        auto wrongSize = saved;
        wrongSize.replace(wrongSize.find("INVENTORY 20"), 12, "INVENTORY 19");
        for (const auto& malformed : std::vector<std::string>{
            "", "VOXEL_PROGRESS 1\nTIER 1\nORE 12 0", "VOXEL_PROGRESS 2\nTIER 1\nORE 12 0 0\n",
            "VOXEL_PROGRESS 3\nTIER 1\nORE 12 0 0\n",
            "VOXEL_PROGRESS 1\nTIER 0\nORE 12 0 0\n", "VOXEL_PROGRESS 1\nTIER 4\nORE 12 0 0\n",
            "VOXEL_PROGRESS 1\nTIER 1\nORE -1 0 0\n", "VOXEL_PROGRESS 1\nTIER 1\nORE 0 1000000 0\n",
            "VOXEL_PROGRESS 1\nTIER 1\nORE 0 0 99999999999999999\n",
            "VOXEL_PROGRESS 1\nTIER 1\nORE 1981 0 0\n",
            "VOXEL_PROGRESS 1\nTIER 1\nORE 990 990 1\n",
            "VOXEL_PROGRESS 1\nTIER 1\nORE 12 0 0\nextra", std::string(16 * 1024 * 1024 + 1, 'x'),
            invalidCount, invalidId, negativeId, noncanonicalEmpty, wrongSize,
            saved.substr(0, saved.rfind("SLOT")), saved + "extra\n"}) {
            { std::ofstream file(path, std::ios::binary); file << malformed; }
            rejects([&] { static_cast<void>(loadEquipment(path)); }, "malformed progress must be rejected");
            check(contents(path) == malformed, "profile loading never overwrites malformed input");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    return failures ? 1 : 0;
}
