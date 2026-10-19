#include "game.h"
#include "input.h"
#include "sections.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
void aim(voxel::Camera& camera, glm::vec3 point)
{
    const auto direction = glm::normalize(point - camera.position());
    const auto old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}
void click(voxel::Game& game, int id)
{
    voxel::InputActions action; action.active = true; action.devClick = id; game.update(action);
}
void press(voxel::Game& game, bool voxel::InputActions::* key)
{
    voxel::InputActions action; action.active = true; action.*key = true; game.update(action);
}
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}
int main()
{
    int failures = 0;
    const auto check = [&](bool result, const char* description) {
        if (!result) { std::cerr << description << '\n'; ++failures; }
    };
    char pattern[] = "/tmp/voxel-section-editor-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern);
    try {
        const std::array<std::string_view, 1> devArg{"--dev"};
        check(voxel::parseOptions(devArg).dev, "--dev opens the section authoring workflow");
        for (const std::vector<std::string_view>& args : {
            std::vector<std::string_view>{"--dev", "--editor"},
            {"--dev", "--root-crossing", "1"}, {"--dev", "--world", "any.vxl"}}) {
            bool rejected = false;
            try { voxel::parseOptions(args); } catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "conflicting startup modes fail before the window opens");
        }
        voxel::Options options; options.dev = true; options.sectionLibrary = (root / "library").string();
        voxel::Game game(options);
        const auto equipment = game.equipment;
        auto& editor = game.sectionEditor();
        check(game.devMode() && game.devMenuOpen() && !game.devFlying()
              && editor.screen() == voxel::SectionEditorScreen::Worlds,
              "startup asks for the world before creating any section");
        check(!std::filesystem::exists(options.sectionLibrary), "opening the world chooser does not write files");
        click(game, voxel::SectionUiWorldBase);
        check(editor.screen() == voxel::SectionEditorScreen::Library, "world choice opens that world's library");
        click(game, voxel::SectionUiLibraryNew);
        check(game.devFlying() && editor.hasSection() && !editor.dirty(), "new section enters flight and saves its first draft");
        const auto initialDrafts = voxel::listSectionDrafts(options.sectionLibrary);
        check(initialDrafts.size() == 1 && initialDrafts.front().meta.world == voxel::SectionWorld::LushCaves,
              "new section draft is stored under the selected world");
        check(editor.hotbar().size() == 9, "creative hotbar exposes nine slots");
        check(!game.nearBlacksmith() && !game.nearExpeditionGate(), "dev world cannot spend equipment or open expedition picker");
        press(game, &voxel::InputActions::devAccept);
        check(editor.screen() == voxel::SectionEditorScreen::Build && !editor.meta().tested,
              "accepting is blocked until a walk test reaches the exit");

        // Pick an unused floor cell and exercise real raycast-based editing.
        game.camera.setPosition({5.5f, 13.0f, 18.5f});
        aim(game.camera, {5.5f, 8.5f, 14.5f});
        voxel::InputActions action; action.active = true;
        game.update(action);
        check(game.target.has_value(), "builder raycast sees the protected floor");
        const auto floorHit = *game.target;
        const auto newCell = floorHit.position + floorHit.normal;
        press(game, &voxel::InputActions::remove);
        check(game.world.get(floorHit.position) != voxel::Block::Air, "creative removal protects the outer room shell");
        press(game, &voxel::InputActions::place);
        check(game.world.get(newCell) == voxel::Block::Stone && editor.dirty(), "creative placement changes the chosen interior voxel");
        press(game, &voxel::InputActions::undo);
        check(game.world.get(newCell) == voxel::Block::Air, "undo removes the newly placed voxel");
        press(game, &voxel::InputActions::redo);
        check(game.world.get(newCell) == voxel::Block::Stone, "redo restores the placed voxel");
        press(game, &voxel::InputActions::undo);
        press(game, &voxel::InputActions::devPalette);
        check(editor.paletteItems().size() == voxel::PlaceableBlocks.size(), "palette includes every creative material");
        click(game, voxel::SectionUiPaletteTabBase + 1);
        check(editor.paletteItems().size() == voxel::sectionAssetCatalog().size(), "palette includes every existing model asset");
        int fernIndex = 0;
        for (std::size_t i = 0; i < editor.paletteItems().size(); ++i)
            if (editor.paletteItems()[i].asset == voxel::HubAssetKind::Fern) fernIndex = static_cast<int>(i);
        click(game, voxel::SectionUiPaletteItemBase + fernIndex);
        check(editor.hotbar()[0].asset == voxel::HubAssetKind::Fern, "palette assigns a model to the selected hotbar slot");
        press(game, &voxel::InputActions::devPalette);
        press(game, &voxel::InputActions::devRotate);
        press(game, &voxel::InputActions::devVariation);
        game.update(action);
        check(editor.placementPreview().has_value(), "asset placement previews the actual model before placement");
        press(game, &voxel::InputActions::place);
        check(editor.assets().size() == 1 && editor.assets()[0].facing == 1 && editor.assets()[0].variation == 1,
              "model placement retains rotation and variant");
        press(game, &voxel::InputActions::undo);
        check(editor.assets().empty(), "asset placements participate in undo");
        press(game, &voxel::InputActions::redo);
        check(editor.assets().size() == 1, "asset placements participate in redo");
        if (!editor.assets().empty()) {
            game.camera.setPosition(editor.assets()[0].position + glm::vec3(0, 1, 3));
            aim(game.camera, editor.assets()[0].position + glm::vec3(0, .35f, 0));
            game.update(action);
            check(editor.targetedAsset().has_value(), "placed assets are selectable by their mesh bounds");
            press(game, &voxel::InputActions::remove);
            check(editor.assets().empty(), "left click removes a targeted model without deleting its supporting block");
        }
        // Aquatic models use their waterline, not their lowest root, as the
        // anchor. Hanging models must instead meet the real ceiling underside.
        const auto equipAsset = [&](voxel::HubAssetKind kind) {
            press(game, &voxel::InputActions::devPalette);
            click(game, voxel::SectionUiPaletteTabBase + 1);
            int index = -1;
            for (std::size_t i = 0; i < editor.paletteItems().size(); ++i)
                if (editor.paletteItems()[i].asset == kind) index = static_cast<int>(i);
            check(index >= 0, "new lush models can be found in the creative inventory");
            click(game, voxel::SectionUiPaletteItemBase + index);
            press(game, &voxel::InputActions::devPalette);
        };
        game.world.set({12,9,12}, voxel::Block::Water);
        game.camera.setPosition({12.5f,13,14.5f}); aim(game.camera,{12.5f,9.5f,12.5f});
        equipAsset(voxel::HubAssetKind::CaveLotus);
        game.update(action);
        check(editor.placementPreview() && std::abs(editor.placementPreview()->position.y - 10) < .001f,
              "lotus placement aligns its waterline with the water surface");
        press(game, &voxel::InputActions::place);
        check(editor.assets().size() == 1 && editor.assets()[0].kind == voxel::HubAssetKind::CaveLotus,
              "new aquatic model can be placed");
        press(game, &voxel::InputActions::undo);
        game.world.set({12,9,12}, voxel::Block::Air);
        game.camera.setPosition({12.5f,52,14.5f}); aim(game.camera,{12.5f,57.5f,12.5f});
        equipAsset(voxel::HubAssetKind::HangingRoots);
        game.update(action);
        check(editor.placementPreview().has_value(), "hanging roots preview on the ceiling underside");
        if (editor.placementPreview()) {
            float highest = 0;
            for (const auto& vertex : voxel::buildHubAsset(*editor.placementPreview()).vertices)
                highest = std::max(highest, vertex.position.y);
            check(std::abs(highest - 57) < .01f, "hanging root geometry meets the ceiling without floating");
        }
        press(game, &voxel::InputActions::place);
        check(editor.assets().size() == 1 && editor.assets()[0].kind == voxel::HubAssetKind::HangingRoots,
              "hanging roots can be placed and removed through editor history");
        press(game, &voxel::InputActions::undo);

        // Marker placement and undo use the same authored edit history.
        game.camera.setPosition({5.5f, 13, 18.5f}); aim(game.camera, {5.5f, 8.5f, 14.5f});
        press(game, &voxel::InputActions::devPalette);
        click(game, voxel::SectionUiPaletteTabBase + 2);
        click(game, voxel::SectionUiPaletteItemBase + 1);
        press(game, &voxel::InputActions::devPalette);
        press(game, &voxel::InputActions::place);
        check(editor.meta().checkpoint.has_value(), "checkpoint marker places on a supported top face");
        press(game, &voxel::InputActions::undo);
        check(!editor.meta().checkpoint, "marker placements are undoable");

        // Start and End are unique objects: placing again moves the existing
        // endpoint instead of leaving a second trigger in the section.
        const auto originalStart = editor.meta().spawn;
        const auto originalEnd = editor.meta().finish;
        press(game, &voxel::InputActions::devPalette);
        check(editor.paletteItems().size() == 3 && editor.paletteItems()[0].name == "Start"
              && editor.paletteItems()[2].name == "End", "marker inventory exposes one Start and one End tool");
        click(game, voxel::SectionUiPaletteItemBase + 2);
        press(game, &voxel::InputActions::devPalette);
        press(game, &voxel::InputActions::place);
        const auto firstEnd = editor.meta().finish;
        check(firstEnd != originalEnd && editor.meta().spawn == originalStart, "placing End moves only the unique end state");
        game.camera.setPosition({9.5f, 13, 18.5f}); aim(game.camera, {9.5f, 8.5f, 14.5f});
        press(game, &voxel::InputActions::place);
        check(editor.meta().finish != firstEnd && editor.assets().empty(), "a second End placement replaces its location without a duplicate asset");
        press(game, &voxel::InputActions::undo);
        check(editor.meta().finish == firstEnd, "undo restores the previous unique End location");
        press(game, &voxel::InputActions::undo);
        check(editor.meta().finish == originalEnd, "undo retains the original End marker");
        press(game, &voxel::InputActions::devPalette);
        click(game, voxel::SectionUiPaletteItemBase);
        press(game, &voxel::InputActions::devPalette);
        press(game, &voxel::InputActions::place);
        check(editor.meta().spawn != originalStart && editor.meta().finish == originalEnd,
              "placing Start moves only the unique spawn state");
        press(game, &voxel::InputActions::undo);
        press(game, &voxel::InputActions::remove);
        check(editor.meta().spawn == originalStart, "the required Start cannot be removed");

        // A pool beside the walking route verifies that F8 tests use the same
        // swimming controls as ordinary play, without moving required markers.
        for (int z = 18; z <= 24; ++z)
            for (int x = 18; x <= 24; ++x)
                for (int y = 9; y <= 21; ++y)
                    game.world.set({x, y, z}, y <= 18 ? voxel::Block::Water : voxel::Block::Air);
        press(game, &voxel::InputActions::devTest);
        check(editor.screen() == voxel::SectionEditorScreen::Test && !game.devFlying()
              && game.player.grounded() && game.player.feetPosition() == editor.meta().spawn,
              "F8 starts a grounded play test at the authored start marker");
        const auto pausedPosition = game.player.feetPosition();
        voxel::InputActions paused; paused.movement = {0, 1}; game.update(paused, .1f);
        check(game.player.feetPosition() == pausedPosition && editor.testElapsed() == 0,
              "paused test input cannot move the player or advance its clock");
        const glm::vec3 swimStart{21.5f, 13, 21.5f};
        check(game.player.setFeetPosition(game.world, swimStart), "F8 test pool fits the physical player");
        game.camera.setPosition(game.player.eyePosition());
        voxel::InputActions swim;
        swim.active = swim.jump = true;
        for (int frame = 0; frame < 45; ++frame) game.update(swim, 1.0f / 60.0f);
        check(game.player.inWater() && game.player.feetPosition().y > swimStart.y + .5f
              && game.camera.position() == game.player.eyePosition(),
              "F8 section tests allow continuous Space swimming and keep the camera attached");
        const auto pausedSwim = game.player.feetPosition();
        const auto pausedElapsed = editor.testElapsed();
        swim.active = false;
        swim.descend = true;
        game.update(swim, .1f);
        check(game.player.feetPosition() == pausedSwim && editor.testElapsed() == pausedElapsed,
              "pausing an underwater section test freezes swimming and the test clock");
        check(game.player.setFeetPosition(game.world, swimStart), "F8 dive fixture resets in water");
        swim = {};
        swim.active = swim.descend = true;
        for (int frame = 0; frame < 30; ++frame) game.update(swim, 1.0f / 60.0f);
        check(game.player.underwater() && game.player.feetPosition().y < swimStart.y - .5f,
              "F8 section tests forward the descend control to swimming physics");
        press(game, &voxel::InputActions::respawn);
        check(game.player.feetPosition() == editor.meta().spawn && !game.player.inWater() && !game.player.underwater(),
              "resetting an underwater section test returns to the dry authored Start");
        voxel::InputActions settle; settle.active = true;
        game.update(settle, .1f);
        check(game.player.grounded() && game.player.feetPosition() == editor.meta().spawn,
              "section reset clears diving momentum before walking resumes");
        action.movement = {0, 1};
        for (int frame = 0; frame < 1000 && !editor.testComplete(); ++frame) game.update(action, 1.0f / 60.0f);
        check(editor.testComplete() && editor.meta().tested && editor.testElapsed() > 1,
              "ordinary walking to the exit certifies the unmodified authored section");
        press(game, &voxel::InputActions::devAccept);
        check(editor.screen() == voxel::SectionEditorScreen::Accept, "tested section asks for a category and name");
        for (int i = 0; i < 30; ++i) press(game, &voxel::InputActions::devBackspace);
        voxel::InputActions rename; rename.active = true; rename.devText = "Root Walk One"; game.update(rename);
        click(game, voxel::SectionUiCategoryBase + static_cast<int>(voxel::SectionCategory::Exploration));
        click(game, voxel::SectionUiConfirm);
        check(editor.screen() == voxel::SectionEditorScreen::Library && editor.entries().size() == 1,
              "accepting adds a tested section to its world library");
        const auto accepted = editor.acceptedPath();
        const auto acceptedBytes = readFile(accepted);
        const auto acceptedDocument = voxel::loadSection(accepted);
        check(acceptedDocument.meta.name == "Root Walk One"
              && acceptedDocument.meta.category == voxel::SectionCategory::Exploration && acceptedDocument.meta.tested,
              "accepted file stores the selected category, name and completed-test state");
        click(game, voxel::SectionUiLibraryEntryBase);
        check(editor.screen() == voxel::SectionEditorScreen::Build && !editor.meta().tested,
              "reopening an accepted room creates an editable copy requiring another test");
        game.camera.setPosition({5.5f, 13, 18.5f}); aim(game.camera, {5.5f, 8.5f, 14.5f});
        press(game, &voxel::InputActions::devPalette);
        click(game, voxel::SectionUiPaletteTabBase);
        click(game, voxel::SectionUiPaletteItemBase);
        press(game, &voxel::InputActions::devPalette);
        press(game, &voxel::InputActions::place);
        press(game, &voxel::InputActions::save);
        check(readFile(accepted) == acceptedBytes, "editing a published section never overwrites the accepted revision");
        const auto draftCount = voxel::listSectionDrafts(options.sectionLibrary).size();
        press(game, &voxel::InputActions::devLibrary);
        click(game, voxel::SectionUiLibraryNew);
        check(voxel::listSectionDrafts(options.sectionLibrary).size() == draftCount + 1,
              "creating another section preserves all earlier drafts");
        press(game, &voxel::InputActions::returnToHub);
        check(!game.devMode() && !editor.hasSection() && editor.assets().empty() && game.equipment == equipment,
              "returning to the hub saves the draft and preserves equipment without leaving stale authoring state");
        const auto savedDrafts = voxel::listSectionDrafts(options.sectionLibrary);
        const auto lastDraftBytes = readFile(savedDrafts.back().path);
        press(game, &voxel::InputActions::devToggle);
        press(game, &voxel::InputActions::save);
        press(game, &voxel::InputActions::devAccept);
        check(editor.screen() == voxel::SectionEditorScreen::Worlds
              && readFile(savedDrafts.back().path) == lastDraftBytes,
              "reentering the chooser cannot overwrite a draft with the hub terrain");

        const auto blocked = root / "blocked";
        { std::ofstream file(blocked); file << "existing file"; }
        voxel::Options badOptions; badOptions.dev = true; badOptions.sectionLibrary = blocked.string();
        voxel::Game bad(badOptions);
        click(bad, voxel::SectionUiWorldBase); click(bad, voxel::SectionUiLibraryNew);
        press(bad, &voxel::InputActions::returnToHub);
        check(bad.devMode() && bad.sectionEditor().hasSection() && bad.sectionEditor().dirty(),
              "a failed draft save keeps the in-memory section open for recovery");
        check(readFile(blocked) == "existing file", "save failures never replace an unrelated file");
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n'; ++failures;
    }
    std::filesystem::remove_all(root);
    return failures == 0 ? 0 : 1;
}
