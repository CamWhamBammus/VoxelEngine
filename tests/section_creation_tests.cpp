#include "game.h"
#include "input.h"
#include "spawner_chains.h"

#include <algorithm>
#include <cmath>
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
void aim(Camera& camera, glm::vec3 point)
{
    const auto direction = glm::normalize(point - camera.position()), old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(old.y)));
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
        constexpr std::array layouts{SectionLayout::Parkour, SectionLayout::Cave, SectionLayout::Arena};
        constexpr std::array entries{SectionUiLibraryNew, SectionUiLibraryCave, SectionUiLibraryArena};
        for (const auto& world : sectionWorldCatalog()) for (const auto layout : layouts) {
            const auto index = static_cast<int>(layout);
            const bool cave = layout == SectionLayout::Cave, arena = layout == SectionLayout::Arena;
            Options options; options.dev = true;
            options.sectionLibrary = (root / (std::string(world.folder) + "-" + std::to_string(index))).string();
            Game game(options);
            auto& editor = game.sectionEditor();
            click(game, SectionUiWorldBase + static_cast<int>(world.id));
            click(game, entries[index]);
            check(editor.screen() == SectionEditorScreen::Create && editor.creationLayout() == layout
                  && editor.creationPreset() == SectionPreset::WorldTheme && !editor.hasSection(),
                  "new sections ask for layout and default to their world's theme before generating");
            check(listSectionDrafts(options.sectionLibrary).empty(), "choosing a preset does not create a draft");
            click(game, SectionUiPresetBase + 1);
            click(game, SectionUiLayoutBase + (index + 1) % 3);
            press(game, &InputActions::closeMenu);
            check(editor.screen() == SectionEditorScreen::Library && !editor.hasSection()
                  && listSectionDrafts(options.sectionLibrary).empty(), "cancel leaves the library untouched");
            click(game, entries[index]);
            check(editor.creationLayout() == layout && editor.creationPreset() == SectionPreset::WorldTheme,
                  "reopening starts with the requested layout and world theme");
            click(game, SectionUiCreate);
            check(editor.screen() == SectionEditorScreen::Build && editor.hasSection() && !editor.dirty()
                  && editor.meta().world == world.id, "create enters the builder and saves to the selected world");
            check(editor.meta().category == (cave ? SectionCategory::Exploration : arena ? SectionCategory::Arena : SectionCategory::Parkour)
                  && editor.meta().lighting == (cave ? SectionLighting::Dark : SectionLighting::Ambient),
                  "presets retain the chosen layout category and lighting");
            check(sectionMarkerSafe(game.world, editor.meta().spawn, editor.assetCollision().get())
                  && sectionMarkerSafe(game.world, editor.meta().finish, editor.assetCollision().get()),
                  "generated start and end remain playable with themed scenery collision");
            const auto drafts = listSectionDrafts(options.sectionLibrary);
            check(drafts.size() == 1, "confirming creates exactly one draft");
            auto saved = loadSection(drafts.at(0).path);
            check(saved.assets == editor.assets() && saved.meta.world == world.id,
                  "the actual themed scenery is saved, not just the chooser's label");
            for (const glm::ivec3 p : {glm::ivec3(-33, 27, 0), {0, 8, 0}, {0, 25, 16}, {12, 37, -14}})
                check(saved.world.get(p) == game.world.get(p), "draft snapshot preserves themed shell and deep material");
            if (arena) {
                check(editor.buildMode() == SectionBuildMode::Single && editor.builderLight(),
                      "arena starts with normal building and a temporary builder light");
                const auto spawner = std::find_if(editor.assets().begin(), editor.assets().end(),
                    [](const HubAsset& asset) { return isSpawnerAsset(asset.kind); });
                check(spawner != editor.assets().end() && validateSpawnerChains(editor.assets()).empty(),
                      "arena arrives with valid linked fights to configure");
                if (spawner != editor.assets().end()) {
                    const auto spawnerIndex = static_cast<std::size_t>(spawner - editor.assets().begin());
                    const auto authored = *spawner;
                    game.camera.setPosition(authored.position + glm::vec3(0, 2, 3));
                    aim(game.camera, authored.position + glm::vec3(0, .5f, 0));
                    press(game, &InputActions::devPalette);
                    check(editor.screen() == SectionEditorScreen::SpawnerSettings,
                          "an arena's generated spawner opens the ordinary settings editor");
                    click(game, SectionUiSpawnerPlusBase);
                    click(game, SectionUiSpawnerApply);
                    check(editor.assets().at(spawnerIndex).spawner.counts[0] == authored.spawner.counts[0] + 1
                          && editor.assets().at(spawnerIndex).spawner.next == authored.spawner.next,
                          "editing generated enemy counts preserves its next-wave link");
                    press(game, &InputActions::save);
                    saved = loadSection(drafts.at(0).path);
                    check(saved.assets == editor.assets() && saved.meta.category == SectionCategory::Arena,
                          "arena edits save as ordinary authored scenery, waves and category");
                }
            }
            press(game, &InputActions::devTest);
            check(editor.screen() == SectionEditorScreen::Test && game.darkCave() == cave,
                  "every layout can enter a real test and dark sections ignore builder assistance");
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
        arrows.devMenuStep = {1, 0}; keyboard.update(arrows);
        check(keyboard.sectionEditor().creationLayout() == SectionLayout::Arena
              && keyboard.sectionEditor().creationPreset() == SectionPreset::PlainStone,
              "right arrow advances from cave to arena without changing the material preset");
        keyboard.update(arrows);
        check(keyboard.sectionEditor().creationLayout() == SectionLayout::Parkour,
              "right arrow wraps arena back to parkour");
        arrows.devMenuStep = {-1, 0}; keyboard.update(arrows);
        check(keyboard.sectionEditor().creationLayout() == SectionLayout::Arena,
              "left arrow wraps parkour back to arena");
        keyboard.update(arrows);
        check(keyboard.sectionEditor().creationLayout() == SectionLayout::Cave,
              "left arrow returns to cave");
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

        press(keyboard, &InputActions::devLibrary);
        click(keyboard, SectionUiLibraryArena);
        click(keyboard, SectionUiPresetBase + 1);
        click(keyboard, SectionUiCreate);
        check(keyboard.sectionEditor().meta().category == SectionCategory::Arena
              && keyboard.sectionEditor().meta().lighting == SectionLighting::Ambient
              && keyboard.sectionEditor().buildMode() == SectionBuildMode::Single
              && std::count_if(keyboard.sectionEditor().assets().begin(), keyboard.sectionEditor().assets().end(),
                  [](const HubAsset& asset) { return isSpawnerAsset(asset.kind); }) > 1,
              "plain-stone arena still contains editable cover and multiple linked spawners");
        press(keyboard, &InputActions::devTools);
        click(keyboard, SectionUiLighting);
        press(keyboard, &InputActions::closeMenu);
        press(keyboard, &InputActions::devTest);
        check(keyboard.sectionEditor().screen() == SectionEditorScreen::Test && keyboard.darkCave(),
              "authors can still choose dark lighting for an arena through build options");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
