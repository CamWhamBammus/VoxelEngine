#include "game.h"
#include "input.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;
void click(Game& game, int id)
{
    InputActions action; action.active = true; action.devClick = id; game.update(action);
}
void press(Game& game, bool InputActions::* key)
{
    InputActions action; action.active = true; action.*key = true; game.update(action);
}
void selectTool(Game& game, int slot)
{
    InputActions action; action.active = true; action.selectedTool = slot; game.update(action);
}
void advance(Game& game, int frames = 1)
{
    InputActions action; action.active = true;
    for (int i = 0; i < frames; ++i) game.update(action, .1f);
}
}

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool result, const char* message) {
        if (!result) { std::cerr << message << '\n'; ++failures; }
    };
    char pattern[] = "/tmp/voxel-cave-game-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern), sections = root / "sections", levels = root / "levels";
    try {
        Options options; options.dev = true;
        options.sectionLibrary = sections.string(); options.levelLibrary = levels.string();
        Game game(options);
        auto& editor = game.sectionEditor();
        check(!game.darkCave() && !game.carriedLantern(),
              "world chooser has no active dark section or authoring lamp");
        click(game, SectionUiWorldBase);
        click(game, SectionUiLibraryCave);
        click(game, SectionUiPresetBase + 1);
        click(game, SectionUiCreate);
        check(editor.hasSection() && editor.screen() == SectionEditorScreen::Build
              && editor.meta().lighting == SectionLighting::Dark
              && editor.meta().category == SectionCategory::Exploration
              && game.world.get({0,25,24}) == Block::Air && game.world.get({0,25,16}) == Block::Stone,
              "new cave enters the stone-filled exploration template through the real library action");
        check(editor.builderLight() && !game.darkCave() && game.carriedLantern(),
              "builder assistance makes a new cave visible without changing its saved dark lighting");
        const auto drafts = listSectionDrafts(sections);
        check(drafts.size() == 1 && drafts.front().meta.lighting == SectionLighting::Dark,
              "initial cave draft stores dark lighting despite its temporary editing assistance");

        press(game, &InputActions::devTools);
        game.equipment.selectedSlot = 1;
        click(game, SectionUiBuilderLight);
        check(!editor.builderLight() && game.darkCave() && !game.carriedLantern() && !editor.dirty(),
              "turning builder light off reveals actual darkness even with a lantern selected in player equipment");
        click(game, SectionUiLighting);
        check(editor.meta().lighting == SectionLighting::Ambient && !game.darkCave()
              && !game.carriedLantern() && editor.dirty(),
              "section lighting changes independently of the temporary authoring lamp");
        press(game, &InputActions::undo);
        check(editor.meta().lighting == SectionLighting::Dark && game.darkCave(),
              "undo restores the cave's authored dark lighting");
        press(game, &InputActions::redo);
        check(editor.meta().lighting == SectionLighting::Ambient && !game.darkCave(),
              "redo restores ambient lighting");
        click(game, SectionUiLighting);
        press(game, &InputActions::devTools);
        press(game, &InputActions::devPalette);
        check(editor.screen() == SectionEditorScreen::Palette && game.darkCave() && !game.carriedLantern(),
              "opening the creative palette keeps the underlying cave dark when assistance is off");
        click(game, SectionUiBuilderLight);
        check(!game.darkCave() && game.carriedLantern() && editor.meta().lighting == SectionLighting::Dark,
              "builder light works while a creative menu is open and never rewrites the section lighting");
        game.equipment.selectedSlot = 0;
        press(game, &InputActions::devTest);
        check(editor.screen() == SectionEditorScreen::Test && editor.builderLight()
              && game.darkCave() && !game.carriedLantern(),
              "F8 ignores editing assistance and starts the cave test without an automatic player lamp");
        selectTool(game, 1);
        check(game.equipment.selectedTool() == Tool::Lantern && game.carriedLantern() && game.darkCave(),
              "hotbar key 2 enables the player's lantern without reintroducing ambient cave lighting");
        press(game, &InputActions::inventory);
        check(game.inventoryOpen() && game.darkCave() && game.carriedLantern(),
              "inventory overlays keep the dark section and existing lantern light");
        press(game, &InputActions::inventory);
        check(game.damagePlayer(Health::Maximum) && game.health().dead()
              && game.darkCave() && !game.carriedLantern(),
              "death turns off the carried lantern while preserving the section's dark environment");
        game.restoreHealth();
        press(game, &InputActions::devTest);
        check(editor.screen() == SectionEditorScreen::Build && !game.darkCave() && game.carriedLantern(),
              "returning from F8 restores the author's builder light");
        press(game, &InputActions::devToggle);
        check(!game.devMode() && !game.darkCave(), "leaving the section builder restores hub lighting");
        selectTool(game, 0);
        check(!game.carriedLantern(), "hub pickaxe has no carried lantern light");
        selectTool(game, 1);
        check(game.carriedLantern(), "hub lantern retains its normal light");
        press(game, &InputActions::monsterPreview);
        check(game.monsterPreviewActive() && !game.darkCave() && !game.carriedLantern(),
              "creature preview never inherits an equipped lantern or cave darkness");
        press(game, &InputActions::returnToHub);
        check(!game.monsterPreviewActive() && !game.darkCave() && game.carriedLantern(),
              "returning from creature preview restores the equipped hub lantern");

        World authored;
        buildCaveSection(authored);
        SectionMetadata dark;
        dark.name = "Dark passage"; dark.category = SectionCategory::Exploration;
        dark.spawn = SectionCaveSpawn; dark.finish = SectionCaveFinish;
        dark.lighting = SectionLighting::Dark; dark.tested = true;
        const auto darkPath = acceptSection(authored, dark, {}, sections);
        buildBlankSection(authored);
        SectionMetadata ambient;
        ambient.name = "Garden chamber"; ambient.tested = true;
        ambient.spawn = {.5f,9,4.5f}; ambient.finish = {.5f,9,-.5f};
        const auto ambientPath = acceptSection(authored, ambient, {}, sections);
        LevelDefinition level;
        level.name = "Cave lighting route";
        level.sections = {std::filesystem::relative(darkPath, sections),
                          std::filesystem::relative(ambientPath, sections),
                          std::filesystem::relative(darkPath, sections)};
        saveLevel(level, sections, levels);
        selectTool(game, 0);
        press(game, &InputActions::levelMenu);
        click(game, LevelUiWorldBase);
        click(game, LevelUiEntryBase);
        check(game.levelActive() && game.darkCave() && !game.carriedLantern()
              && game.player.feetPosition() == SectionCaveSpawn,
              "a published level loads its authored dark cave without creative lighting assistance");
        selectTool(game, 1);
        check(game.carriedLantern() && game.darkCave(), "published cave play supports the ordinary lantern slot");
        selectTool(game, 0);
        for (int section = 0; section < 2; ++section) {
            const bool wasDark = section == 0;
            check(game.player.setFeetPosition(game.world, game.levelController().meta().finish),
                  "lighting test can reach the authored safe End marker");
            advance(game);
            check(game.levelController().screen() == LevelScreen::Transition && game.darkCave() == wasDark,
                  "transition overlay retains the finished section's lighting until the next section loads");
            advance(game, 4);
            check(game.levelController().screen() == LevelScreen::Play
                  && game.levelController().sectionIndex() == static_cast<std::size_t>(section + 1)
                  && game.darkCave() != wasDark && !game.carriedLantern(),
                  "section transition applies the next room's saved lighting without leaking builder or lantern light");
        }
        check(game.player.setFeetPosition(game.world, game.levelController().meta().finish),
              "final dark section has a reachable safe End");
        advance(game);
        check(game.levelController().screen() == LevelScreen::Complete && game.darkCave(),
              "completion overlay retains the final cave's dark lighting");
        press(game, &InputActions::returnToHub);
        check(!game.levelActive() && !game.darkCave() && !game.carriedLantern(),
              "returning from a dark expedition restores the hub and respects the equipped tool");

        Options oldEditor; oldEditor.editor = true; oldEditor.seedExplicit = true;
        Game terrainEditor(oldEditor);
        terrainEditor.equipment.selectedSlot = 1;
        check(!terrainEditor.darkCave() && !terrainEditor.carriedLantern(),
              "legacy terrain editor keeps its previous lighting without a carried player lantern");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    std::filesystem::remove_all(root);
    if (!failures) std::cout << "Cave runtime lighting, editor assistance and expedition transitions passed\n";
    return failures ? 1 : 0;
}
