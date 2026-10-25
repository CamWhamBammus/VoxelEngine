#include "game.h"
#include "input.h"
#include "sections.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {
void click(voxel::Game& game, int id)
{
    voxel::InputActions action; action.active = true; action.devClick = id; game.update(action);
}
void press(voxel::Game& game, bool voxel::InputActions::* key)
{
    voxel::InputActions action; action.active = true; action.*key = true; game.update(action);
}
void type(voxel::Game& game, const std::string& text)
{
    voxel::InputActions action; action.active = true; action.devText = text; game.update(action);
}
void rename(voxel::Game& game, const std::string& text)
{
    press(game, &voxel::InputActions::devRename);
    type(game, text);
    click(game, voxel::SectionUiRenameApply);
}
std::string bytes(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void aim(voxel::Camera& camera, glm::vec3 point)
{
    const auto direction = glm::normalize(point - camera.position());
    const auto old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}
}

int main()
{
    int failures = 0;
    const auto check = [&](bool result, const char* description) {
        if (!result) { std::cerr << description << '\n'; ++failures; }
    };
    char pattern[] = "/tmp/voxel-section-naming-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern);
    try {
        voxel::Options options;
        options.dev = true; options.sectionLibrary = (root / "sections").string();
        voxel::Game game(options);
        auto& editor = game.sectionEditor();
        press(game, &voxel::InputActions::devRename);
        check(editor.screen() == voxel::SectionEditorScreen::Worlds, "naming needs an open section");
        click(game, voxel::SectionUiWorldBase);
        click(game, voxel::SectionUiLibraryCave);
        const auto draft = voxel::listSectionDrafts(options.sectionLibrary).front().path;
        const auto spawn = editor.meta().spawn;
        const auto finish = editor.meta().finish;
        const auto originalBytes = bytes(draft);

        press(game, &voxel::InputActions::devRename);
        check(editor.screen() == voxel::SectionEditorScreen::Rename && editor.typing()
              && editor.menuOpen() && !editor.flying() && editor.nameSelected()
              && editor.renameText() == "Untitled cave", "F2 opens a preselected name before any playtest");
        type(game, "  Mosswater Grotto  ");
        check(editor.meta().name == "Untitled cave" && !editor.dirty() && bytes(draft) == originalBytes,
              "typing stays in a buffer until Save");
        press(game, &voxel::InputActions::upgrade);
        check(editor.screen() == voxel::SectionEditorScreen::Build && !editor.dirty()
              && !editor.testComplete() && !editor.meta().tested && editor.meta().name == "Mosswater Grotto",
              "Enter names and saves an untested section, trimming edge spaces");
        check(voxel::loadSection(draft).meta.name == "Mosswater Grotto"
              && editor.draftEntries().size() == 1 && editor.draftEntries().front().meta.name == "Mosswater Grotto"
              && voxel::listSectionDrafts(options.sectionLibrary).size() == 1,
              "rename updates the existing draft and library metadata, without making copies");
        check(editor.meta().spawn == spawn && editor.meta().finish == finish
              && editor.meta().lighting == voxel::SectionLighting::Dark
              && editor.meta().category == voxel::SectionCategory::Exploration,
              "naming preserves the cave's markers, category and lighting");
        press(game, &voxel::InputActions::devLibrary);
        click(game, voxel::SectionUiLibraryDraftBase);
        check(editor.meta().name == "Mosswater Grotto", "a reopened draft retains its name");

        click(game, voxel::SectionUiRename);
        type(game, "Discard me");
        press(game, &voxel::InputActions::closeMenu);
        check(editor.meta().name == "Mosswater Grotto" && !editor.dirty()
              && voxel::loadSection(draft).meta.name == "Mosswater Grotto",
              "Escape cancels without changing metadata or the saved draft");
        press(game, &voxel::InputActions::devRename);
        press(game, &voxel::InputActions::devBackspace);
        press(game, &voxel::InputActions::upgrade);
        check(editor.screen() == voxel::SectionEditorScreen::Rename && editor.renameText().empty()
              && editor.meta().name == "Mosswater Grotto", "an empty name is rejected without discarding the dialog");
        type(game, "    ");
        click(game, voxel::SectionUiRenameApply);
        check(editor.screen() == voxel::SectionEditorScreen::Rename && !editor.dirty(), "space-only names are rejected");
        click(game, voxel::SectionUiNameField);
        type(game, std::string(60, 'A'));
        check(editor.renameText() == std::string(48, 'A'), "clicking the name selects all and names are limited to 48 characters");
        press(game, &voxel::InputActions::devBackspace);
        check(editor.renameText().size() == 47, "Backspace removes the last character after typing");
        press(game, &voxel::InputActions::devSelectAll);
        type(game, "Old Roots\n\t");
        press(game, &voxel::InputActions::save);
        check(editor.meta().name == "Old Roots" && !editor.dirty(), "select-all replaces the name, ignores controls and Save persists it");

        press(game, &voxel::InputActions::devPalette);
        click(game, voxel::SectionUiRename);
        type(game, "Palette Name");
        click(game, voxel::SectionUiRenameApply);
        check(editor.screen() == voxel::SectionEditorScreen::Palette && editor.meta().name == "Palette Name",
              "saving from the creative menu returns to the creative menu");
        press(game, &voxel::InputActions::devPalette);
        press(game, &voxel::InputActions::devTools);
        press(game, &voxel::InputActions::devRename);
        type(game, "Cancel tools");
        click(game, voxel::SectionUiBack);
        check(editor.screen() == voxel::SectionEditorScreen::Tools && editor.meta().name == "Palette Name",
              "cancel returns to the original building-tools menu");
        click(game, voxel::SectionUiPasteSettings);
        press(game, &voxel::InputActions::devRename);
        type(game, "Still Caves");
        click(game, voxel::SectionUiRenameApply);
        check(editor.screen() == voxel::SectionEditorScreen::PasteSettings && editor.meta().name == "Still Caves",
              "paste settings supports naming without changing the active build tool");
        click(game, voxel::SectionUiBack);
        press(game, &voxel::InputActions::devTools);

        // Mix a real carved voxel edit and a name edit; neither should consume
        // the other's history or restore an old metadata snapshot accidentally.
        rename(game, "Before Carving");
        game.camera.setPosition({.5f, 27, 23.5f});
        aim(game.camera, {12.5f, 27, 23.5f});
        voxel::InputActions idle; idle.active = true; game.update(idle);
        check(game.target.has_value(), "carving fixture targets a stone wall");
        const auto carved = game.target ? game.target->position : glm::ivec3(12, 27, 23);
        const auto beforeBlock = game.world.get(carved);
        press(game, &voxel::InputActions::remove);
        check(beforeBlock == voxel::Block::Stone && game.world.get(carved) == voxel::Block::Air && editor.dirty(),
              "fixture carves actual stone before renaming");
        rename(game, "After Carving");
        press(game, &voxel::InputActions::undo);
        check(editor.meta().name == "Before Carving" && game.world.get(carved) == voxel::Block::Air,
              "undoing the name leaves the carved tunnel intact");
        press(game, &voxel::InputActions::undo);
        check(editor.meta().name == "Before Carving" && game.world.get(carved) == beforeBlock,
              "terrain undo preserves the current name");
        press(game, &voxel::InputActions::redo);
        press(game, &voxel::InputActions::redo);
        check(editor.meta().name == "After Carving" && game.world.get(carved) == voxel::Block::Air,
              "redo independently restores the carve and name edits");

        press(game, &voxel::InputActions::devTest);
        check(game.player.setFeetPosition(game.world, editor.meta().finish), "test finish remains reachable after carving");
        game.update(idle, .05f);
        check(editor.testComplete() && editor.meta().tested, "fixture reaches the test finish");
        press(game, &voxel::InputActions::devTest);
        const auto elapsed = editor.testElapsed();
        rename(game, "Tested Cavern");
        check(editor.testComplete() && editor.meta().tested && editor.testElapsed() == elapsed
              && voxel::loadSection(draft).meta.tested, "naming preserves the completed test and persists its result");
        press(game, &voxel::InputActions::undo);
        check(editor.meta().name == "After Carving" && editor.testComplete() && editor.meta().tested,
              "undoing a name does not invalidate a completed test");
        press(game, &voxel::InputActions::redo);
        check(editor.meta().name == "Tested Cavern" && editor.testComplete() && editor.meta().tested,
              "redoing a name does not invalidate a completed test");
        press(game, &voxel::InputActions::save);

        // A directory in the draft's place forces an atomic-save failure on
        // every host, without relying on user permissions or touching real saves.
        const auto backup = root / "backup.vxs";
        std::filesystem::rename(draft, backup);
        std::filesystem::create_directory(draft);
        rename(game, "Unsaved Name");
        check(editor.screen() == voxel::SectionEditorScreen::Rename && editor.renameText() == "Unsaved Name"
              && editor.meta().name == "Tested Cavern" && !editor.dirty() && editor.meta().tested,
              "save failure retains the edit buffer but rolls back live metadata and dirty/test state");
        click(game, voxel::SectionUiBack);
        check(editor.meta().name == "Tested Cavern" && editor.screen() == voxel::SectionEditorScreen::Build,
              "a failed rename can be cancelled safely");
        rename(game, "Recovered Name");
        std::filesystem::remove(draft);
        std::filesystem::rename(backup, draft);
        click(game, voxel::SectionUiRenameApply);
        check(editor.screen() == voxel::SectionEditorScreen::Build && editor.meta().name == "Recovered Name"
              && voxel::loadSection(draft).meta.name == "Recovered Name", "retry saves after the disk problem is resolved");
        press(game, &voxel::InputActions::undo);
        check(editor.meta().name == "Tested Cavern", "failed and cancelled attempts add no undo entries");
        press(game, &voxel::InputActions::redo);

        press(game, &voxel::InputActions::devAccept);
        check(editor.screen() == voxel::SectionEditorScreen::Accept, "a renamed tested section remains eligible for publishing");
        click(game, voxel::SectionUiConfirm);
        const auto published = editor.acceptedPath();
        const auto publishedBytes = bytes(published);
        click(game, voxel::SectionUiLibraryEntryBase);
        rename(game, "Revised Cavern");
        check(bytes(published) == publishedBytes && editor.meta().name == "Revised Cavern"
              && !editor.meta().tested && voxel::listSectionDrafts(options.sectionLibrary).size() == 2,
              "renaming an accepted section saves a draft copy without changing its published revision");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::filesystem::remove_all(root);
    if (!failures) std::cout << "Section naming checks passed\n";
    return failures ? 1 : 0;
}
