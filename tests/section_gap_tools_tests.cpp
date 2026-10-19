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
void nudge(Game& game, glm::ivec3 movement)
{
    InputActions actions; actions.active = true; actions.devPasteNudge = movement; game.update(actions);
}
void copy(Game& game, SectionBox box)
{
    game.camera.setPosition(glm::vec3(box.min) + glm::vec3(.5f));
    press(game, &InputActions::devCornerA);
    game.camera.setPosition(glm::vec3(box.max) + glm::vec3(.5f));
    press(game, &InputActions::devCornerB);
    game.camera.setPosition({20.5f, 30, 20.5f});
    press(game, &InputActions::devCopy);
}
void settings(Game& game)
{
    press(game, &InputActions::devTools);
    click(game, SectionUiPasteSettings);
}
glm::ivec3 origin(const SectionEditor& editor)
{
    if (!editor.toolPreview()) throw std::runtime_error("expected a paste preview");
    return editor.toolPreview()->min;
}
} // namespace

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool result, const char* description) {
        if (!result) { std::cerr << description << '\n'; ++failures; }
    };
    char pattern[] = "/tmp/voxel-gap-tools-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern);
    try {
        const SectionBox source{{-24, 15, -24}, {-22, 16, -23}};
        World fixture;
        buildBlankSection(fixture);
        for (int z = source.min.z; z <= source.max.z; ++z)
            for (int x = source.min.x; x <= source.max.x; ++x) fixture.set({x, 15, z}, Block::Path);
        fixture.set(source.min, Block::Timber);
        for (int z = 4; z <= 9; ++z)
            for (int x = 4; x <= 9; ++x) fixture.set({x, 15, z}, Block::MossStone);
        fixture.set({6, 15, 6}, Block::Air);
        fixture.set({7, 15, 6}, Block::Air);
        fixture.set({7, 15, 7}, Block::Water);
        fixture.set({8, 15, 6}, Block::CopperOre);
        const std::vector<HubAsset> models{
            {HubAssetKind::Fern, {-23.5f, 16, -23.5f}, 1, 17},
            {HubAssetKind::Flowers, {7.5f, 16, 7.5f}, 0, 23}
        };
        Options options; options.dev = true; options.sectionLibrary = (root / "sections").string();
        saveSectionDraft(fixture, {}, models, options.sectionLibrary);
        Game game(options);
        auto& editor = game.sectionEditor();
        click(game, SectionUiWorldBase);
        click(game, SectionUiLibraryDraftBase);
        check(editor.screen() == SectionEditorScreen::Build, "fixture opens as an editable section draft");
        const auto markers = editor.meta();
        copy(game, source);
        check(editor.clipboard().size == glm::ivec3(3, 2, 2) && editor.clipboard().assets.size() == 1,
              "floor clipboard includes its shape, empty headroom, and source decoration");
        settings(game);
        check(editor.screen() == SectionEditorScreen::PasteSettings && game.menuOpen() && !game.devFlying(),
              "paste settings open from building tools as a paused authoring menu");
        click(game, SectionUiPasteModeBase + 1);
        check(editor.screen() == SectionEditorScreen::Build && editor.pasteFillGaps()
              && !editor.pasteFloorHeight() && !editor.toolPreview(),
              "Fill Gaps first asks for a floor level without inventing a floating preview");
        const auto beforeGap = editor.revision();
        press(game, &InputActions::placePressed);
        check(editor.revision() == beforeGap, "right click cannot fill anything before selecting a floor level");

        game.camera.setPosition({5.5f, 22, 14.5f}); aim(game.camera, {5.5f, 16, 6.5f}); update(game);
        check(game.target && game.target->position.y == 15 && game.target->normal.y == 1,
              "the floor-height fixture aims at a real top face");
        press(game, &InputActions::removePressed);
        check(editor.pasteFloorHeight() == 15 && game.world.get({5, 15, 6}) == Block::MossStone,
              "left click chooses the floor's existing block layer without removing its material");
        game.camera.setPosition({6.5f, 22, 14.5f}); aim(game.camera, {6.5f, 16, 6.5f}); update(game);
        check(origin(editor) == glm::ivec3(6, 15, 6) && editor.toolPreviewValid(),
              "a larger floor stamp stays flush with surrounding blocks over a smaller gap");
        check(editor.previewBlockChanges().size() == 2
              && std::all_of(editor.previewBlockChanges().begin(), editor.previewBlockChanges().end(), [](const auto& block) {
                  return block.position.y == 15 && block.before == Block::Air && block.after != Block::Air;
              }), "preview highlights only the two missing blocks rather than the complete copied floor");
        press(game, &InputActions::placePressed);
        check(game.world.get({6, 15, 6}) == Block::Timber && game.world.get({7, 15, 6}) == Block::Path
              && game.world.get({8, 15, 6}) == Block::CopperOre && game.world.get({7, 15, 7}) == Block::Water
              && game.world.get({6, 15, 7}) == Block::MossStone,
              "Fill Gaps fills only air and preserves solid materials, ore, and water inside the stamp");
        check(editor.assets() == models && editor.meta().spawn == markers.spawn && editor.meta().finish == markers.finish,
              "gap filling retains destination models and markers without duplicating copied decoration");
        check(editor.previewBlockChanges().empty(), "the preview refreshes after stamping and no longer highlights filled cells");
        const auto afterStamp = editor.revision();
        press(game, &InputActions::placePressed);
        check(editor.revision() == afterStamp, "stamping an already filled gap creates no extra history operation");
        press(game, &InputActions::undo);
        check(game.world.get({6, 15, 6}) == Block::Air && game.world.get({7, 15, 6}) == Block::Air
              && game.world.get({8, 15, 6}) == Block::CopperOre && editor.assets() == models
              && editor.previewBlockChanges().size() == 2,
              "one undo restores both holes, retains original scenery, and rebuilds the missing-block preview");
        press(game, &InputActions::redo);
        check(game.world.get({6, 15, 6}) == Block::Timber && editor.previewBlockChanges().empty(),
              "one redo restores the complete gap stamp");

        const auto locked = origin(editor);
        press(game, &InputActions::devPastePin);
        check(editor.pastePinned(), "P pins the gap stamp at its current world position");
        game.camera.setPosition({12.5f, 25, 12.5f}); aim(game.camera, {20, 18, 12.5f}); update(game);
        check(origin(editor) == locked, "moving and looking around leaves a pinned stamp fixed");
        press(game, &InputActions::devRotate);
        check(origin(editor) == locked && editor.toolPreview()->max - locked + glm::ivec3(1) == glm::ivec3(2, 2, 3),
              "rotating a pinned rectangular stamp preserves its origin");
        nudge(game, {0, 1, 0});
        check(origin(editor) == locked + glm::ivec3(0, 1, 0) && editor.pasteFloorHeight() == 16,
              "Page Up moves a pinned stamp exactly one block and updates the locked floor height");
        nudge(game, {0, -1, 0});
        check(origin(editor) == locked && editor.pasteFloorHeight() == 15, "Page Down reverses the height nudge");
        nudge(game, {0, 0, -1});
        check(origin(editor) == locked + glm::ivec3(1, 0, 0), "Up moves the stamp forward relative to an east-facing camera");
        nudge(game, {1, 0, 0});
        check(origin(editor) == locked + glm::ivec3(1, 0, 1), "Right moves sideways relative to the camera instead of a fixed world axis");
        settings(game); click(game, SectionUiPasteResume);
        check(editor.screen() == SectionEditorScreen::Build && editor.pastePinned()
              && origin(editor) == locked + glm::ivec3(1, 0, 1),
              "opening paste settings and resuming keeps the pinned position");
        game.camera.setPosition({5.5f, 22, 14.5f}); aim(game.camera, {5.5f, 16, 6.5f});
        press(game, &InputActions::devPasteLevel);
        check(!editor.pastePinned() && editor.pasteFloorHeight() == 15,
              "L chooses a fresh floor level and releases the old pin");
        nudge(game, {1, 0, 0});
        check(editor.pastePinned(), "the first manual adjustment automatically pins an unpinned preview");
        press(game, &InputActions::devCancel);
        check(editor.buildMode() == SectionBuildMode::Single && !editor.pastePinned() && !editor.pasteFloorHeight()
              && !editor.toolPreview(), "X clears the floating stamp and its position locks");

        // Standard copying also gains precise adjustment, without changing its
        // surface-snapping default or its ability to include copied decoration.
        settings(game); click(game, SectionUiPasteModeBase);
        game.camera.setPosition({-7.5f, 15, -5.5f}); aim(game.camera, {-7.5f, 8.5f, -10.5f}); update(game);
        check(!editor.pasteFillGaps() && !editor.pastePinned() && editor.toolPreviewValid()
              && game.target && origin(editor) == game.target->position + game.target->normal,
              "standard paste retains adjacent-surface placement and starts without a pin");
        const auto standard = origin(editor);
        nudge(game, {0, 1, 0});
        check(editor.pastePinned() && origin(editor) == standard + glm::ivec3(0, 1, 0),
              "standard copies can be moved up one cell with the same adjustment controls");
        game.camera.setPosition({-15.5f, 20, -5.5f}); aim(game.camera, {-15.5f, 8.5f, -10.5f}); update(game);
        check(origin(editor) == standard + glm::ivec3(0, 1, 0), "standard pasted models remain pinned when the camera moves");
        press(game, &InputActions::devPastePin);
        check(!editor.pastePinned() && origin(editor) != standard + glm::ivec3(0, 1, 0),
              "P again returns a standard copy to ordinary cursor placement");

        press(game, &InputActions::devCancel);
        settings(game); click(game, SectionUiPasteModeBase + 1);
        game.camera.setPosition({18.5f, 14, 18.5f}); aim(game.camera, {18.5f, 8.5f, 14.5f});
        press(game, &InputActions::devPasteLevel);
        check(editor.pasteFloorHeight() == SectionInteriorMin.y,
              "the protected room floor chooses the first editable layer rather than the shell");
        press(game, &InputActions::devLibrary); click(game, SectionUiLibraryNew);
        check(!editor.pastePinned() && !editor.pasteFloorHeight() && editor.buildMode() == SectionBuildMode::Single,
              "starting a new room cannot inherit the previous section's placement locks");
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n'; ++failures;
    }
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
