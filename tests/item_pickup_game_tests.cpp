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
void approach(Game& game, const HubAsset& asset)
{
    close(game);
    require(game.player.setFeetPosition(game.world, asset.position + glm::vec3(0, 0, 2.5f)),
            "the pickup approach must be clear");
    game.camera.setPosition(game.player.eyePosition());
    const auto next = glm::normalize(asset.position + glm::vec3(0, .3f, 0) - game.camera.position());
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
}

int main()
{
    using namespace voxel;
    char pattern[] = "/tmp/voxel-item-pickups-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern), sections = root / "sections";
    int result = 0;
    try {
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
            InputActions idle; idle.active = true; game.update(idle);
            require(game.lootPrompt().find(std::string(item.name)) != std::string::npos,
                    "ground pickup prompt names its exact item");
            press(game, &InputActions::interact);
            require(game.equipment.inventory.count(item.id) == before + asset.pickupItem.count,
                    "E collects the exact authored ground stack");
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
        approach(resumed, assets[static_cast<int>(ItemId::Key)]);
        press(resumed, &InputActions::interact);
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
        press(partial, &InputActions::interact);
        require(partial.equipment.inventory.count(ItemId::Ruby) == 99 && visible(partial, ItemId::Ruby),
                "partially fitting pickup stays visible with its unclaimed remainder");
        InputActions idle; idle.active = true; partial.update(idle);
        require(partial.lootPrompt().find("4 Ruby") != std::string::npos,
                "the prompt shows the remaining quantity after a partial pickup");
        const auto full = partial.equipment;
        press(partial, &InputActions::interact);
        require(partial.inventoryOpen() && partial.equipment == full && visible(partial, ItemId::Ruby),
                "a full bag opens inventory feedback without losing ground items");
        close(partial);
        require(partial.levelController().start(partial, level), "partial claims survive replay");
        partial.equipment.inventory.slots[1] = {};
        approach(partial, ruby);
        press(partial, &InputActions::interact);
        require(partial.equipment.inventory.count(ItemId::Ruby) == 103 && !visible(partial, ItemId::Ruby),
                "only the remaining four items can be collected on replay");

        auto failedOptions = options;
        failedOptions.progressPath = (root / "missing" / "progress.vxp").string();
        Game failed(failedOptions);
        require(failed.levelController().start(failed, level), "failed-save pickup fixture loads");
        const auto beforeFailure = failed.equipment;
        approach(failed, ruby);
        press(failed, &InputActions::interact);
        require(failed.equipment == beforeFailure && visible(failed, ItemId::Ruby)
                && failed.inventoryMessage().find("NOT SAVED") != std::string::npos,
                "a failed save cannot consume a pickup or create a permanent claim");

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
        approach(builder, ruby);
        press(builder, &InputActions::interact);
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
