#include "game.h"
#include "input.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void update(Game& game)
{
    InputActions action; action.active = true; game.update(action);
}

void press(Game& game, bool InputActions::* key)
{
    InputActions action; action.active = true; action.*key = true; game.update(action);
}

void click(Game& game, int id, bool shift = false)
{
    InputActions action; action.active = true; action.devClick = id; action.sprint = shift;
    game.update(action);
}

void aim(Camera& camera, glm::vec3 point)
{
    const auto direction = glm::normalize(point - camera.position()), old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}

glm::vec3 center(const HubAsset& asset)
{
    if (isItemPickup(asset.kind)) return asset.position + PickupVisualCenter;
    glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
    for (const auto& vertex : buildHubAsset(asset).vertices) {
        low = glm::min(low, vertex.position); high = glm::max(high, vertex.position);
    }
    return (low + high) * .5f;
}

void pointAt(Game& game, const HubAsset& asset)
{
    const auto point = center(asset);
    game.camera.setPosition(point + glm::vec3(0, 2, 3));
    aim(game.camera, point); update(game);
}

void floorAt(Game& game, float x, float z)
{
    game.camera.setPosition({x, 13, z + 3});
    aim(game.camera, {x, 8.5f, z}); update(game);
}

void openPalette(Game& game)
{
    if (game.sectionEditor().screen() != SectionEditorScreen::Palette)
        press(game, &InputActions::devPalette);
    require(game.sectionEditor().screen() == SectionEditorScreen::Palette, "creative palette must open");
}

void collect(Game& game, const HubAsset& asset)
{
    require(game.player.setFeetPosition(game.world, asset.position),
            "dropped items must not block the player");
    game.camera.setPosition(game.player.eyePosition());
    aim(game.camera, game.camera.position() + glm::vec3(0, 0, 1));
    InputActions walking; walking.active = true;
    for (int frame = 0; frame < 30; ++frame) game.update(walking, 1.0f / 60);
}
}

int main()
{
    using namespace voxel;
    char pattern[] = "/tmp/voxel-pickup-editor-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern);
    int result = 0;
    try {
        World world; buildBlankSection(world);
        const HubAsset legacy{HubAssetKind::KeyPickup, {24.5f, 9, -20.5f}};
        const HubAsset boulder{HubAssetKind::Boulder, {24.5f, 9, 16.5f}};
        const auto draft = saveSectionDraft(world, {}, std::array{legacy, boulder}, root / "sections");
        Options options; options.dev = true; options.sectionLibrary = (root / "sections").string();
        options.progressPath = (root / "profile.vxp").string();
        Game game(options); auto& editor = game.sectionEditor();
        click(game, SectionUiWorldBase); click(game, SectionUiLibraryDraftBase);
        const auto originalEquipment = game.equipment;
        const auto unchangedRevision = editor.revision();
        openPalette(game);
        click(game, SectionUiPaletteTabBase + 1);
        const auto sceneryCount = std::count_if(sectionAssetCatalog().begin(), sectionAssetCatalog().end(),
            [](const auto& entry) { return !isItemPickup(entry.kind); });
        require(editor.paletteItems().size() == static_cast<std::size_t>(sceneryCount)
                && std::none_of(editor.paletteItems().begin(), editor.paletteItems().end(),
                    [](const auto& entry) { return entry.pickup || (entry.asset && isItemPickup(*entry.asset)); }),
                "Assets contains every scenery/chest model but no legacy or generic pickups");
        InputActions tab; tab.active = true; tab.devTabStep = 1; game.update(tab);
        require(editor.paletteTab() == 3, "Tab follows the displayed Assets to Items order");
        require(editor.paletteItems().size() == itemCatalog().size(), "Items exposes every ore and cave key");
        for (const auto& info : itemCatalog()) {
            const auto& entry = editor.paletteItems()[static_cast<int>(info.id)];
            require(entry.pickup == info.id && entry.asset == HubAssetKind::ItemPickup
                    && !entry.block && entry.marker == SectionMarker::None && entry.name == info.name,
                    "each Items entry has an explicit item payload and generic pickup model");
        }
        game.update(tab);
        require(editor.paletteTab() == 2 && editor.paletteItems().size() == 3,
                "Markers keeps its original tab ID after Items");
        tab.devTabStep = -1; game.update(tab);
        require(editor.paletteTab() == 3, "reverse Tab returns from Markers to Items");

        click(game, SectionUiPickupMinus, true);
        require(editor.pickupQuantity() == 1, "pickup quantity cannot fall below one");
        click(game, SectionUiPickupPlus, true); click(game, SectionUiPickupPlus);
        require(editor.pickupQuantity() == 12, "pickup quantity supports Shift-ten and single increments");
        for (int i = 0; i < 10; ++i) click(game, SectionUiPickupPlus, true);
        require(editor.pickupQuantity() == ChestStackLimit, "pickup quantity cannot exceed 99");
        for (int i = 0; i < 10; ++i) click(game, SectionUiPickupMinus, true);
        require(editor.pickupQuantity() == 1 && editor.revision() == unchangedRevision && !editor.dirty(),
                "palette and quantity changes do not alter the authored section");

        std::vector<HubAsset> placed;
        for (int i = 0; i < ItemCount; ++i) {
            openPalette(game); click(game, SectionUiPaletteTabBase + 3);
            click(game, SectionUiHotbarBase + i % 9);
            click(game, SectionUiPaletteItemBase + i);
            if (i) click(game, SectionUiPickupPlus);
            require(editor.hotbar()[i % 9].pickup == static_cast<ItemId>(i),
                    "every item can equip any creative hotbar slot");
            press(game, &InputActions::devPalette);
            floorAt(game, -23.5f + (i % 5) * 8, -12.5f + (i / 5) * 8);
            const auto previousVariation = editor.variation();
            press(game, &InputActions::devVariation);
            press(game, &InputActions::devRotate);
            require(editor.variation() == previousVariation && editor.placementPreview()
                    && editor.placementPreview()->pickupItem == ItemStack{static_cast<ItemId>(i), i + 1}
                    && editor.placementPreview()->variation == 0,
                    "ground preview preserves item quantity and rotation without Y variants");
            const auto preview = *editor.placementPreview();
            const auto count = editor.assets().size();
            press(game, &InputActions::place);
            require(editor.assets().size() == count + 1 && editor.assets().back() == preview,
                    "right click places the exact preview stack");
            const auto [low, high] = pickupVisualBounds(editor.assets().back());
            require(std::abs(editor.assets().back().position.y - 9) < .002f
                    && low.y > editor.assets().back().position.y && high.y > low.y,
                    "dropped items keep floor spawn anchors while their visible models hover above them");
            placed.push_back(editor.assets().back());
        }
        const auto built = editor.assets();
        // Creative flight normally moves only the camera. Attach the authored
        // room's collision here before probing its drops with the walking body.
        game.player.setAssetCollision(editor.assetCollision());
        for (const auto& drop : placed) {
            require(game.player.setFeetPosition(game.world, drop.position),
                    "creative pickup spawns have no solid collision");
            game.camera.setPosition(game.player.eyePosition()); update(game);
        }
        require(editor.assets() == built && game.equipment == originalEquipment,
                "moving through dropped items in creative mode never collects them");
        press(game, &InputActions::undo);
        require(editor.assets().size() == built.size() - 1, "pickup placement has a single undo action");
        press(game, &InputActions::redo);
        require(editor.assets() == built, "redo preserves every pickup payload");

        const auto reject = [&](glm::vec3 camera, glm::vec3 target, const char* message) {
            game.camera.setPosition(camera); aim(game.camera, target); update(game);
            require(!editor.placementPreview(), message);
            press(game, &InputActions::place);
            require(editor.assets() == built, "invalid item placement cannot mutate the room");
        };
        reject({26.5f, 14, -26.5f}, {32.5f, 14, -26.5f}, "pickups cannot attach sideways to walls");
        reject({26.5f, 52, -26.5f}, {26.5f, 57.5f, -27.5f}, "pickups cannot attach to ceilings");
        game.world.set({26, 9, 24}, Block::Water);
        reject({26.5f, 13, 27.5f}, {26.5f, 9.5f, 24.5f}, "pickups cannot float on water");
        game.world.set({26, 9, 24}, Block::Air);
        pointAt(game, boulder);
        require(editor.targetedAsset() && !editor.placementPreview(),
                "an aimed model prevents placing an item underneath its occluded floor");
        press(game, &InputActions::place);
        require(editor.assets() == built, "occluded ground cannot create buried pickup models");

        openPalette(game);
        for (int i = 0; i < 3; ++i) click(game, SectionUiPickupMinus, true);
        press(game, &InputActions::devPalette);
        pointAt(game, placed[13]); press(game, &InputActions::pick);
        require(editor.hotbar()[editor.selectedSlot()].pickup == placed[13].pickupItem.item
                && editor.pickupQuantity() == placed[13].pickupItem.count
                && editor.facing() == placed[13].facing,
                "middle pick restores the exact item, stack count and facing");
        press(game, &InputActions::remove);
        require(editor.assets().size() == built.size() - 1,
                "left click removes the hovered item at its visible floating position");
        press(game, &InputActions::undo);
        require(editor.assets() == built, "undo restores the removed dropped stack exactly");
        pointAt(game, legacy); press(game, &InputActions::pick);
        require(editor.hotbar()[editor.selectedSlot()].pickup == ItemId::Key
                && editor.hotbar()[editor.selectedSlot()].asset == HubAssetKind::ItemPickup
                && editor.pickupQuantity() == 1,
                "legacy keys middle-pick into the Items catalog without exposing an Assets entry");

        // Copy a full floor cell and the ore above it through the public editor tools.
        const auto source = placed[0];
        const glm::ivec3 corner(glm::floor(source.position));
        game.camera.setPosition(glm::vec3(corner) + glm::vec3(.5f)); press(game, &InputActions::devCornerA);
        game.camera.setPosition(glm::vec3(corner + glm::ivec3(0, 2, 0)) + glm::vec3(.5f));
        press(game, &InputActions::devCornerB); press(game, &InputActions::devCopy);
        require(editor.clipboard().assets.size() == 1
                && editor.clipboard().assets[0].pickupItem == source.pickupItem,
                "copy retains an independent item stack payload");
        floorAt(game, 16.5f, -24.5f);
        press(game, &InputActions::devPaste); press(game, &InputActions::devRotate); update(game);
        require(editor.toolPreview() && editor.toolPreviewValid(), "pickup selection can be rotated and pasted");
        press(game, &InputActions::placePressed);
        require(editor.assets().size() == built.size() + 1
                && editor.assets().back().pickupItem == source.pickupItem
                && editor.assets().back().facing == (source.facing + 1) % 4
                && editor.assets()[2] == source,
                "rotated paste keeps payloads and leaves the original unchanged");
        const auto withCopy = editor.assets();
        press(game, &InputActions::undo);
        require(editor.assets() == built, "undo removes the copied pickup alone");
        press(game, &InputActions::redo);
        require(editor.assets() == withCopy, "redo restores the copied pickup payload");
        press(game, &InputActions::devCancel); press(game, &InputActions::save);
        require(loadSection(draft).assets == withCopy, "section save/load retains all authored item stacks");

        press(game, &InputActions::devTest);
        require(editor.screen() == SectionEditorScreen::Test, "authored item room can enter F8 walk testing");
        collect(game, placed[0]); collect(game, placed.back());
        require(game.equipment.inventory.count(ItemId::Coal) == placed[0].pickupItem.count
                && game.equipment.inventory.count(ItemId::Key) == placed.back().pickupItem.count
                && game.sectionSceneAssets().size() == withCopy.size() - 2,
                "walking through drops in F8 collects exact ore/key stacks without aiming or pressing E");
        require(editor.assets() == withCopy, "playtest collection never changes the authored item placements");
        press(game, &InputActions::devTest);
        require(editor.screen() == SectionEditorScreen::Build && game.equipment == originalEquipment
                && game.sectionSceneAssets().size() == withCopy.size()
                && loadSection(draft).assets == withCopy && !std::filesystem::exists(options.progressPath),
                "leaving F8 restores equipment and all authored pickups without a persistent profile write");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result = 1;
    }
    std::filesystem::remove_all(root);
    return result;
}
