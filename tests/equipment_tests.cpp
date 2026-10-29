#include "equipment.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
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
              && equipment.inventory.count(ItemId::Coal) == 12 && equipment.inventory.occupiedSlots() == 1,
              "new players receive the four-tool kit and twelve starter coal");
        for (int slot = 0; slot < ToolCount; ++slot) {
            equipment.selectedSlot = slot;
            check(static_cast<int>(equipment.selectedTool()) == slot && !toolName(equipment.selectedTool()).empty(),
                  "each hotbar slot selects a named tool");
        }
        equipment.selectedSlot=3;
        check(equipment.selectedTool()==Tool::Sword && toolName(Tool::Sword)=="Sword",
              "fourth tool is the sword without changing the original three slots");
        equipment.selectedSlot=ToolCount;
        check(equipment.selectedTool()==Tool::Pickaxe,"out-of-range tool selection safely falls back");
        equipment.selectedSlot = 0;
        check(gearMaterials().size() == GearTierCount && GearSlotCount == 6,
              "eighteen ore materials cover two tools and four armor slots");
        constexpr std::array expectedOres{
            ItemId::Coal, ItemId::CopperOre, ItemId::TinOre, ItemId::IronOre, ItemId::Quartz,
            ItemId::Amber, ItemId::SilverOre, ItemId::GoldOre, ItemId::Jade, ItemId::Amethyst,
            ItemId::Obsidian, ItemId::CobaltOre, ItemId::TitaniumOre, ItemId::Ruby,
            ItemId::Sapphire, ItemId::Emerald, ItemId::MithrilOre, ItemId::GlowCrystal};
        std::array<int, ItemCount> appearances{};
        for (int tier = 1; tier <= GearTierCount; ++tier) {
            const auto& material = gearMaterial(tier);
            check(material.tier == tier && material.ore == expectedOres[tier - 1]
                  && !material.name.empty() && material.color == itemInfo(material.ore).color,
                  "the material hierarchy has stable names, order, and ore colors");
            ++appearances[static_cast<int>(material.ore)];
        }
        for (int item = 0; item < ItemCount; ++item)
            check(appearances[item] == (item == static_cast<int>(ItemId::Key) ? 0 : 1),
                  "every ore supplies one gear family and cave keys supply none");
        check(gearMaterial(-1).tier == 1 && gearMaterial(999).tier == GearTierCount,
              "material lookups clamp invalid render tiers safely");

        for (int index = 0; index < GearSlotCount; ++index) {
            const auto slot = static_cast<GearSlot>(index);
            Equipment gear;
            gear.inventory = {};
            const int firstTier = index < 2 ? 1 : 0;
            check(gearTier(gear, slot) == firstTier && !gearSlotName(slot).empty(),
                  "tools begin at coal and armor begins empty");
            for (int tier = firstTier; tier < GearTierCount; ++tier) {
                const auto cost = gearUpgradeCost(gear, slot);
                check(cost && cost->count == 10 && cost->nextTier == tier + 1
                      && cost->item == gearMaterial(tier).ore,
                      "each step costs ten of its current ore; first armor costs coal");
                const auto before = gear;
                check(upgradeGear(gear, slot) == UpgradeResult::InsufficientResources && gear == before,
                      "missing upgrade ingredients leave all gear and inventory unchanged");
                gear.inventory.add(cost->item, 9);
                const auto partial = gear;
                check(upgradeGear(gear, slot) == UpgradeResult::InsufficientResources && gear == partial,
                      "partial upgrade payments are never consumed");
                gear.inventory.add(cost->item, 4);
                check(upgradeGear(gear, slot) == UpgradeResult::Upgraded
                      && gearTier(gear, slot) == tier + 1 && gear.inventory.count(cost->item) == 3,
                      "every gear step spends exactly ten resources and advances only one tier");
                if (index >= 2)
                    check(armorProtection(gear) == armorProtection(before) + 1,
                          "every paid armor upgrade adds one percent protection");
                for (int other = 0; other < GearSlotCount; ++other)
                    if (other != index)
                        check(gearTier(gear, static_cast<GearSlot>(other)) == gearTier(before, static_cast<GearSlot>(other)),
                              "upgrading a piece leaves all other gear alone");
                check(gearName(gear, slot) == std::string(gearMaterial(tier + 1).name) + " " + std::string(gearSlotName(slot)),
                      "all 108 material and slot combinations have their own readable name");
                gear.inventory = {};
            }
            gear.inventory.add(ItemId::GlowCrystal, 99);
            const auto maximum = gear;
            check(!gearUpgradeCost(gear, slot) && upgradeGear(gear, slot) == UpgradeResult::MaxTier && gear == maximum,
                  "maximum-tier gear has no next upgrade and never consumes resources");
        }
        check(upgradePickaxe(equipment) == UpgradeResult::Upgraded
              && equipment.pickaxeTier == 2 && equipment.inventory.count(ItemId::Coal) == 2,
              "the pickaxe compatibility helper follows the shared ten-ore chain");
        rejects([&] { static_cast<void>(gearTier(equipment, GearSlot::Count)); }, "invalid gear slots are rejected");
        const auto beforeInvalid = equipment;
        rejects([&] { static_cast<void>(upgradeGear(equipment, static_cast<GearSlot>(-1))); },
                "unknown upgrade slots are rejected before spending anything");
        check(equipment == beforeInvalid, "unknown slots cannot spend ore");

        Equipment stats;
        check(swordDamage(stats) == 25 && armorProtection(stats) == 0 && armoredDamage(stats, 100) == 100,
              "new players retain baseline sword damage and no armor reduction");
        int previousSword = 0, previousArmor = 0;
        for (int tier = 1; tier <= GearTierCount; ++tier) {
            stats.swordTier = tier;
            stats.armorTiers.fill(tier);
            check(swordDamage(stats) > previousSword && armorProtection(stats) == previousArmor + 4,
                  "every material tier improves sword damage and each armor piece by one percent");
            check(armoredDamage(stats, 100) == 100 - armorProtection(stats),
                  "damage uses the combined equipped armor percentage");
            previousSword = swordDamage(stats);
            previousArmor = armorProtection(stats);
        }
        check(swordDamage(stats) == 59 && armorProtection(stats) == 72 && armoredDamage(stats, 100) == 28,
              "maximum gear reaches 59 sword damage and 72 percent protection");
        check(armoredDamage(stats, 1) == 1 && armoredDamage(stats, 0) == 0 && armoredDamage(stats, -5) == 0
              && armoredDamage(stats, std::numeric_limits<int>::max()) > 0,
              "damage rounding preserves small hits and handles input bounds without overflow");
        for (int slot = 0; slot < 4; ++slot) {
            stats.armorTiers = {};
            for (int tier = 0; tier <= GearTierCount; ++tier) {
                stats.armorTiers[slot] = tier;
                check(armorProtection(stats) == tier,
                      "every armor slot contributes its displayed material tier as protection");
            }
        }

        TemporaryDirectory directory;
        const auto path = directory.path / "progress.vxp";
        equipment.inventory = {};
        for (int slot = 0; slot < Inventory::SlotCount; ++slot)
            equipment.inventory.slots[slot] = {static_cast<ItemId>(slot % ItemCount), slot * 4 + 1};
        equipment.selectedSlot = 2;
        equipment.pickaxeTier = 18;
        equipment.swordTier = 13;
        equipment.armorTiers = {2, 7, 11, 18};
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
        check(saved.starts_with("VOXEL_PROGRESS 4\nGEAR 18 13 2 7 11 18"), "new profiles persist every equipped piece");
        for (int slot = 0; slot < GearSlotCount; ++slot) {
            for (const int invalid : {-1, GearTierCount + 1}) {
                auto bad = expected;
                if (slot == 0) bad.pickaxeTier = invalid;
                else if (slot == 1) bad.swordTier = invalid;
                else bad.armorTiers[slot - 2] = invalid;
                rejects([&] { saveEquipment(bad, path); }, "out-of-range gear cannot be saved");
                check(contents(path) == saved, "invalid gear preserves the previous profile");
            }
        }
        for (const auto header : {"GEAR 0 1 0 0 0 0", "GEAR 1 0 0 0 0 0", "GEAR 19 1 0 0 0 0",
                                  "GEAR 1 19 0 0 0 0", "GEAR 1 1 -1 0 0 0", "GEAR 1 1 0 19 0 0",
                                  "GEAR 1 1 0 0 -1 0", "GEAR 1 1 0 0 0 19", "GEAR 1 1 0 0 0",
                                  "GEAR 1 1 0 0 0 0 0", "TIER 1"}) {
            const auto malformed = std::string("VOXEL_PROGRESS 4\n") + header + "\n" + saved.substr(saved.find("INVENTORY "));
            { std::ofstream file(path); file << malformed; }
            rejects([&] { static_cast<void>(loadEquipment(path)); }, "invalid or incomplete gear records are rejected");
            check(contents(path) == malformed, "invalid gear loading never overwrites the file");
        }
        saveEquipment(expected, path);

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

        const auto inventoryStart = saved.find("INVENTORY ");
        const auto legacyInventory = saved.substr(inventoryStart, saved.find("LOOT ") - inventoryStart);
        auto versionTwo = "VOXEL_PROGRESS 2\nTIER 2\n" + legacyInventory;
        { std::ofstream file(path); file << versionTwo; }
        auto oldInventory = expected;
        oldInventory.lootProgress.clear();
        oldInventory.pickaxeTier = 11;
        oldInventory.swordTier = 1;
        oldInventory.armorTiers = {};
        check(loadEquipment(path) == oldInventory, "version two preserves every ordinary slot and starts an empty chest ledger");
        check(contents(path) == versionTwo, "version two migration does not modify the original file");
        saveEquipment(oldInventory, path);
        check(loadEquipment(path) == oldInventory && contents(path).starts_with("VOXEL_PROGRESS 4"),
              "version two upgrades to version four on the next successful save");
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

        constexpr std::array migratedTiers{6, 11, 18};
        for (int version = 1; version <= 3; ++version) {
            for (int oldTier = 1; oldTier <= 3; ++oldTier) {
                const auto prefix = "VOXEL_PROGRESS " + std::to_string(version) + "\nTIER " + std::to_string(oldTier) + "\n";
                const auto oldBytes = prefix + (version == 1 ? std::string("ORE 205 103 7\n")
                    : legacyInventory + (version == 3 ? saved.substr(saved.find("LOOT ")) : std::string{}));
                { std::ofstream file(path); file << oldBytes; }
                const auto loaded = loadEquipment(path);
                check(loaded.pickaxeTier == migratedTiers[oldTier - 1] && loaded.swordTier == 1
                      && loaded.armorTiers == std::array<int,4>{},
                      "all three old profile versions preserve mining access and start new gear at baseline");
                check(version == 3 ? loaded.lootProgress == expected.lootProgress : loaded.lootProgress.empty(),
                      "migration preserves an existing loot ledger without inventing records");
                check(contents(path) == oldBytes, "migration leaves the source file unchanged until a successful save");
                saveEquipment(loaded, path);
                check(loadEquipment(path) == loaded, "migration to the gear profile is stable across repeated loads");
            }
        }

        const std::string legacy = "VOXEL_PROGRESS 1\nTIER 2\nORE 205 103 7\n";
        { std::ofstream file(path); file << legacy; }
        auto migrated = loadEquipment(path);
        check(migrated.pickaxeTier == 11 && migrated.inventory.count(ItemId::CopperOre) == 205
              && migrated.inventory.count(ItemId::IronOre) == 103
              && migrated.inventory.count(ItemId::GlowCrystal) == 7
              && migrated.inventory.occupiedSlots() == 6 && migrated.inventory.valid(),
              "legacy balances split into stacks without dropping any ore or adding starter coal");
        check(contents(path) == legacy, "loading a legacy profile does not alter it on disk");
        saveEquipment(migrated, path);
        check(loadEquipment(path) == migrated && contents(path).starts_with("VOXEL_PROGRESS 4"),
              "the next successful save upgrades the profile format");
        { std::ofstream file(path); file << "VOXEL_PROGRESS 1\nTIER 1\nORE 1980 0 0\n"; }
        check(loadEquipment(path).inventory.count(ItemId::CopperOre) == 1980,
              "a legacy balance that exactly fills twenty stacks is preserved");
        migrated.inventory = {};
        migrated.inventory.slots[7] = {ItemId::Ruby, 4};
        migrated.inventory.slots[19] = {ItemId::CopperOre, 99};
        saveEquipment(migrated, path);
        check(loadEquipment(path) == migrated, "sparse inventory slot arrangement is retained across saves");

        for (int tier = 1; tier <= GearTierCount; ++tier) {
            auto everyMaterial = expected;
            everyMaterial.pickaxeTier = everyMaterial.swordTier = tier;
            everyMaterial.armorTiers.fill(tier);
            saveEquipment(everyMaterial, path);
            check(loadEquipment(path) == everyMaterial, "all 108 material and slot variants survive a profile round trip");
        }

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
