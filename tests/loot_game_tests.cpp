#include "game.h"
#include "input.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void press(Game& game, bool InputActions::* key)
{
    InputActions action; action.active = true; action.*key = true; game.update(action, 1.f / 60);
}
void click(Game& game, int id)
{
    InputActions action; action.active = true; action.devClick = id; game.update(action);
}
void aim(Game& game, glm::vec3 point)
{
    game.camera.setPosition(game.player.eyePosition());
    const auto direction = glm::normalize(point - game.camera.position()), previous = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(previous.z, previous.x)),
                       glm::degrees(std::asin(direction.y) - std::asin(previous.y)));
}
int total(const std::vector<ItemStack>& stacks)
{
    return std::accumulate(stacks.begin(), stacks.end(), 0, [](int n, const ItemStack& s) { return n + s.count; });
}
std::string read(const std::filesystem::path& path)
{
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}

int main()
{
    char pattern[] = "/tmp/voxel-loot-game-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern), sections = root / "sections", profile = root / "progress.vxp";
    int result = 0;
    try {
        World room;
        buildBlankSection(room);
        SectionMetadata meta;
        meta.name = "Chest room"; meta.tested = true;
        meta.spawn = {.5f, 9, 4.5f}; meta.finish = {.5f, 9, -20.5f};
        const HubAsset chest{HubAssetKind::LootChest, {.5f, 9, 1.5f}, 0, 1};
        const auto section = acceptSection(room, meta, std::span(&chest, 1), sections);
        SectionMetadata empty = meta; empty.name = "Empty room";
        const auto emptySection = acceptSection(room, empty, {}, sections);
        LevelDefinition level;
        level.name = "Loot expedition"; level.number = 1;
        level.sections = {section.lexically_relative(sections), emptySection.lexically_relative(sections)};
        Options options; options.sectionLibrary = sections.string();
        options.levelLibrary = (root / "levels").string(); options.progressPath = profile.string();
        Game game(options);
        require(game.equipment.inventory.slots.size() == 20 && game.equipment.inventory.count(ItemId::CopperOre) == 12,
                "twenty inventory slots coexist with the three starter tools");
        press(game, &InputActions::inventory);
        require(game.inventoryOpen() && game.menuOpen(), "I opens the inventory as a modal");
        const auto feet = game.player.feetPosition();
        InputActions movement; movement.active = true; movement.movement = {0, 1}; movement.jump = true;
        game.update(movement, .1f);
        require(game.player.feetPosition() == feet, "open inventory pauses player movement");
        press(game, &InputActions::closeMenu);
        require(!game.inventoryOpen(), "Escape closes inventory");

        require(game.levelController().start(game, level), "accepted chest sections load into expeditions");
        aim(game, chest.position + glm::vec3(0, .65f, 0));
        // Interaction needs both reach and a clear view of the real model.
        game.world.set({0, 10, 3}, Block::Stone);
        press(game, &InputActions::interact);
        require(!game.chestOpen(), "a solid wall prevents remote chest interaction");
        game.world.set({0, 10, 3}, Block::Air);
        require(game.player.setFeetPosition(game.world, {.5f, 9, 8.5f}), "far chest fixture fits");
        aim(game, chest.position + glm::vec3(0, .65f, 0));
        press(game, &InputActions::interact);
        require(!game.chestOpen(), "chests beyond four meters cannot be opened");
        require(game.player.setFeetPosition(game.world, meta.spawn), "return to chest approach");
        aim(game, chest.position + glm::vec3(0, .65f, 0));
        press(game, &InputActions::interact);
        require(game.chestOpen() && !game.chestContents().empty(), "E opens an aimed chest with generated loot");
        const auto initial = game.chestContents();
        const auto first = initial.front();
        const auto elapsed = game.levelController().elapsed();
        InputActions take; take.active = true; take.lootClick = 0;
        const int owned = game.equipment.inventory.count(first.item);
        game.update(take, .1f);
        require(game.equipment.inventory.count(first.item) == owned + first.count && game.chestContents()[0].count == 0,
                "clicking chest loot transfers it once");
        require(loadEquipment(profile).inventory == game.equipment.inventory,
                "collected loot is persisted immediately");
        require(game.levelController().elapsed() == elapsed, "looting pauses the expedition timer");
        game.update(take);
        require(game.equipment.inventory.count(first.item) == owned + first.count, "an empty loot row cannot duplicate items");
        press(game, &InputActions::lootAll);
        require(total(game.chestContents()) == 0, "Take All collects the remaining stacks");
        const auto collected = game.equipment;
        press(game, &InputActions::closeMenu);
        press(game, &InputActions::respawn);
        aim(game, chest.position + glm::vec3(0, .65f, 0));
        press(game, &InputActions::interact);
        require(game.chestOpen() && total(game.chestContents()) == 0, "checkpoint resets do not refill chests");
        press(game, &InputActions::lootAll);
        require(game.equipment == collected, "empty chest reopen does not change inventory");
        press(game, &InputActions::closeMenu);

        require(game.levelController().start(game, level), "a fresh run prepares fresh chest contents");
        aim(game, chest.position + glm::vec3(0, .65f, 0));
        press(game, &InputActions::interact);
        const auto rolled = game.chestContents();
        for (auto& slot : game.equipment.inventory.slots) slot = {ItemId::Coal, Inventory::StackLimit};
        game.equipment.inventory.slots[0] = {rolled[0].item, Inventory::StackLimit - 3};
        const int remainingBefore = total(rolled);
        press(game, &InputActions::lootAll);
        require(game.equipment.inventory.slots[0].count == Inventory::StackLimit
            && total(game.chestContents()) == remainingBefore - 3,
            "a nearly full bag accepts only its remaining capacity and keeps all excess in the chest");
        const auto full = game.equipment;
        const auto leftover = game.chestContents();
        press(game, &InputActions::lootAll);
        require(game.equipment == full && game.chestContents() == leftover,
                "a completely full bag loses and duplicates nothing");
        press(game, &InputActions::closeMenu);
        // Replacing a section clears chest state; it cannot remain open over the next room.
        require(game.player.setFeetPosition(game.world, meta.finish), "finish fixture fits");
        InputActions idle; idle.active = true;
        for (int i = 0; i < 5; ++i) game.update(idle, .1f);
        require(game.levelController().sectionIndex() == 1 && !game.chestOpen() && game.lootPrompt().empty(),
                "section transitions clear old chest interactions");
        press(game, &InputActions::returnToHub);
        require(game.equipment == full && !game.levelActive(), "expedition loot returns to the hub");

        // Sorting and moving use the same atomic inventory profile transaction.
        game.equipment = collected;
        press(game, &InputActions::inventory);
        InputActions slot; slot.active = true; slot.inventoryClick = 0; game.update(slot);
        slot.inventoryClick = 19; game.update(slot);
        require(game.equipment.inventory.slots[19].count > 0 && game.equipment.inventory.slots[0].count == 0,
                "two slot clicks move a stack into an empty slot");
        press(game, &InputActions::inventorySort);
        require(game.equipment.inventory.slots[0].count > 0
            && loadEquipment(profile).inventory == game.equipment.inventory, "Sort compacts stacks and saves layout");
        press(game, &InputActions::closeMenu);

        // F8 test runs have a temporary bag and never modify the real profile.
        Options dev = options; dev.dev = true;
        Game builder(dev);
        const auto beforeTest = builder.equipment;
        const auto savedBytes = read(profile);
        click(builder, SectionUiWorldBase);
        const auto& entries = builder.sectionEditor().entries();
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& item) { return item.path == section; });
        require(entry != entries.end(), "accepted chest room is listed in the builder");
        click(builder, SectionUiLibraryEntryBase + static_cast<int>(entry - entries.begin()));
        press(builder, &InputActions::devTest);
        require(builder.sectionEditor().screen() == SectionEditorScreen::Test, "F8 begins a chest playtest");
        aim(builder, chest.position + glm::vec3(0, .65f, 0));
        press(builder, &InputActions::interact);
        require(builder.chestOpen(), "E opens loot chests during F8 playtests");
        press(builder, &InputActions::lootAll);
        require(builder.equipment != beforeTest && read(profile) == savedBytes,
                "test loot changes the temporary bag but never the saved profile");
        press(builder, &InputActions::closeMenu);
        press(builder, &InputActions::devTest);
        require(builder.equipment == beforeTest && read(profile) == savedBytes,
                "returning to Build restores the real bag without test loot");

        Options broken = options; broken.progressPath = (root / "missing" / "progress.vxp").string();
        Game unsaved(broken);
        require(unsaved.levelController().start(unsaved, level), "failed-save fixture can still enter expeditions");
        aim(unsaved, chest.position + glm::vec3(0, .65f, 0));
        press(unsaved, &InputActions::interact);
        const auto bagBefore = unsaved.equipment;
        const auto chestBefore = unsaved.chestContents();
        press(unsaved, &InputActions::lootAll);
        require(unsaved.equipment == bagBefore && unsaved.chestContents() == chestBefore
            && unsaved.inventoryMessage().find("NOT SAVED") != std::string::npos,
            "profile write failures preserve both chest contents and player inventory");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result = 1;
    }
    std::filesystem::remove_all(root);
    return result;
}
