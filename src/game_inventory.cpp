#include "game.h"
#include "input.h"

#include <algorithm>

namespace voxel {
const std::vector<ItemStack>& Game::chestContents() const
{
    static const std::vector<ItemStack> empty;
    return openChest_ ? loot_.contents(*openChest_) : empty;
}

void Game::closeInventory()
{
    inventoryOpen_ = false;
    openChest_.reset();
    inventorySelection_ = -1;
    chestName_.clear();
}

void Game::endSectionLoot()
{
    closeInventory();
    loot_.clear();
    lootPrompt_.clear();
    if (playtestEquipment_) {
        equipment = *playtestEquipment_;
        playtestEquipment_.reset();
    }
}

void Game::beginSectionLoot(std::span<const HubAsset> assets, bool playtest, std::uint32_t seed)
{
    endSectionLoot();
    if (playtest) playtestEquipment_ = equipment;
    loot_.begin(assets, seed);
    refreshLootPrompt();
}

void Game::refreshLootPrompt()
{
    lootPrompt_.clear();
    const auto target = loot_.target(world, player.eyePosition(), camera.forward());
    if (!target) return;
    const auto& contents = loot_.contents(*target);
    const bool empty = std::none_of(contents.begin(), contents.end(), [](const auto& stack) { return stack.count > 0; });
    lootPrompt_ = empty ? "E  EMPTY CHEST" : "E  OPEN " + std::string(loot_.name(*target));
}

bool Game::commitEquipment(const Equipment& next, std::string_view action)
{
    if (!playtestEquipment_ && !progressError_.empty()) {
        inventoryMessage_ = progressError_;
        return false;
    }
    try {
        // Save first. A failed write must not remove anything from the chest or
        // replace the previous inventory. Test loot never reaches this profile.
        if (!playtestEquipment_ && !progressPath_.empty()) saveEquipment(next, progressPath_);
        equipment = next;
        inventoryMessage_ = action;
        return true;
    } catch (const std::exception& error) {
        inventoryMessage_ = std::string("NOT SAVED: ") + error.what();
        return false;
    }
}

bool Game::updateInventory(const InputActions& actions)
{
    const bool testing = devMode_ && sectionEditor_.screen() == SectionEditorScreen::Test;
    const bool playing = !editorMode_ && ((!devMode_ && !levelController_.open() && !smithOpen_ && !crossingMenuOpen_)
        || testing || levelController_.screen() == LevelScreen::Play);
    if (!playing) { closeInventory(); lootPrompt_.clear(); return false; }
    refreshLootPrompt();
    if (inventoryOpen_ && actions.closeMenu) { closeInventory(); return true; }
    if (!actions.active) return inventoryOpen_;
    if (actions.inventory) {
        if (inventoryOpen_) closeInventory();
        else {
            inventoryOpen_ = true;
            inventoryMessage_ = testing ? "PLAYTEST BAG / LOOT IS TEMPORARY" : "SELECT TWO SLOTS TO MOVE OR MERGE A STACK";
        }
        return true;
    }
    if (!inventoryOpen_) {
        if (!actions.interact) return false;
        const auto target = loot_.target(world, player.eyePosition(), camera.forward());
        if (!target) return false;
        openChest_ = target;
        inventoryOpen_ = true;
        chestName_ = loot_.name(*target);
        inventoryMessage_ = "CLICK LOOT TO TAKE / ENTER TAKES ALL THAT FITS";
        return true;
    }
    if (actions.interact && openChest_) { closeInventory(); return true; }
    if (actions.inventorySort) {
        auto next = equipment;
        next.inventory.sort();
        if (commitEquipment(next, "BAG SORTED")) inventorySelection_ = -1;
    } else if (actions.inventoryClick) {
        const int slot = *actions.inventoryClick;
        if (slot < 0 || slot >= Inventory::SlotCount) return true;
        if (inventorySelection_ < 0) {
            if (equipment.inventory.slots[slot].count > 0) {
                inventorySelection_ = slot;
                inventoryMessage_ = "SELECT A DESTINATION / SELECT AGAIN TO CANCEL";
            }
        } else if (inventorySelection_ == slot) inventorySelection_ = -1;
        else {
            auto next = equipment;
            if (next.inventory.move(inventorySelection_, slot) && commitEquipment(next, "STACK MOVED"))
                inventorySelection_ = -1;
        }
    } else if (openChest_ && (actions.lootAll || actions.lootClick)) {
        auto remaining = loot_.contents(*openChest_);
        auto next = equipment;
        int taken = 0;
        for (std::size_t i = 0; i < remaining.size(); ++i) {
            if (!actions.lootAll && (!actions.lootClick || *actions.lootClick != static_cast<int>(i))) continue;
            auto& stack = remaining[i];
            if (stack.count <= 0) continue;
            const int left = next.inventory.add(stack.item, stack.count);
            taken += stack.count - left;
            stack.count = left;
        }
        const bool leftovers = std::any_of(remaining.begin(), remaining.end(), [](const auto& stack) { return stack.count > 0; });
        if (!taken) inventoryMessage_ = leftovers ? "BAG FULL / REMAINING LOOT STAYS HERE" : "CHEST EMPTY";
        else if (commitEquipment(next, "COLLECTED " + std::to_string(taken) + " ITEMS"
            + (leftovers ? " / REMAINING LOOT STAYS HERE" : " / CHEST EMPTY"))) {
            loot_.contents(*openChest_) = std::move(remaining);
        }
    }
    return true; // Inventory pauses movement, section timers and interactions.
}
} // namespace voxel
