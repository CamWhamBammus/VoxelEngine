#include "game.h"
#include "input.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;

void aim(Camera& camera, glm::vec3 point)
{
    const auto direction = glm::normalize(point - camera.position());
    const auto old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}
void update(Game& game)
{
    InputActions actions; actions.active = true; game.update(actions);
}
void click(Game& game, int id)
{
    InputActions actions; actions.active = true; actions.devClick = id; game.update(actions);
}
void press(Game& game, bool InputActions::* key)
{
    InputActions actions; actions.active = true; actions.*key = true; game.update(actions);
}
void closeTools(Game& game)
{
    if (game.sectionEditor().screen() == SectionEditorScreen::Tools) press(game, &InputActions::devTools);
}
void tools(Game& game)
{
    if (game.sectionEditor().screen() != SectionEditorScreen::Tools) press(game, &InputActions::devTools);
}
void select(Game& game, glm::ivec3 first, glm::ivec3 last)
{
    closeTools(game);
    game.camera.setPosition(glm::vec3(first) + glm::vec3(.5f));
    press(game, &InputActions::devCornerA);
    game.camera.setPosition(glm::vec3(last) + glm::vec3(.5f));
    press(game, &InputActions::devCornerB);
    game.camera.setPosition({22.5f, 35, 22.5f});
    update(game);
}
bool boxIs(const std::optional<SectionBox>& box, glm::ivec3 first, glm::ivec3 last)
{
    return box && box->min == first && box->max == last;
}
template<typename Predicate>
bool everyCell(const World& world, const SectionBox& box, Predicate predicate)
{
    for (int z = box.min.z; z <= box.max.z; ++z)
        for (int y = box.min.y; y <= box.max.y; ++y)
            for (int x = box.min.x; x <= box.max.x; ++x)
                if (!predicate(glm::ivec3(x, y, z), world.get({x, y, z}))) return false;
    return true;
}
void equipBlock(Game& game, Block block)
{
    closeTools(game);
    press(game, &InputActions::devPalette);
    click(game, SectionUiPaletteTabBase);
    const auto& items = game.sectionEditor().paletteItems();
    const auto found = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.block == block; });
    if (found == items.end()) throw std::runtime_error("building material missing from palette");
    click(game, SectionUiPaletteItemBase + static_cast<int>(found - items.begin()));
    press(game, &InputActions::devPalette);
}
void equipAsset(Game& game, HubAssetKind kind)
{
    closeTools(game);
    press(game, &InputActions::devPalette);
    click(game, SectionUiPaletteTabBase + 1);
    const auto& items = game.sectionEditor().paletteItems();
    const auto found = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.asset == kind; });
    if (found == items.end()) throw std::runtime_error("model missing from palette");
    click(game, SectionUiPaletteItemBase + static_cast<int>(found - items.begin()));
    press(game, &InputActions::devPalette);
}
void newRoom(Game& game)
{
    press(game, &InputActions::devLibrary);
    click(game, SectionUiLibraryNew);
}
}

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool result, const char* description) {
        if (!result) { std::cerr << description << '\n'; ++failures; }
    };
    char pattern[] = "/tmp/voxel-build-tools-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern);
    try {
        Options options; options.dev = true; options.sectionLibrary = (root / "sections").string();
        Game game(options);
        auto& editor = game.sectionEditor();
        click(game, SectionUiWorldBase);
        click(game, SectionUiLibraryNew);
        const auto originalSpawn = editor.meta().spawn;
        const auto originalFinish = editor.meta().finish;
        const auto originalRevision = editor.revision();
        InputActions scroll; scroll.active = true; scroll.materialStep = 3; game.update(scroll);
        check(editor.selectedSlot() == 3, "creative hotbar consumes all positive scroll ticks");
        scroll.materialStep = -5; game.update(scroll);
        check(editor.selectedSlot() == 7, "creative hotbar wraps all negative scroll ticks");
        scroll.materialStep = 0; scroll.devSlot = 0; game.update(scroll);

        const SectionBox fill{{5,12,5}, {7,14,7}};
        select(game, fill.max, fill.min);
        check(boxIs(editor.buildSelection(), fill.min, fill.max)
              && editor.buildMode() == SectionBuildMode::Select,
              "J and K select arbitrary air corners and normalize their order");
        check(!editor.dirty() && editor.revision() == originalRevision,
              "selection and hotbar navigation do not modify the section");
        tools(game);
        check(editor.screen() == SectionEditorScreen::Tools && game.menuOpen() && !game.devFlying(),
              "G opens the building tools menu and stops flight");
        InputActions slot; slot.active = true; slot.devSlot = 1; game.update(slot);
        check(editor.selectedSlot() == 1 && editor.screen() == SectionEditorScreen::Tools,
              "number keys change the fill material without leaving the tools menu");
        slot.devSlot = 0; game.update(slot);
        click(game, SectionUiFill);
        check(editor.screen() == SectionEditorScreen::Tools
              && everyCell(game.world, fill, [](auto, Block b) { return b == Block::Stone; }),
              "one fill command builds the entire selected volume and leaves tools open");
        press(game, &InputActions::undo);
        check(editor.screen() == SectionEditorScreen::Tools
              && everyCell(game.world, fill, [](auto, Block b) { return b == Block::Air; }),
              "one undo removes the entire filled volume without leaving tools");
        press(game, &InputActions::redo);
        check(editor.screen() == SectionEditorScreen::Tools
              && everyCell(game.world, fill, [](auto, Block b) { return b == Block::Stone; }),
              "one redo restores the entire filled volume without leaving tools");
        closeTools(game);

        const SectionBox hollow{{10,12,5}, {14,16,9}};
        game.world.set({12,14,7}, Block::Clay);
        select(game, hollow.min, hollow.max);
        tools(game); click(game, SectionUiShell);
        check(everyCell(game.world, hollow, [&](glm::ivec3 p, Block b) {
            const bool edge = p.x == hollow.min.x || p.x == hollow.max.x
                || p.y == hollow.min.y || p.y == hollow.max.y || p.z == hollow.min.z || p.z == hollow.max.z;
            return edge ? b == Block::Stone : b == (p == glm::ivec3(12,14,7) ? Block::Clay : Block::Air);
        }), "shell builds walls, floor and ceiling while preserving the existing interior");
        closeTools(game); press(game, &InputActions::undo);
        check(game.world.get(hollow.min) == Block::Air && game.world.get({12,14,7}) == Block::Clay,
              "shell undo preserves preexisting interior decoration");
        tools(game); click(game, SectionUiHeightUp);
        check(boxIs(editor.buildSelection(), hollow.min, hollow.max + glm::ivec3(0,1,0)),
              "selection height can be raised without moving its lower corner");
        click(game, SectionUiHeightDown);
        check(boxIs(editor.buildSelection(), hollow.min, hollow.max), "selection height can be lowered again");
        closeTools(game); press(game, &InputActions::devCancel);
        check(editor.buildMode() == SectionBuildMode::Single && boxIs(editor.buildSelection(), hollow.min, hollow.max),
              "X returns to single placement while retaining the reusable selection");

        // Selection clicks are physical presses: held brush events cannot move
        // corners. A click on the protected floor picks the adjacent interior.
        tools(game); click(game, SectionUiModeBase + 1);
        game.camera.setPosition({18.5f,14,18.5f}); aim(game.camera,{18.5f,8.5f,14.5f}); update(game);
        const auto floor = game.target;
        press(game, &InputActions::removePressed);
        check(floor && editor.firstCorner() == floor->position + floor->normal,
              "selection begins above the room floor without including its protected shell");
        const auto firstCorner = editor.firstCorner();
        game.camera.setPosition({20.5f,14,18.5f}); aim(game.camera,{20.5f,8.5f,14.5f}); update(game);
        press(game, &InputActions::remove);
        check(editor.firstCorner() == firstCorner, "a held-click repeat cannot move a selection corner");
        press(game, &InputActions::placePressed);
        tools(game); click(game, SectionUiErase);
        check(floor && game.world.get(floor->position) == Block::Stone,
              "bulk erase cannot remove the room floor");

        // Build a small asymmetric platform and model through the public tools.
        const SectionBox platform{{6,9,12}, {8,9,13}};
        select(game, platform.min, platform.max); tools(game); click(game, SectionUiFill);
        equipBlock(game, Block::Timber);
        select(game, platform.min, platform.min); tools(game); click(game, SectionUiFill);
        closeTools(game); press(game, &InputActions::devCancel);
        equipAsset(game, HubAssetKind::Fern);
        press(game, &InputActions::devRotate); press(game, &InputActions::devVariation);
        game.camera.setPosition({7.5f,14,16.5f}); aim(game.camera,{7.5f,9.5f,12.5f}); update(game);
        press(game, &InputActions::place);
        check(editor.assets().size() == 1, "source model can be built on the copied platform");
        if (editor.assets().empty()) throw std::runtime_error("source model was not placed");
        const auto sourceAsset = editor.assets().front();
        press(game, &InputActions::devPalette); click(game, SectionUiPaletteTabBase + 2);
        click(game, SectionUiPaletteItemBase + 1); press(game, &InputActions::devPalette);
        // Keep the checkpoint beside the fern: its leaves now have collision,
        // so the former point through the model no longer has player headroom.
        game.camera.setPosition({6.5f,14,16.5f}); aim(game.camera,{6.5f,9.5f,13.5f});
        press(game, &InputActions::place);
        check(editor.meta().checkpoint.has_value(), "source region contains an authored checkpoint to exclude from copying");
        const auto checkpoint = editor.meta().checkpoint;
        const SectionBox source{{6,9,12}, {8,12,13}};
        select(game, source.min, source.max);
        press(game, &InputActions::save);
        const auto beforeCopy = editor.revision();
        press(game, &InputActions::devCopy);
        const auto copied = editor.clipboard();
        check(!copied.empty() && copied.size == glm::ivec3(3,4,2) && copied.assets.size() == 1,
              "copy collects the selected voxels and model origins, excluding marker objects");
        check(!editor.dirty() && editor.revision() == beforeCopy,
              "copying does not dirty a saved section or add a history action");
        tools(game); click(game, SectionUiCopy);
        check(editor.screen() == SectionEditorScreen::Tools && !editor.dirty(),
              "the menu copy command retains the menu and leaves the draft unchanged");
        click(game, SectionUiPaste);
        check(editor.screen() == SectionEditorScreen::Build && editor.buildMode() == SectionBuildMode::Paste,
              "the menu paste command returns directly to the placement preview");

        game.camera.setPosition({-7.5f,15,-5.5f}); aim(game.camera,{-7.5f,8.5f,-10.5f});
        press(game, &InputActions::devPaste); press(game, &InputActions::devRotate); update(game);
        const auto preview = editor.toolPreview();
        check(editor.buildMode() == SectionBuildMode::Paste && preview && editor.toolPreviewValid()
              && preview->max - preview->min + glm::ivec3(1) == glm::ivec3(2,4,3),
              "paste enters a valid preview and T rotates the complete copied volume");
        if (!preview) throw std::runtime_error("paste preview missing");
        const auto destination = preview->min;
        check(game.target && destination == game.target->position + game.target->normal,
              "paste preview begins at the aimed adjacent cell");
        press(game, &InputActions::placePressed);
        check(game.world.get(destination + glm::ivec3(0,0,2)) == Block::Timber
              && game.world.get(destination + glm::ivec3(1,0,0)) == Block::Stone,
              "rotated stamp preserves the asymmetric platform materials");
        check(editor.assets().size() == 2
              && editor.assets().back().position == glm::vec3(destination)
                  + glm::vec3(sourceAsset.position.z - source.min.z,
                              sourceAsset.position.y - source.min.y,
                              3 - (sourceAsset.position.x - source.min.x))
              && editor.assets().back().facing == (sourceAsset.facing + 1) % 4
              && editor.assets().back().variation == sourceAsset.variation,
              "rotated paste moves the model with its platform and preserves its variant");
        check(editor.meta().spawn == originalSpawn && editor.meta().finish == originalFinish
              && editor.meta().checkpoint == checkpoint,
              "copy and paste leave unique Start, End and Checkpoint markers unchanged");
        press(game, &InputActions::undo);
        check(editor.assets().size() == 1
              && everyCell(game.world, *preview, [](auto, Block b) { return b == Block::Air; }),
              "one undo removes both pasted voxels and models");
        press(game, &InputActions::redo);
        check(editor.assets().size() == 2 && game.world.get(destination + glm::ivec3(0,0,2)) == Block::Timber,
              "one redo restores both pasted voxels and models");
        const auto afterStamp = editor.revision();
        game.camera.setPosition({-17.5f,15,-5.5f}); aim(game.camera,{-17.5f,8.5f,-10.5f}); update(game);
        press(game, &InputActions::place);
        check(editor.revision() == afterStamp && editor.assets().size() == 2,
              "held right-button repeats never stamp a second clipboard copy");
        game.camera.setPosition({29.5f,14,-3.5f}); aim(game.camera,{31.8f,8.5f,-6.5f}); update(game);
        check(editor.toolPreview() && !editor.toolPreviewValid(), "paste beyond the room boundary has an invalid preview");
        press(game, &InputActions::placePressed);
        check(editor.revision() == afterStamp && editor.assets().size() == 2,
              "an invalid paste is rejected atomically without changing history or models");
        press(game, &InputActions::devCancel);
        select(game, preview->min, preview->max); tools(game); click(game, SectionUiErase);
        check(editor.assets().size() == 1
              && everyCell(game.world, *preview, [](auto, Block b) { return b == Block::Air; }),
              "bulk erase removes model anchors together with the selected voxels");
        closeTools(game); press(game, &InputActions::undo);
        check(editor.assets().size() == 2 && game.world.get(destination + glm::ivec3(0,0,2)) == Block::Timber,
              "one undo restores erased geometry and models together");
        press(game, &InputActions::save);
        const auto drafts = listSectionDrafts(options.sectionLibrary);
        check(drafts.size() == 1, "bulk editing retains a single draft revision");
        if (!drafts.empty()) {
            const auto saved = loadSection(drafts.front().path);
            check(saved.assets.size() == 2 && saved.world.get(destination + glm::ivec3(0,0,2)) == Block::Timber
                  && saved.meta.checkpoint == checkpoint,
                  "saved section round-trip retains pasted blocks, models and the original markers");
        }

        newRoom(game);
        check(editor.buildMode() == SectionBuildMode::Single && !editor.buildSelection()
              && !editor.firstCorner() && !editor.secondCorner() && !editor.clipboard().empty()
              && editor.clipboard().size == copied.size && editor.clipboard().assets.size() == copied.assets.size(),
              "a new room clears selection and tool mode while keeping the clipboard for reuse");
        press(game, &InputActions::devTest);
        InputActions walking; walking.active = true; walking.movement = {0,1};
        for (int frame = 0; frame < 1000 && !editor.testComplete(); ++frame) game.update(walking, 1.0f / 60);
        check(editor.testComplete() && editor.meta().tested, "blank room can pass its ordinary walking test");
        press(game, &InputActions::devTest);
        equipBlock(game, Block::Stone);
        select(game, {5,12,5}, {7,14,7}); tools(game); click(game, SectionUiFill);
        check(!editor.testComplete() && !editor.meta().tested, "a bulk edit invalidates the previously passed walk test");
        closeTools(game); press(game, &InputActions::undo);
        check(!editor.meta().tested, "undoing a bulk edit still requires a new walk test before acceptance");

        tools(game); click(game, SectionUiBrushSizeBase + 1);
        check(editor.brushSize() == 3 && editor.screen() == SectionEditorScreen::Tools,
              "brush size changes without closing the tools menu");
        click(game, SectionUiModeBase + 2);
        game.camera.setPosition({-15.5f,15,17.5f}); aim(game.camera,{-15.5f,8.5f,12.5f}); update(game);
        const auto brush = editor.toolPreview();
        check(editor.buildMode() == SectionBuildMode::Brush && brush && editor.toolPreviewValid(),
              "brush mode previews its volume above the protected floor");
        if (!brush) throw std::runtime_error("brush preview missing");
        const auto existingClay = brush->min + glm::ivec3(0,1,0);
        game.world.set(existingClay, Block::Clay);
        press(game, &InputActions::place);
        check(everyCell(game.world, *brush, [&](auto p, Block b) { return b == (p == existingClay ? Block::Clay : Block::Stone); }),
              "brush repeats fill empty cells in a single stroke without overwriting existing scenery");
        press(game, &InputActions::undo);
        check(everyCell(game.world, *brush, [&](auto p, Block b) { return b == (p == existingClay ? Block::Clay : Block::Air); }),
              "one brush undo removes its stroke while preserving the existing block");
        equipAsset(game, HubAssetKind::Fern);
        const auto beforeInvalidBrush = editor.revision();
        press(game, &InputActions::place);
        check(editor.revision() == beforeInvalidBrush && editor.assets().empty(),
              "brush mode requires a block and cannot scatter an asset slot by mistake");

        tools(game); click(game, SectionUiModeBase);
        game.world.set({0,9,0}, Block::Stone);
        game.camera.setPosition({1.5f,13,4.5f}); aim(game.camera,{1.5f,8.5f,.5f}); update(game);
        press(game, &InputActions::place);
        check(editor.assets().size() == 1, "brush erasure fixture includes a nearby model anchored to the floor");
        equipBlock(game, Block::Stone);
        tools(game); click(game, SectionUiModeBase + 2);
        game.camera.setPosition({.5f,10,5.5f}); aim(game.camera,{.5f,9.5f,.5f}); update(game);
        press(game, &InputActions::remove);
        check(editor.assets().empty() && game.world.get({0,9,0}) == Block::Air
              && game.world.get({0,8,0}) == Block::Stone,
              "erase brush clears blocks and model anchors while protecting the floor");
        check(editor.brushErasing(), "left brush strokes switch the outline and label to the carve volume");
        press(game, &InputActions::undo);
        check(editor.assets().size() == 1 && game.world.get({0,9,0}) == Block::Stone,
              "erase brush restores removed blocks and models in one undo");
        press(game, &InputActions::place);
        check(!editor.brushErasing(), "right brush strokes restore the add-volume preview");

        // A cave is a separate starting layout. It uses the same saved sections,
        // markers, undo history, palette and test workflow as an open room.
        press(game, &InputActions::devLibrary);
        click(game, SectionUiLibraryCave);
        check(editor.screen() == SectionEditorScreen::Build && editor.buildMode() == SectionBuildMode::CarveRound
              && editor.meta().category == SectionCategory::Exploration && editor.meta().lighting == SectionLighting::Dark,
              "new cave starts an Exploration section with dark lighting and the round carving tool");
        check(editor.meta().spawn == SectionCaveSpawn && editor.meta().finish == SectionCaveFinish
              && game.camera.position() == SectionCaveSpawn + glm::vec3(0, Player::EyeHeight, 0)
              && game.world.get(glm::ivec3(glm::floor(game.camera.position()))) == Block::Air,
              "new cave camera starts inside the hollow chamber instead of inside its stone ceiling");
        check(game.world.get({12,27,12}) == Block::Stone && editor.assets().empty() && editor.builderLight()
              && !editor.dirty(), "new cave is filled with stone and has an editing-only light and saved draft");
        press(game, &InputActions::devTest);
        for (int frame = 0; frame < 160 && !editor.testComplete(); ++frame) game.update(walking, 1.0f / 60);
        check(editor.testComplete() && editor.meta().tested, "cave starter markers have enough space and support to walk-test");
        press(game, &InputActions::devTest);
        press(game, &InputActions::save);
        tools(game);
        const auto testedRevision = editor.revision();
        click(game, SectionUiBuilderLight);
        check(!editor.builderLight() && editor.meta().tested && !editor.dirty() && editor.revision() == testedRevision,
              "turning off the builder light neither changes the saved scene nor invalidates a passed test");
        click(game, SectionUiBuilderLight);
        click(game, SectionUiLighting);
        check(editor.meta().lighting == SectionLighting::Ambient && editor.dirty() && !editor.meta().tested,
              "changing authored lighting is a real edit and invalidates the previous walk test");
        press(game, &InputActions::undo);
        check(editor.meta().lighting == SectionLighting::Dark && !editor.meta().tested,
              "undo restores the dark lighting and still requires a fresh walk test");
        press(game, &InputActions::redo);
        check(editor.meta().lighting == SectionLighting::Ambient, "redo restores the authored lighting change");
        press(game, &InputActions::undo);
        closeTools(game);

        // Tools live in their own palette tab and never overwrite a hotbar slot.
        const auto storedSlot = editor.hotbar()[editor.selectedSlot()];
        press(game, &InputActions::devPalette);
        click(game, SectionUiPaletteTabBase + 4);
        check(editor.paletteItems().size() >= 4 && editor.paletteItems()[0].tool == SectionBuildMode::CarveRound
              && editor.paletteItems()[1].tool == SectionBuildMode::CarveBox,
              "creative Tools tab exposes both carving shapes alongside the existing build tools");
        click(game, SectionUiPaletteItemBase);
        check(editor.screen() == SectionEditorScreen::Build && editor.buildMode() == SectionBuildMode::CarveRound
              && editor.hotbar()[editor.selectedSlot()].name == storedSlot.name
              && !editor.hotbar()[editor.selectedSlot()].tool,
              "choosing a palette tool returns to building without replacing the selected inventory item");
        tools(game); click(game, SectionUiBrushSizeBase + 1); closeTools(game);
        check(editor.brushSize() == 3 && editor.buildMode() == SectionBuildMode::CarveRound,
              "changing carve size keeps the selected round tool");
        game.camera.setPosition({.5f,27.5f,24.5f}); aim(game.camera,{.5f,27.5f,16.5f}); update(game);
        check(game.target && game.target->position == glm::ivec3(0,27,16)
              && boxIs(editor.toolPreview(), {-1,26,15}, {1,28,17}) && editor.toolPreviewValid(),
              "carve preview is centered on the solid hit, extending into the cave wall");
        const auto beforeCarve = editor.revision();
        press(game, &InputActions::place);
        check(editor.revision() == beforeCarve && editor.assets().empty(),
              "right click in carve mode cannot place the selected hotbar model by accident");
        press(game, &InputActions::remove);
        check(game.world.get({0,27,16}) == Block::Air && game.world.get({0,27,15}) == Block::Air
              && game.world.get({1,28,15}) == Block::Stone && editor.revision() == beforeCarve + 1,
              "round carve opens the rock inward while retaining corners outside its sphere");
        press(game, &InputActions::undo);
        check(game.world.get({0,27,16}) == Block::Stone && game.world.get({0,27,15}) == Block::Stone,
              "one undo restores a complete carving stroke");
        press(game, &InputActions::redo);
        check(game.world.get({0,27,16}) == Block::Air && game.world.get({0,27,15}) == Block::Air,
              "one redo reapplies the carving stroke");
        press(game, &InputActions::undo);
        tools(game); click(game, SectionUiCarveBox);
        press(game, &InputActions::remove);
        check(editor.buildMode() == SectionBuildMode::CarveBox && game.world.get({1,28,15}) == Block::Air,
              "box carving removes corners for squared rooms and corridors");
        press(game, &InputActions::undo);
        tools(game); click(game, SectionUiBrushSizeBase + 3); closeTools(game);
        game.camera.setPosition({29.5f,35.5f,0.5f}); aim(game.camera,{35.5f,35.5f,0.5f}); update(game);
        check(editor.buildMode() == SectionBuildMode::CarveBox && editor.brushSize() == 9
              && editor.toolPreview() && editor.toolPreview()->max.x == SectionInteriorMax.x - 1,
              "large carving brushes clip their preview to the protected outer shell");
        press(game, &InputActions::remove);
        check(game.world.get({31,35,0}) == Block::Air && game.world.get({32,35,0}) == Block::Stone,
              "carving near a boundary clears only editable stone and retains the sealed shell");
        press(game, &InputActions::devCancel);
        check(editor.buildMode() == SectionBuildMode::Single && !editor.toolPreview(),
              "X removes the carving outline and returns to normal single placement");
        press(game, &InputActions::save);
        const auto caveDrafts = listSectionDrafts(options.sectionLibrary);
        const auto caveDraft = std::find_if(caveDrafts.begin(), caveDrafts.end(), [](const auto& entry) {
            return entry.meta.name == "Untitled cave";
        });
        check(caveDraft != caveDrafts.end(), "cave layout has its own persistent draft");
        if (caveDraft != caveDrafts.end()) {
            const auto savedCave = loadSection(caveDraft->path);
            check(savedCave.meta.lighting == SectionLighting::Dark && savedCave.meta.category == SectionCategory::Exploration
                  && savedCave.world.get({31,35,0}) == Block::Air,
                  "saving a cave preserves its lighting, category and excavation");
            press(game, &InputActions::devLibrary);
            const auto found = std::find_if(editor.draftEntries().begin(), editor.draftEntries().end(),
                [&](const auto& entry) { return entry.path == caveDraft->path; });
            if (found == editor.draftEntries().end()) throw std::runtime_error("cave draft not in library");
            click(game, SectionUiLibraryDraftBase + static_cast<int>(found - editor.draftEntries().begin()));
            check(editor.meta().lighting == SectionLighting::Dark && editor.builderLight()
                  && game.world.get(glm::ivec3(glm::floor(game.camera.position()))) == Block::Air,
                  "reopening the saved cave restores dark lighting and a camera inside the starting room");
        }
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n'; ++failures;
    }
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
