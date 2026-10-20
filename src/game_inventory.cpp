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
    discardSectionKeys();
    loot_.clear();
    lootPrompt_.clear();
    sectionLootActive_ = false;
    if (playtestEquipment_) {
        equipment = *playtestEquipment_;
        playtestEquipment_.reset();
    }
}

void Game::discardSectionKeys()
{
    // Unused keys were never permanently claimed. Their original sources become
    // available on the next visit, including after closing the game.
    for (auto& slot : equipment.inventory.slots)
        if (slot.item == ItemId::Key) slot = {};
    heldKeys_.clear();
}

std::span<const HubAsset> Game::sectionSceneAssets() const
{
    if (sectionLootActive_) return loot_.sceneAssets();
    if (devMode_) return sectionEditor_.assets();
    if (levelController_.active()) return levelController_.assets();
    return {};
}

void Game::beginSectionLoot(std::span<const HubAsset> assets, bool playtest, std::string_view section)
{
    endSectionLoot();
    if (playtest) playtestEquipment_ = equipment;
    // Authoring always tests a fresh room without touching real collection history.
    loot_.begin(assets, section, playtest ? std::map<std::string, LootProgress>{} : equipment.lootProgress);
    sectionLootActive_ = true;
    player.setAssetCollision(loot_.collision());
    player.setFeetPosition(world, player.feetPosition());
    refreshLootPrompt();
}

void Game::refreshLootPrompt()
{
    lootPrompt_.clear();
    const auto target = loot_.target(world, player.eyePosition(), camera.forward());
    if (!target) return;
    if (loot_.pickup(*target)) {
        lootPrompt_ = "E  COLLECT CAVE KEY / THIS SECTION ONLY";
        return;
    }
    if (loot_.locked(*target)) {
        lootPrompt_ = equipment.inventory.count(ItemId::Key) > 0
            ? "E  UNLOCK IRONBOUND CHEST / USE 1 KEY"
            : "LOCKED CHEST / FIND A CAVE KEY IN THIS SECTION";
        return;
    }
    const auto& contents = loot_.contents(*target);
    const bool empty = std::none_of(contents.begin(), contents.end(), [](const auto& stack) { return stack.count > 0; });
    lootPrompt_ = empty ? "E  EMPTY CHEST" : "E  OPEN " + std::string(loot_.name(*target));
}

bool Game::openLoot(std::size_t index)
{
    if (loot_.pickup(index)) {
        takeLoot(index);
        return false;
    }
    if (loot_.locked(index)) {
        auto receipt = std::find_if(heldKeys_.begin(), heldKeys_.end(), [](const auto& key) { return key.count > 0; });
        if (equipment.inventory.count(ItemId::Key) == 0 || receipt == heldKeys_.end()) {
            inventoryMessage_ = "LOCKED / FIND A CAVE KEY IN THIS SECTION";
            return false;
        }
        auto next = equipment;
        next.inventory.remove(ItemId::Key, 1);
        ++next.lootProgress[receipt->source].taken[receipt->slot];
        next.lootProgress[loot_.recordKey(index)].unlocked = true;
        if (!commitEquipment(next, "UNLOCKED / ONE CAVE KEY USED")) {
            inventoryOpen_ = true;
            return false;
        }
        --receipt->count;
        loot_.unlock(index);
    } else inventoryMessage_ = "CLICK LOOT TO TAKE / ENTER TAKES ALL THAT FITS";
    openChest_ = index;
    inventoryOpen_ = true;
    chestName_ = loot_.name(index);
    return true;
}

void Game::takeLoot(std::size_t index, std::optional<std::size_t> selected)
{
    if (loot_.locked(index)) return;
    auto remaining = loot_.contents(index);
    auto next = equipment;
    auto keys = heldKeys_;
    int taken = 0, keyCount = 0;
    for (std::size_t i = 0; i < remaining.size(); ++i) {
        if (selected && *selected != i) continue;
        auto& stack = remaining[i];
        if (stack.count <= 0) continue;
        const int left = next.inventory.add(stack.item, stack.count);
        const int collected = stack.count - left;
        if (collected == 0) continue;
        taken += collected;
        stack.count = left;
        if (stack.item == ItemId::Key) {
            // Reserve keys in memory; only spending one permanently claims its source.
            keys.push_back({loot_.recordKey(index), i, collected});
            keyCount += collected;
        } else next.lootProgress[loot_.recordKey(index)].taken[i] += collected;
    }
    const bool leftovers = std::any_of(remaining.begin(), remaining.end(), [](const auto& stack) { return stack.count > 0; });
    if (!taken) {
        inventoryMessage_ = leftovers ? "BAG FULL / REMAINING LOOT STAYS HERE" : "CHEST EMPTY";
        if (loot_.pickup(index)) inventoryOpen_ = true;
    } else if (commitEquipment(next, keyCount > 0 ? "KEY COLLECTED / USE IN THIS SECTION OR FIND IT AGAIN LATER"
        : "COLLECTED " + std::to_string(taken) + " ITEMS / SAVED PERMANENTLY")) {
        loot_.contents(index) = std::move(remaining);
        heldKeys_ = std::move(keys);
        const auto previousCollision = loot_.collision();
        loot_.refreshAssets();
        if (loot_.collision() != previousCollision) player.setAssetCollision(loot_.collision());
    } else if (loot_.pickup(index)) inventoryOpen_ = true;
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
        openLoot(*target);
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
        if (actions.lootAll) takeLoot(*openChest_);
        else if (*actions.lootClick >= 0) takeLoot(*openChest_, static_cast<std::size_t>(*actions.lootClick));
    }
    return true; // Inventory pauses movement, section timers and interactions.
}
} // namespace voxel
