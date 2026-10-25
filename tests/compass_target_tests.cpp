#include "game.h"
#include "input.h"
#include "scene.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void press(Game& game, bool InputActions::* key)
{
    InputActions action; action.active = true; action.*key = true; game.update(action);
}
void click(Game& game, int id)
{
    InputActions action; action.active = true; action.devClick = id; game.update(action);
}
void tick(Game& game, int frames = 1)
{
    InputActions action; action.active = true;
    for (int i = 0; i < frames; ++i) game.update(action, .1f);
}
void position(Game& game, glm::vec3 feet)
{
    require(game.player.setFeetPosition(game.world, feet), "compass fixture position must fit");
    game.camera.setPosition(game.player.eyePosition());
}
void respawn(Game& game, glm::vec3 destination, glm::vec3 exit)
{
    for (int i = 0; i < 20 && game.health().dead(); ++i) {
        require(game.compassTarget() == exit, "dying must not redirect the compass toward the respawn marker");
        tick(game);
    }
    require(!game.health().dead() && game.player.feetPosition() == destination,
            "the fixture must finish respawning at its expected marker");
    require(game.compassTarget() == exit, "respawning must keep the current section's exit target");
}
}

int main()
{
    using namespace voxel;
    char pattern[] = "/tmp/voxel-compass-target-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern), sections = root / "sections";
    int result = 0;
    try {
        World world;
        buildBlankSection(world);
        SectionMetadata first;
        first.name = "West exit"; first.tested = true;
        first.spawn = {.5f, 9, 8.5f}; first.finish = {-12.5f, 9, .5f};
        first.checkpoint = glm::vec3{.5f, 9, .5f};
        const auto firstPath = acceptSection(world, first, {}, sections);
        SectionMetadata second = first;
        second.name = "East exit"; second.spawn = {-8.5f, 9, 8.5f};
        second.finish = {12.5f, 9, -8.5f}; second.checkpoint.reset();
        const auto secondPath = acceptSection(world, second, {}, sections);
        LevelDefinition level;
        level.name = "Changing compass destinations";
        level.sections = {firstPath.lexically_relative(sections), secondPath.lexically_relative(sections)};
        Options options;
        options.sectionLibrary = sections.string(); options.levelLibrary = (root / "levels").string();
        Game game(options);
        require(game.compassTarget() == HeartTreeCenter, "the village compass points to the central tree");
        auto& controller = game.levelController();
        require(controller.start(game, level) && game.compassTarget() == first.finish,
                "starting a level targets the first section's authored End");
        game.equipment.selectedSlot = 2;
        game.camera.rotate(137, 45);
        require(game.compassTarget() == first.finish, "looking around does not move the world-space compass target");
        require(game.damagePlayer(100), "the first section must allow a death before its checkpoint");
        respawn(game, first.spawn, first.finish);
        position(game, *first.checkpoint); tick(game);
        require(controller.checkpointReached() && game.compassTarget() == first.finish,
                "activating a checkpoint leaves the compass pointing at End");
        position(game, {18.5f, -2, 18.5f}); tick(game);
        require(game.health().dead(), "falling below the section must enter the death flow");
        respawn(game, *first.checkpoint, first.finish);

        position(game, first.finish); tick(game);
        require(controller.screen() == LevelScreen::Transition && game.compassTarget() == first.finish,
                "the transition keeps the finished room's End until the next room loads");
        tick(game, 4);
        require(controller.screen() == LevelScreen::Play && controller.sectionIndex() == 1
                && game.player.feetPosition() == second.spawn && game.compassTarget() == second.finish,
                "the next section immediately supplies its own exit to the compass");
        require(game.damagePlayer(100), "the second section must allow a death");
        respawn(game, second.spawn, second.finish);
        press(game, &InputActions::restartCrossing);
        require(controller.sectionIndex() == 0 && game.compassTarget() == first.finish,
                "restarting the expedition restores the first section's compass destination");
        press(game, &InputActions::returnToHub);
        require(!game.levelActive() && game.compassTarget() == HeartTreeCenter,
                "leaving an expedition restores the village destination");

        options.dev = true;
        Game builder(options);
        click(builder, SectionUiWorldBase);
        const auto& entries = builder.sectionEditor().entries();
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& value) {
            return value.path == firstPath;
        });
        require(entry != entries.end(), "the authored compass fixture must appear in the section library");
        click(builder, SectionUiLibraryEntryBase + static_cast<int>(entry - entries.begin()));
        press(builder, &InputActions::devTest);
        require(builder.sectionEditor().screen() == SectionEditorScreen::Test
                && builder.compassTarget() == first.finish,
                "section playtests target the authored End instead of the village or Start");
        position(builder, *first.checkpoint); tick(builder);
        require(builder.sectionEditor().checkpointReached(), "the playtest checkpoint must activate");
        require(builder.damagePlayer(100), "the playtest must allow checkpoint death");
        respawn(builder, *first.checkpoint, first.finish);
        press(builder, &InputActions::devTest);
        press(builder, &InputActions::devToggle);
        require(!builder.devMode() && builder.compassTarget() == HeartTreeCenter,
                "closing the section builder clears its exit destination");

        options.dev = false; options.rootCrossing = 0;
        Game crossing(options);
        auto course = buildRootCrossing(world, 0);
        require(crossing.compassTarget() == course.finish, "Root Crossing uses the course's finish marker");
        InputActions next; next.active = true; next.crossingStep = 1;
        crossing.update(next);
        course = buildRootCrossing(world, 1);
        require(crossing.rootCrossingIndex() == 1 && crossing.compassTarget() == course.finish,
                "changing Root Crossing courses refreshes the compass destination");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result = 1;
    }
    std::filesystem::remove_all(root);
    if (!result) std::cout << "Compass destinations across sections, respawns and hub transitions passed\n";
    return result;
}
