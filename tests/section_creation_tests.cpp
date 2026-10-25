#include "game.h"
#include "input.h"

#include <filesystem>
#include <iostream>
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
}

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool result, const char* description) {
        if (!result) { std::cerr << description << '\n'; ++failures; }
    };
    char pattern[] = "/tmp/voxel-section-creation-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern);
    try {
        for (const auto& world : sectionWorldCatalog()) for (const bool cave : {false, true}) {
            Options options; options.dev = true;
            options.sectionLibrary = (root / (std::string(world.folder) + (cave ? "-cave" : "-parkour"))).string();
            Game game(options);
            auto& editor = game.sectionEditor();
            click(game, SectionUiWorldBase + static_cast<int>(world.id));
            click(game, cave ? SectionUiLibraryCave : SectionUiLibraryNew);
            check(editor.screen() == SectionEditorScreen::Create && editor.creationCave() == cave
                  && editor.creationPreset() == SectionPreset::WorldTheme && !editor.hasSection(),
                  "new sections ask for layout and default to their world's theme before generating");
            check(listSectionDrafts(options.sectionLibrary).empty(), "choosing a preset does not create a draft");
            click(game, SectionUiPresetBase + 1);
            click(game, SectionUiLayoutBase + !cave);
            press(game, &InputActions::closeMenu);
            check(editor.screen() == SectionEditorScreen::Library && !editor.hasSection()
                  && listSectionDrafts(options.sectionLibrary).empty(), "cancel leaves the library untouched");
            click(game, cave ? SectionUiLibraryCave : SectionUiLibraryNew);
            check(editor.creationCave() == cave && editor.creationPreset() == SectionPreset::WorldTheme,
                  "reopening starts with the requested layout and world theme");
            click(game, SectionUiCreate);
            check(editor.screen() == SectionEditorScreen::Build && editor.hasSection() && !editor.dirty()
                  && editor.meta().world == world.id, "create enters the builder and saves to the selected world");
            check(editor.meta().category == (cave ? SectionCategory::Exploration : SectionCategory::Parkour)
                  && editor.meta().lighting == (cave ? SectionLighting::Dark : SectionLighting::Ambient),
                  "presets retain the chosen layout category and lighting");
            check(sectionMarkerSafe(game.world, editor.meta().spawn, editor.assetCollision().get())
                  && sectionMarkerSafe(game.world, editor.meta().finish, editor.assetCollision().get()),
                  "generated start and end remain playable with themed scenery collision");
            const auto drafts = listSectionDrafts(options.sectionLibrary);
            check(drafts.size() == 1, "confirming creates exactly one draft");
            const auto saved = loadSection(drafts.at(0).path);
            check(saved.assets == editor.assets() && saved.meta.world == world.id,
                  "the actual themed scenery is saved, not just the chooser's label");
            for (const glm::ivec3 p : {glm::ivec3(-33, 27, 0), {0, 8, 0}, {0, 25, 16}, {12, 37, -14}})
                check(saved.world.get(p) == game.world.get(p), "draft snapshot preserves themed shell and deep material");
            press(game, &InputActions::devTest);
            check(editor.screen() == SectionEditorScreen::Test && game.darkCave() == cave,
                  "both layouts can enter a real test and dark caves ignore builder assistance");
            press(game, &InputActions::devTest);
            press(game, &InputActions::devLibrary);
            click(game, SectionUiLibraryNew);
            click(game, SectionUiBack);
            check(editor.hasSection() && listSectionDrafts(options.sectionLibrary).size() == 1,
                  "canceling a new section preserves the existing draft and does not create another");
            click(game, SectionUiLibraryDraftBase);
            check(editor.assets() == saved.assets && editor.meta().lighting == saved.meta.lighting,
                  "reopening loads the snapshot without regenerating a preset");
        }

        Options options; options.dev = true; options.sectionLibrary = (root / "keyboard").string();
        Game keyboard(options);
        click(keyboard, SectionUiWorldBase);
        press(keyboard, &InputActions::upgrade); // Library's new section entry.
        InputActions arrows; arrows.active = true; arrows.devMenuStep = {1, 1}; keyboard.update(arrows);
        check(keyboard.sectionEditor().creationCave()
              && keyboard.sectionEditor().creationPreset() == SectionPreset::PlainStone,
              "keyboard arrows choose a cave layout and plain materials");
        press(keyboard, &InputActions::upgrade);
        check(keyboard.sectionEditor().screen() == SectionEditorScreen::Build
              && keyboard.world.get({0,25,16}) == Block::Stone && keyboard.sectionEditor().assets().empty(),
              "Enter creates the unadorned stone cave for custom builds");
        check(keyboard.sectionEditor().messageVisible(), "creation feedback is initially visible");
        InputActions idle; idle.active = true;
        for (int frame = 0; frame < 310; ++frame) keyboard.update(idle, 1.f / 60.f);
        check(!keyboard.sectionEditor().messageVisible(), "idle feedback fades instead of occupying the builder HUD forever");
        press(keyboard, &InputActions::save);
        check(keyboard.sectionEditor().messageVisible(), "new save feedback becomes visible immediately");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
