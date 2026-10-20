#include "game.h"
#include "input.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
void press(Game& game, bool InputActions::* key)
{
    InputActions action;
    action.active = true;
    action.*key = true;
    game.update(action);
}
void close(Game& game)
{
    if (game.inventoryOpen()) press(game, &InputActions::closeMenu);
}
void tick(Game& game)
{
    InputActions idle; idle.active = true; game.update(idle, .1f);
}
void walkOver(Game& game, const HubAsset& asset)
{
    close(game);
    require(game.player.setFeetPosition(game.world, asset.position + glm::vec3(0, 0, .7f)),
            "dropped items do not block a walking approach");
    game.camera.setPosition(game.player.eyePosition());
    for (int frame = 0; frame < 3; ++frame) tick(game);
}
void approach(Game& game, const HubAsset& asset)
{
    close(game);
    require(game.player.setFeetPosition(game.world, asset.position + glm::vec3(0, 0, 2.5f)),
            "the pickup approach must be clear");
    game.camera.setPosition(game.player.eyePosition());
    const auto next = glm::normalize(asset.position + PickupVisualCenter - game.camera.position());
    const auto previous = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(next.z, next.x) - std::atan2(previous.z, previous.x)),
                       glm::degrees(std::asin(next.y) - std::asin(previous.y)));
}
bool visible(const Game& game, ItemId item)
{
    return std::any_of(game.sectionSceneAssets().begin(), game.sectionSceneAssets().end(),
        [&](const HubAsset& asset) {
            return isItemPickup(asset.kind) && itemPickupStack(asset).item == item;
        });
}
std::string read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void proximityRules()
{
    World world;
    buildBlankSection(world);
    const HubAsset drop{HubAssetKind::ItemPickup, {.5f, 9, .5f}, 0, 0, {}, {ItemId::Ruby, 7}};
    const HubAsset legacy{HubAssetKind::KeyPickup, {8.5f, 9, .5f}};
    LootSession loot;
    loot.begin(std::array{drop, legacy});
    require(loot.collision()->empty(), "generic and legacy dropped items never create physical obstacles");
    require(loot.canCollectPickup(0, world, {1.99f, 9, .5f})
            && !loot.canCollectPickup(0, world, {2.01f, 9, .5f})
            && !loot.canCollectPickup(0, world, {.5f, 9.81f, .5f}),
            "magnetic reach is bounded horizontally and vertically");
    require(!loot.canCollectPickup(0, world, {.5f, 7, .5f})
            && !loot.canCollectPickup(0, world, {.5f, 11, .5f}),
            "pickups cannot be collected through the floor below or above");
    world.set({1, 9, 0}, Block::Stone);
    require(!loot.canCollectPickup(0, world, {1.99f, 9, .5f}),
            "a solid voxel between the player and item blocks magnetic collection");
    world.set({1, 9, 0}, Block::Air);
    const HubAsset chest{HubAssetKind::LootChest, {1.4f, 9, .5f}, 0, 0, {{ItemId::Coal, 1}}};
    loot.begin(std::array{drop, chest});
    require(!loot.canCollectPickup(0, world, {1.99f, 9, .5f}),
            "solid scenery between a nearby player and item blocks magnetic collection");
    loot.begin(std::array{drop, legacy});
    const auto authored = loot.sceneAssets();
    const auto collision = loot.collision();
    require(!loot.pullPickup(0, {1.8f, 9, .5f}, .05f)
            && glm::length(loot.scenePickupOffsets()[0]) > .1f
            && loot.sceneAssets() == authored && loot.collision() == collision,
            "a pickup visibly moves toward the player before collection without mutating static assets");
    require(!loot.pullPickup(0, {1.8f, 9, .5f}, 0)
            && !loot.pullPickup(0, {1.8f, 9, .5f}, -.1f),
            "invalid or zero frame time cannot advance pickup attraction");
    require(loot.pullPickup(0, {1.8f, 9, .5f}, .1f),
            "a short magnetic flight reaches the player before inventory transfer");
    loot.stopPickupPull(0);
    require(loot.scenePickupOffsets()[0] == glm::vec3(0),
            "interrupted pickup attraction returns the item to its authored anchor");
    loot.contents(0)[0].count = 0;
    loot.refreshAssets();
    require(loot.sceneAssets().size() == 1 && loot.scenePickupOffsets().size() == 1
            && loot.sceneAssets()[0] == legacy && loot.collision() == collision,
            "collection keeps dynamic offsets aligned without rebuilding physical scenery");
}
}

int main()
{
    using namespace voxel;
    char pattern[] = "/tmp/voxel-item-pickups-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern), sections = root / "sections";
    int result = 0;
    try {
        proximityRules();
        World room;
        buildBlankSection(room);
        SectionMetadata meta;
        meta.name = "Ground rewards";
        meta.tested = true;
        std::vector<HubAsset> assets;
        for (const auto& item : itemCatalog()) {
            const int index = static_cast<int>(item.id);
            assets.push_back({HubAssetKind::ItemPickup,
                {-12.5f + (index % 5) * 5, 9, 11.5f - (index / 5) * 5}, 0, 0, {},
                {item.id, item.id == ItemId::Key ? 1 : 7}});
        }
        assets.push_back({HubAssetKind::IronboundChest, {17.5f, 9, .5f}, 0, 0, {{ItemId::Ruby, 2}}});
        const auto section = acceptSection(room, meta, assets, sections);
        LevelDefinition level;
        level.name = "Pickup expedition";
        level.sections = {section.lexically_relative(sections)};
        Options options;
        options.sectionLibrary = sections.string();
        options.levelLibrary = (root / "levels").string();
        options.progressPath = (root / "progress.vxp").string();
        Game game(options);
        require(game.levelController().start(game, level), "saved pickups load into an expedition");
        for (const auto& item : itemCatalog()) {
            const auto& asset = assets[static_cast<int>(item.id)];
            const int before = game.equipment.inventory.count(item.id);
            approach(game, asset);
            tick(game);
            require(game.lootPrompt().find(std::string(item.name)) != std::string::npos
                    && game.lootPrompt().find("WALK OVER") != std::string::npos,
                    "ground pickup prompt names its exact item and walk-over interaction");
            press(game, &InputActions::interact);
            require(game.equipment.inventory.count(item.id) == before,
                    "E does not remotely collect a ground item");
            require(game.player.setFeetPosition(game.world, asset.position),
                    "uncollected dropped items have no physical collision");
            const auto staticScene = std::vector<HubAsset>(game.sectionSceneAssets().begin(), game.sectionSceneAssets().end());
            tick(game);
            require(visible(game, item.id) && game.equipment.inventory.count(item.id) == before
                    && std::equal(game.sectionSceneAssets().begin(), game.sectionSceneAssets().end(), staticScene.begin()),
                    "items remain visible during the first flight frame before entering the bag");
            walkOver(game, asset);
            require(game.equipment.inventory.count(item.id) == before + asset.pickupItem.count,
                    "walking over collects the exact authored ground stack");
            require(!visible(game, item.id), "collected pickup disappears from the scene");
            require(game.player.setFeetPosition(game.world, asset.position),
                    "collected pickup leaves no invisible collision");
        }
        require(game.equipment.inventory.count(ItemId::Key) == 1, "generic key pickup enters the inventory");
        const auto collected = game.equipment;
        press(game, &InputActions::returnToHub);
        require(game.equipment.inventory.count(ItemId::Key) == 0, "generic keys cannot leave their section");
        Game resumed(options);
        require(resumed.levelController().start(resumed, level), "pickup collection survives restarting the game");
        for (const auto& item : itemCatalog()) {
            require(visible(resumed, item.id) == (item.id == ItemId::Key),
                    "ordinary ground items stay gone, but an unused key returns to its original source");
            if (item.id != ItemId::Key)
                require(resumed.equipment.inventory.count(item.id) == collected.inventory.count(item.id),
                        "all ordinary pickups remain in the saved inventory");
        }
        walkOver(resumed, assets[static_cast<int>(ItemId::Key)]);
        approach(resumed, assets.back());
        press(resumed, &InputActions::interact);
        require(resumed.chestOpen() && resumed.equipment.inventory.count(ItemId::Key) == 0,
                "a key from the Items tab unlocks the same Ironbound chests as a legacy key");
        close(resumed);
        require(resumed.levelController().start(resumed, level), "a section with a used generic key can be replayed");
        require(!visible(resumed, ItemId::Key), "a spent generic key never respawns");

        // Partial transfers retain the unclaimed quantity and never duplicate it.
        auto partialOptions = options;
        partialOptions.progressPath = (root / "partial.vxp").string();
        Game partial(partialOptions);
        require(partial.levelController().start(partial, level), "partial pickup fixture loads");
        for (auto& slot : partial.equipment.inventory.slots) slot = {ItemId::Coal, Inventory::StackLimit};
        partial.equipment.inventory.slots[0] = {ItemId::Ruby, 96};
        const auto& ruby = assets[static_cast<int>(ItemId::Ruby)];
        approach(partial, ruby);
        walkOver(partial, ruby);
        require(partial.equipment.inventory.count(ItemId::Ruby) == 99 && visible(partial, ItemId::Ruby),
                "partially fitting pickup stays visible with its unclaimed remainder");
        approach(partial, ruby);
        tick(partial);
        require(partial.lootPrompt().find("4 Ruby") != std::string::npos,
                "the prompt shows the remaining quantity after a partial pickup");
        const auto full = partial.equipment;
        walkOver(partial, ruby);
        require(!partial.inventoryOpen() && partial.equipment == full && visible(partial, ItemId::Ruby),
                "a full bag never forces the inventory open or loses ground items");
        partial.equipment.inventory.slots[1] = {};
        for (int frame = 0; frame < 3; ++frame) tick(partial);
        require(partial.equipment.inventory.count(ItemId::Ruby) == 103 && !visible(partial, ItemId::Ruby),
                "making bag space retries a stationary pickup and transfers only its remaining amount");
        close(partial);
        require(partial.levelController().start(partial, level), "partial claims survive replay");
        walkOver(partial, ruby);
        require(partial.equipment.inventory.count(ItemId::Ruby) == 103 && !visible(partial, ItemId::Ruby),
                "fully collected partial pickups cannot duplicate on replay");

        auto failedOptions = options;
        failedOptions.progressPath = (root / "missing" / "progress.vxp").string();
        Game failed(failedOptions);
        require(failed.levelController().start(failed, level), "failed-save pickup fixture loads");
        const auto beforeFailure = failed.equipment;
        walkOver(failed, ruby);
        require(failed.equipment == beforeFailure && visible(failed, ItemId::Ruby)
                && !failed.inventoryOpen() && failed.inventoryMessage().find("NOT SAVED") != std::string::npos,
                "a failed save cannot consume a pickup or create a permanent claim");
        std::filesystem::create_directory(root / "missing");
        for (int frame = 0; frame < 20; ++frame) tick(failed);
        require(!std::filesystem::exists(failedOptions.progressPath) && failed.equipment == beforeFailure,
                "a failed automatic save does not retry every stationary frame");
        approach(failed, ruby); tick(failed);
        walkOver(failed, ruby);
        require(failed.equipment.inventory.count(ItemId::Ruby) == 7 && !visible(failed, ItemId::Ruby),
                "leaving and returning to pickup reach retries a repaired save destination");

        auto pausedOptions = options;
        pausedOptions.progressPath.clear();
        Game paused(pausedOptions);
        require(paused.levelController().start(paused, level), "paused pickup fixture loads");
        require(paused.player.setFeetPosition(paused.world, ruby.position + glm::vec3(1.2f, 0, 0)),
                "paused fixture can approach its item");
        InputActions inactive;
        for (int frame = 0; frame < 4; ++frame) paused.update(inactive, .1f);
        require(visible(paused, ItemId::Ruby) && paused.equipment.inventory.count(ItemId::Ruby) == 0,
                "paused gameplay neither attracts nor collects nearby items");
        InputActions active; active.active = true;
        for (int frame = 0; frame < 3; ++frame) paused.update(active, 1.f / 60);
        const auto offsets = std::vector<glm::vec3>(paused.sectionPickupOffsets().begin(), paused.sectionPickupOffsets().end());
        require(visible(paused, ItemId::Ruby) && paused.equipment.inventory.count(ItemId::Ruby) == 0
                && std::any_of(offsets.begin(), offsets.end(), [](glm::vec3 offset) { return glm::length(offset) > .1f; }),
                "nearby drops visibly fly toward the player for several frames before collection");
        for (int frame = 0; frame < 4; ++frame) paused.update(inactive, .1f);
        require(std::equal(offsets.begin(), offsets.end(), paused.sectionPickupOffsets().begin())
                && paused.equipment.inventory.count(ItemId::Ruby) == 0,
                "pausing freezes a pickup already in flight");
        press(paused, &InputActions::inventory);
        for (int frame = 0; frame < 4; ++frame) tick(paused);
        require(visible(paused, ItemId::Ruby) && paused.equipment.inventory.count(ItemId::Ruby) == 0,
                "an open inventory pauses magnetic pickups");
        close(paused);
        approach(paused, ruby); tick(paused);
        require(visible(paused, ItemId::Ruby)
                && std::all_of(paused.sectionPickupOffsets().begin(), paused.sectionPickupOffsets().end(),
                               [](glm::vec3 offset) { return offset == glm::vec3(0); }),
                "leaving attraction range returns the waiting drop to its original hover position");
        InputActions movement; movement.active = true; movement.movement = {0, 1};
        for (int frame = 0; frame < 8; ++frame) paused.update(movement, .1f);
        require(!visible(paused, ItemId::Ruby) && paused.equipment.inventory.count(ItemId::Ruby) == 7,
                "walking toward a drop attracts and collects it without an interaction button");

        // Editor tests start with fresh pickups and never change real progress.
        auto devOptions = options;
        devOptions.dev = true;
        Game builder(devOptions);
        const auto beforeTest = builder.equipment;
        const auto saved = read(options.progressPath);
        const auto click = [&](int id) {
            InputActions action; action.active = true; action.devClick = id; builder.update(action);
        };
        click(SectionUiWorldBase);
        click(SectionUiLibraryEntryBase);
        press(builder, &InputActions::devTest);
        require(visible(builder, ItemId::Ruby), "F8 restores authored pickups despite permanent gameplay claims");
        walkOver(builder, ruby);
        press(builder, &InputActions::devTest);
        require(builder.equipment == beforeTest && read(options.progressPath) == saved,
                "F8 pickup tests restore the real bag and permanent collection history");
        std::cout << "Ground item pickup gameplay passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return result;
}
