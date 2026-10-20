#include "inventory.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <set>
#include <string_view>

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
        Inventory bag;
        check(bag.valid() && bag.slots.size() == 20 && bag.occupiedSlots() == 0,
              "an empty inventory has exactly twenty available slots");
        check(bag.add(ItemId::CopperOre, 103) == 0
              && bag.slots[0] == ItemStack{ItemId::CopperOre, 99}
              && bag.slots[1] == ItemStack{ItemId::CopperOre, 4},
              "items stack up to ninety-nine before opening another slot");
        check(bag.move(0, 7) && bag.slots[0] == ItemStack{} && bag.slots[7].count == 99,
              "moving a stack to an empty slot preserves count and clears its old slot");
        check(bag.add(ItemId::CopperOre, 5) == 0 && bag.slots[1].count == 9 && bag.slots[0].count == 0,
              "new items fill matching stacks before using an earlier empty slot");
        check(bag.add(ItemId::Emerald, 3) == 0 && bag.move(0, 1)
              && bag.slots[0] == ItemStack{ItemId::CopperOre, 9}
              && bag.slots[1] == ItemStack{ItemId::Emerald, 3},
              "moving onto a different item swaps the complete stacks");
        check(!bag.move(0, 7) && bag.slots[0].count == 9,
              "moving onto a full matching stack leaves both stacks untouched");
        check(bag.move(7, 0) && bag.slots[0].count == 99 && bag.slots[7].count == 9,
              "a partial merge fills the destination and preserves the source remainder");
        check(bag.move(7, 8) && bag.slots[7] == ItemStack{},
              "moved empty stacks use the canonical empty value");
        const auto beforeFailure = bag;
        check(!bag.remove(ItemId::CopperOre, 109) && bag == beforeFailure,
              "removing too many items is atomic");
        check(bag.remove(ItemId::CopperOre, 105) && bag.count(ItemId::CopperOre) == 3
              && bag.slots[0] == ItemStack{} && bag.slots[8] == ItemStack{ItemId::CopperOre, 3},
              "removal can cross stacks and clears fully consumed slots");
        check(bag.remove(ItemId::IronOre, 0) && bag.add(ItemId::IronOre, 0) == 0,
              "zero-count operations do not create empty typed slots");
        const auto beforeInvalid = bag;
        check(!bag.move(-1, 0) && !bag.move(0, 20) && !bag.move(1, 1)
              && !bag.remove(ItemId::CopperOre, -1) && bag == beforeInvalid,
              "invalid moves and removals never change inventory");
        rejects([&] { bag.add(static_cast<ItemId>(-1), 1); }, "unknown items cannot be added");
        rejects([&] { bag.add(ItemId::Coal, -1); }, "negative item amounts cannot be added");

        bag = {};
        check(bag.add(ItemId::Coal, 1981) == 1 && bag.occupiedSlots() == 20
              && bag.count(ItemId::Coal) == 1980,
              "a full bag reports overflow instead of silently deleting items");
        check(bag.add(ItemId::GoldOre, 7) == 7 && bag.count(ItemId::GoldOre) == 0,
              "a different item cannot replace items in a full inventory");
        check(bag.remove(ItemId::Coal, 8) && bag.add(ItemId::Coal, 12) == 4
              && bag.count(ItemId::Coal) == 1980,
              "a full inventory still accepts matching items into a partial stack");
        check(bag.add(ItemId::Coal, std::numeric_limits<int>::max()) == std::numeric_limits<int>::max(),
              "very large positive input cannot overflow the remainder count");

        bag = {};
        bag.slots[2] = {ItemId::Emerald, 2};
        bag.slots[5] = {ItemId::CopperOre, 60};
        bag.slots[7] = {ItemId::Coal, 99};
        bag.slots[13] = {ItemId::CopperOre, 70};
        bag.slots[19] = {ItemId::Emerald, 5};
        bag.sort();
        check(bag.valid() && bag.occupiedSlots() == 4
              && bag.slots[0] == ItemStack{ItemId::Coal, 99}
              && bag.slots[1] == ItemStack{ItemId::CopperOre, 99}
              && bag.slots[2] == ItemStack{ItemId::CopperOre, 31}
              && bag.slots[3] == ItemStack{ItemId::Emerald, 7},
              "sort combines partial stacks, orders item types, and moves empty slots to the end");
        const auto sorted = bag;
        bag.sort();
        check(bag == sorted, "sorting twice leaves the same arrangement");
        for (const auto invalid : std::array<ItemStack, 4>{
                 ItemStack{ItemId::CopperOre, 0}, ItemStack{ItemId::Coal, -1},
                 ItemStack{ItemId::Coal, 100}, ItemStack{ItemId::Count, 1}}) {
            bag = {}; bag.slots[0] = invalid;
            check(!bag.valid(), "invalid counts, ids, and typed empty slots are rejected");
        }

        std::set<std::string_view> names;
        check(itemCatalog().size() == 19, "the catalog includes eighteen resources and a section key");
        for (const auto& item : itemCatalog()) {
            check(validItem(item.id) && &itemInfo(item.id) == &item
                  && !item.name.empty() && !item.description.empty() && !item.rarity.empty()
                  && names.insert(item.name).second,
                  "each catalog entry has a stable id and distinct descriptive metadata");
            check(item.color.r >= 0 && item.color.r <= 1 && item.color.g >= 0 && item.color.g <= 1
                  && item.color.b >= 0 && item.color.b <= 1,
                  "catalog icon colors are within display range");
        }
        rejects([&] { static_cast<void>(itemInfo(ItemId::Count)); }, "unknown item metadata is rejected");
        std::vector<ItemStack> authored;
        check(validChestItems(authored), "new chests may be empty");
        for (int slot = 0; slot < ChestSlotCount; ++slot)
            authored.push_back({static_cast<ItemId>(slot % ItemCount), ChestStackLimit});
        check(validChestItems(authored), "authored chests accept exactly twenty full stacks including repeated items");
        authored.push_back({ItemId::Ruby, 1});
        check(!validChestItems(authored), "a twenty-first authored stack is rejected");
        authored.pop_back();
        for (const auto bad : {ItemStack{ItemId::Coal, 0}, ItemStack{ItemId::Coal, -1},
                               ItemStack{ItemId::Coal, ChestStackLimit + 1}, ItemStack{ItemId::Count, 1}}) {
            authored[0] = bad;
            check(!validChestItems(authored), "authored chest rows require valid ids and positive bounded amounts");
        }
        std::set<ItemId> obtainable;
        for (std::uint32_t variation = 0; variation < 3; ++variation) {
            bool varied = false;
            const auto first = legacyChestLoot(variation, 0);
            for (std::uint32_t seed = 0; seed < 128; ++seed) {
                const auto loot = legacyChestLoot(variation, seed);
                check(loot == legacyChestLoot(variation, seed), "legacy migration rolls remain repeatable");
                check(loot.size() >= 4 && loot.size() <= 6, "legacy migration preserves the old stack count");
                std::set<ItemId> unique;
                for (const auto& stack : loot) {
                    check(validItem(stack.item) && stack.count > 0 && stack.count <= Inventory::StackLimit
                          && unique.insert(stack.item).second, "chest stacks contain distinct, valid collectible items");
                    obtainable.insert(stack.item);
                }
                varied |= loot != first;
            }
            check(varied, "legacy seed differences remain compatible during migration");
        }
        check(obtainable.size() == 18 && !obtainable.contains(ItemId::Key),
              "legacy migration supports all original resources without inventing new section keys");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    return failures ? 1 : 0;
}
