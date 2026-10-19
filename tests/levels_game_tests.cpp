#include "game.h"
#include "input.h"
#include "levels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
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
void type(voxel::Game& game, std::string text)
{
    voxel::InputActions action; action.active = true; action.devText = std::move(text); game.update(action);
}
void advance(voxel::Game& game, int frames = 5)
{
    voxel::InputActions action; action.active = true;
    for (int i = 0; i < frames; ++i) game.update(action, .1f);
}
bool walkTo(voxel::Game& game, glm::vec3 destination)
{
    voxel::InputActions action; action.active = true; action.movement = {0, 1};
    for (int i = 0; i < 240; ++i) {
        if (game.levelController().screen() != voxel::LevelScreen::Play) return true;
        auto delta = destination - game.player.feetPosition();
        delta.y = 0;
        if (glm::length(delta) < .15f) return true;
        const auto facing = game.camera.forward();
        game.camera.rotate(glm::degrees(std::atan2(delta.z, delta.x) - std::atan2(facing.z, facing.x)),
                           -glm::degrees(std::asin(facing.y)));
        game.update(action, 1.0f / 60.0f);
    }
    return false;
}
std::string read(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
}

int main()
{
    int failures = 0;
    const auto check = [&](bool result, const char* message) {
        if (!result) { std::cerr << message << '\n'; ++failures; }
    };
    char pattern[] = "/tmp/voxel-level-game-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern), sections = root / "sections", levels = root / "levels";
    try {
        check(voxel::parseOptions(std::array<std::string_view, 1>{"--level-creator"}).levelCreator,
              "--level-creator opens the level assembler");
        for (const std::vector<std::string_view>& args : {
            std::vector<std::string_view>{"--level-creator", "--dev"}, {"--level-creator", "--editor"},
            {"--level-creator", "--root-crossing", "1"}, {"--level-creator", "--world", "bad.vxl"}}) {
            bool rejected = false;
            try { voxel::parseOptions(args); } catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "conflicting level creator startup modes are rejected");
        }
        voxel::World world(123);
        voxel::buildBlankSection(world);
        // Store water in the accepted section itself so level loading, not a
        // runtime patch, supplies the pool used by the swimming checks below.
        for (int z = 18; z <= 24; ++z)
            for (int x = 18; x <= 24; ++x)
                for (int y = 9; y <= 18; ++y)
                    world.set({x, y, z}, voxel::Block::Water);
        voxel::SectionMetadata first;
        first.name = "Root entry"; first.tested = true;
        first.spawn = {.5f, 9, 4.5f}; first.finish = {.5f, 9, -.5f}; first.checkpoint = {.5f, 9, 2.5f};
        const auto firstPath = voxel::acceptSection(world, first, {}, sections);
        voxel::SectionMetadata second = first;
        second.name = "Garden turn"; second.category = voxel::SectionCategory::Exploration;
        second.spawn = {8.5f, 9, 8.5f}; second.finish = {3.5f, 9, 8.5f}; second.checkpoint.reset();
        world.set({12, 9, 12}, voxel::Block::Grass);
        const std::array<voxel::HubAsset, 1> decor{{{voxel::HubAssetKind::Fern, {14, 9, 14}, 0, 0}}};
        const auto secondPath = voxel::acceptSection(world, second, decor, sections);
        voxel::SectionMetadata crystal = first;
        crystal.name = "Crystal entry"; crystal.world = voxel::SectionWorld::CrystalHollows;
        voxel::acceptSection(world, crystal, {}, sections);

        voxel::Options options; options.levelCreator = true;
        options.sectionLibrary = sections.string(); options.levelLibrary = levels.string();
        voxel::Game game(options);
        const auto equipment = game.equipment;
        auto& controller = game.levelController();
        check(game.levelCreatorActive() && game.levelMenuOpen() && !game.levelActive(),
              "level creator begins with a modal world choice");
        click(game, voxel::LevelUiWorldBase);
        check(controller.screen() == voxel::LevelScreen::Library && controller.sections().size() == 2,
              "creator only offers accepted sections in the selected world");
        const auto firstIndex = static_cast<int>(std::find_if(controller.sections().begin(), controller.sections().end(),
            [&](const auto& entry) { return entry.path == firstPath; }) - controller.sections().begin());
        const auto secondIndex = static_cast<int>(std::find_if(controller.sections().begin(), controller.sections().end(),
            [&](const auto& entry) { return entry.path == secondPath; }) - controller.sections().begin());
        click(game, voxel::LevelUiNew);
        check(controller.screen() == voxel::LevelScreen::Compose && controller.definition().sections.empty(),
              "new level starts with an empty ordered list");
        click(game, voxel::LevelUiName); type(game, "First expedition");
        click(game, voxel::LevelUiNumber); type(game, "5");
        click(game, voxel::LevelUiSectionBase + firstIndex);
        click(game, voxel::LevelUiSectionBase + secondIndex);
        click(game, voxel::LevelUiSectionBase + firstIndex);
        check(controller.definition().sections.size() == 3, "the same section can be added more than once");
        click(game, voxel::LevelUiMoveUp);
        check(controller.definition().sections[0] == controller.definition().sections[1], "queue items can be reordered");
        click(game, voxel::LevelUiRemove);
        check(controller.definition().sections.size() == 2, "selected queue items can be removed");
        auto draft = voxel::loadLevelDraft(levels, voxel::SectionWorld::LushCaves);
        check(draft && draft->number == 5 && draft->name == "First expedition" && draft->sections.size() == 2,
              "name, number and section order are autosaved independently of publication");
        click(game, voxel::LevelUiPublish);
        const auto published = voxel::listLevels(levels);
        check(published.size() == 1 && published[0].definition.number == 5, "publishing creates an in-game numbered level");
        const auto manifestBefore = read(published[0].path);
        click(game, voxel::LevelUiSectionBase + firstIndex);
        click(game, voxel::LevelUiPublish);
        check(controller.screen() == voxel::LevelScreen::Replace && read(published[0].path) == manifestBefore,
              "publishing over a claimed level number requires explicit replace confirmation");
        click(game, voxel::LevelUiCancel);
        check(controller.screen() == voxel::LevelScreen::Compose && read(published[0].path) == manifestBefore,
              "cancelling replacement preserves the current published level");
        click(game, voxel::LevelUiPublish); click(game, voxel::LevelUiReplace);
        check(voxel::listLevels(levels)[0].definition.sections.size() == 3,
              "confirmed replacement publishes the revised order");
        press(game, &voxel::InputActions::levelCreator);
        check(!game.levelCreatorActive() && !game.levelMenuOpen(), "F11 closes the creator after its draft is saved");
        press(game, &voxel::InputActions::levelMenu);
        click(game, voxel::LevelUiWorldBase);
        check(!controller.creator() && controller.entries().size() == 1, "F4 opens the playable level catalog");
        click(game, voxel::LevelUiEntryBase);
        check(game.levelActive() && controller.screen() == voxel::LevelScreen::Play,
              "published catalog entries load the first section for normal play");
        check(game.player.feetPosition() == first.spawn, "level begins at the authored Start");
        check(!game.devMode() && !game.devFlying() && !game.nearBlacksmith(), "level play cannot use creative mode or hub purchases");
        voxel::InputActions idle;
        const float elapsed = controller.elapsed();
        game.update(idle, 100);
        check(controller.elapsed() == elapsed, "paused gameplay never advances the level timer");
        const glm::vec3 swimStart{21.5f, 13, 21.5f};
        check(game.world.get({21, 13, 21}) == voxel::Block::Water
              && game.player.setFeetPosition(game.world, swimStart),
              "published sections retain their authored swimming water when loaded into a level");
        game.camera.setPosition(game.player.eyePosition());
        voxel::InputActions swim;
        swim.active = swim.jump = true;
        for (int frame = 0; frame < 45; ++frame) game.update(swim, 1.0f / 60.0f);
        check(game.player.inWater() && game.player.feetPosition().y > swimStart.y + .5f
              && game.camera.position() == game.player.eyePosition(),
              "authored expedition play supports held Space swimming with the physical camera");
        const auto pausedSwim = game.player.feetPosition();
        const auto pausedElapsed = controller.elapsed();
        swim.active = false;
        swim.descend = true;
        game.update(swim, .1f);
        check(game.player.feetPosition() == pausedSwim && controller.elapsed() == pausedElapsed,
              "paused expedition play freezes swimming and its run timer");
        check(game.player.setFeetPosition(game.world, swimStart), "level dive fixture resets in the pool");
        swim = {};
        swim.active = swim.descend = true;
        for (int frame = 0; frame < 30; ++frame) game.update(swim, 1.0f / 60.0f);
        check(game.player.underwater() && game.player.feetPosition().y < swimStart.y - .5f,
              "authored expedition play forwards the descend control to water movement");
        press(game, &voxel::InputActions::respawn);
        check(game.player.feetPosition() == first.spawn && !game.player.inWater() && !game.player.underwater(),
              "resetting underwater returns to the current section Start with dry player state");
        voxel::InputActions settle; settle.active = true;
        game.update(settle, .1f);
        check(game.player.grounded() && game.player.feetPosition() == first.spawn,
              "level reset clears diving momentum before the normal route continues");
        voxel::InputActions tools; tools.active = true; tools.selectedTool = 2;
        game.update(tools);
        check(game.equipment.selectedSlot == 2 && game.compassTarget() == first.finish,
              "normal tool selection and compass-to-End remain available");
        check(walkTo(game, *first.checkpoint) && controller.checkpointReached(), "walking onto the authored checkpoint activates it");
        press(game, &voxel::InputActions::respawn);
        check(game.player.feetPosition() == *first.checkpoint, "R respawns at the current section checkpoint");
        check(walkTo(game, first.finish) && controller.screen() == voxel::LevelScreen::Transition,
              "walking to End begins an explicit section transition");
        const auto firstContents = game.world;
        check(controller.sectionIndex() == 0 && game.world.sameContents(firstContents), "transition initially retains the finished room");
        advance(game);
        check(controller.sectionIndex() == 1 && game.player.feetPosition() == second.spawn,
              "the next room spawns at its own Start, regardless of its position");
        check(controller.assets().size() == 1 && !controller.checkpointReached(), "placed assets load and checkpoints reset between sections");
        check(game.world.get({12, 9, 12}) == voxel::Block::Grass, "next section installs its authored voxel snapshot");
        press(game, &voxel::InputActions::restartCrossing);
        check(controller.sectionIndex() == 0 && controller.elapsed() == 0 && game.player.feetPosition() == first.spawn,
              "Shift+R restarts the whole level at section one");
        walkTo(game, first.finish); advance(game);
        walkTo(game, second.finish); advance(game);
        check(controller.sectionIndex() == 2 && game.player.feetPosition() == first.spawn,
              "duplicate section references replay as a separate ordered occurrence");
        walkTo(game, first.finish);
        check(controller.screen() == voxel::LevelScreen::Complete && game.levelMenuOpen(), "last End completes the level");
        const auto finalTime = controller.elapsed(); advance(game);
        check(controller.elapsed() == finalTime, "completed run time is frozen");
        click(game, voxel::LevelUiReplay);
        check(controller.screen() == voxel::LevelScreen::Play && controller.sectionIndex() == 0 && controller.elapsed() == 0,
              "completed levels can be replayed");
        press(game, &voxel::InputActions::returnToHub);
        check(!game.levelActive() && !game.menuOpen(), "B returns to the village");
        check(game.equipment.pickaxeTier == equipment.pickaxeTier && game.equipment.copper == equipment.copper && game.equipment.iron == equipment.iron && game.equipment.crystal == equipment.crystal,
              "level creation and play preserve equipment and resources");

        // A missing later section must not destroy the active room or crash.
        voxel::LevelDefinition broken = published[0].definition;
        broken.sections = {firstPath.lexically_relative(sections), secondPath.lexically_relative(sections)};
        check(controller.start(game, broken), "valid references are checked before a run begins");
        const auto secondBackup = root / "second-backup.vxs";
        std::filesystem::rename(secondPath, secondBackup);
        walkTo(game, first.finish);
        const auto activeRoom = game.world;
        advance(game);
        check(controller.screen() == voxel::LevelScreen::Error && game.world.sameContents(activeRoom)
              && controller.sectionIndex() == 0, "a section removed during play leaves the previous room intact with retry");
        click(game, voxel::LevelUiRetry);
        check(controller.screen() == voxel::LevelScreen::Error && game.world.sameContents(activeRoom), "failed retry stays recoverable");
        std::filesystem::rename(secondBackup, secondPath);
        click(game, voxel::LevelUiRetry);
        check(controller.screen() == voxel::LevelScreen::Play && controller.sectionIndex() == 1,
              "restoring the section lets retry continue the same level");
        press(game, &voxel::InputActions::returnToHub);
        const auto village = game.world;
        broken.sections.push_back("lush-caves/parkour/missing.vxs");
        check(!controller.start(game, broken) && !game.levelActive() && game.world.sameContents(village),
              "bad references are rejected before replacing the hub");

        press(game, &voxel::InputActions::levelCreator); click(game, voxel::LevelUiWorldBase);
        click(game, voxel::LevelUiResume);
        check(controller.definition().sections.size() == 3 && controller.definition().name == "First expedition",
              "world-specific assembly drafts survive leaving and returning");
        click(game, voxel::LevelUiNumber); type(game, "0");
        click(game, voxel::LevelUiPublish);
        check(controller.screen() == voxel::LevelScreen::Compose && voxel::listLevels(levels).size() == 1,
              "invalid level number cannot publish");
        check(!game.saveAuthoringDraft() && game.hasUnsavedChanges(), "invalid pending input is not silently discarded on exit");
        click(game, voxel::LevelUiNumber); type(game, "7");
        check(game.saveAuthoringDraft(), "corrected level input can be safely saved");
        const auto corruptPath = voxel::saveLevel(controller.definition(), sections, levels);
        { std::ofstream damaged(corruptPath, std::ios::binary | std::ios::trunc); damaged << "broken manifest"; }
        click(game, voxel::LevelUiPublish);
        check(controller.screen() == voxel::LevelScreen::Replace,
              "a corrupt manifest still gets a visible replace decision for its claimed number");
        click(game, voxel::LevelUiReplace);
        check(voxel::loadLevel(corruptPath).number == 7, "confirmed publication repairs the corrupt manifest");
    } catch (const std::exception& error) {
        std::cerr << "Level game test exception: " << error.what() << '\n'; ++failures;
    }
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
