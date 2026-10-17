#include "game.h"
#include "input.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <unistd.h>

int main()
{
    int failures = 0;
    const auto check = [&](bool passed, const char* message) {
        if (!passed) { std::cerr << message << '\n'; ++failures; }
    };
    std::string pattern = (std::filesystem::temp_directory_path() / "voxel-game-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    if (!mkdtemp(name.data())) return 1;
    const std::filesystem::path directory(name.data());
    try {
        voxel::Options options;
        options.seed = 42;
        options.seedExplicit = true;
        options.smokeTest = true;
        options.worldExplicit = true;
        options.worldPath = (directory / "world.vxl").string();
        voxel::Game game(options);
        check(game.world.seed() == 42 && !game.hasUnsavedChanges(), "new game uses requested seed");

        voxel::InputActions cycle;
        cycle.materialStep = -1;
        game.update(cycle);
        check(game.selectedBlock == voxel::Block::Sand, "previous material wraps from grass to sand");
        cycle.materialStep = 1;
        game.update(cycle);
        check(game.selectedBlock == voxel::Block::Grass, "next material wraps from sand to grass");
        for (const auto expected : {voxel::Block::Dirt, voxel::Block::Stone, voxel::Block::Sand}) {
            game.update(cycle);
            check(game.selectedBlock == expected, "material cycling follows the numbered slots");
        }
        cycle.selectedBlock = voxel::Block::Grass;
        game.update(cycle);
        check(game.selectedBlock == voxel::Block::Grass, "number selection wins over cycling in the same frame");
        check(!game.hasUnsavedChanges() && game.history.undoCount() == 0,
              "material selection does not modify the world or history");

        const int height = voxel::terrainHeight(0, 0, options.seed);
        const glm::ivec3 surface(0, height, 0);
        const auto original = game.world.get(surface);
        game.camera.setPosition({0.5f, height + 12.5f, 0.5f});
        game.camera.rotate(125.0f, -65.0f);
        game.update({});
        check(!game.target, "default reach excludes distant terrain");
        auto extendedOptions = options;
        extendedOptions.editReach = 64;
        voxel::Game extended(extendedOptions);
        extended.camera = game.camera;
        extended.update({});
        check(extended.target && extended.target->position == surface, "configured reach can target distant terrain");
        check(extended.statusText(0, 0, 0, 0, true, true).find("EDIT REACH 64 BLOCKS") != std::string::npos,
              "help reports the configured reach");
        game.camera.setPosition({0.5f, height + 3.5f, 0.5f});
        game.update({});
        check(game.target && game.target->position == surface, "game finds the block under the crosshair");
        const auto inspected = game.statusText(24, 100, 120, 60, true, false);
        const auto targetDescription = "TARGET " + std::string(voxel::blockName(original))
            + " | XYZ 0, " + std::to_string(height) + ", 0";
        check(inspected.find(targetDescription) != std::string::npos
              && inspected.find("DIST 2.5/8 BLOCKS") != std::string::npos,
              "HUD identifies the aimed block and distance within the configured reach");
        check(game.statusText(24, 100, 120, 60, false, false).find("TARGET PAUSED") != std::string::npos,
              "released mouse describes targeting as paused");

        voxel::InputActions actions;
        actions.remove = true;
        game.update(actions);
        check(game.world.get(surface) == voxel::Block::Air && game.hasUnsavedChanges(),
              "remove action changes world and saved status");
        check(game.status == "Removed " + std::string(voxel::blockName(original)), "removal reports its material");
        actions = {};
        actions.undo = true;
        game.update(actions);
        check(game.world.get(surface) == original && !game.hasUnsavedChanges(),
              "undo returns to the saved state");

        actions = {};
        actions.selectedBlock = voxel::Block::Sand;
        actions.place = true;
        game.update(actions);
        const glm::ivec3 placed(0, height + 1, 0);
        check(game.world.get(placed) == voxel::Block::Sand, "material selection applies before placement");
        check(game.status == "Placed Sand", "successful placement is confirmed");
        actions = {};
        actions.save = true;
        game.update(actions);
        check(std::filesystem::is_regular_file(options.worldPath) && !game.hasUnsavedChanges(),
              "save action writes the session and clears unsaved status");

        actions = {};
        actions.selectedBlock = voxel::Block::Grass;
        actions.pick = true;
        game.update(actions);
        check(game.selectedBlock == voxel::Block::Sand, "middle-click picks the target material");
        check(game.status == "Selected Sand", "material picking is confirmed");
        actions = {};
        actions.remove = true;
        game.update(actions);
        check(game.hasUnsavedChanges(), "editing after save marks changes");
        actions = {};
        actions.undo = true;
        game.update(actions);
        check(!game.hasUnsavedChanges(), "undo back to last save clears changes");
        actions = {};
        actions.redo = true;
        game.update(actions);
        check(game.world.get(placed) == voxel::Block::Air, "redo applies the removal again");
        actions = {};
        actions.load = true;
        game.update(actions);
        check(game.world.get(placed) == voxel::Block::Sand && !game.hasUnsavedChanges()
              && game.history.undoCount() == 0, "reload restores blocks and clears session history");

        voxel::Options loadOptions = options;
        loadOptions.seed = 99;
        voxel::Game loaded(loadOptions);
        check(loaded.world.seed() == 42 && loaded.world.sameContents(game.world),
              "an explicit existing save takes precedence over the seed");

        actions = {};
        actions.remove = true;
        game.update(actions);
        const auto beforeFailure = game.world;
        const auto undoCount = game.history.undoCount();
        { std::ofstream corrupt(options.worldPath); corrupt << "broken save"; }
        actions = {};
        actions.load = true;
        game.update(actions);
        check(game.world.sameContents(beforeFailure) && game.history.undoCount() == undoCount
              && game.hasUnsavedChanges() && game.status.starts_with("File error:"),
              "failed reload keeps edits, history, and useful error status");
        check(game.statusText(24, 100, 120, 60, false, true).find("MOUSE RELEASED") != std::string::npos,
              "status text describes the input state");

        // Invalid actions should explain what to do without adding undo entries.
        auto feedbackOptions = options;
        feedbackOptions.worldExplicit = false;
        voxel::Game feedback(feedbackOptions);
        feedback.camera.rotate(125.0f, -65.0f);
        feedback.camera.setPosition({0.5f, 18.5f, 0.5f});
        feedback.world.set({0, 15, 0}, voxel::Block::Stone);
        const auto beforeRejectedPlace = feedback.world;
        actions = {};
        actions.place = true;
        feedback.update(actions);
        check(feedback.status == "Cannot place: outside world bounds"
              && feedback.history.undoCount() == 0 && feedback.world.sameContents(beforeRejectedPlace),
              "out-of-world placement explains the limit without editing");
        feedback.world.set({0, 15, 0}, voxel::Block::Air);
        feedback.world.set({0, 14, 0}, voxel::Block::Stone);
        feedback.camera.setPosition({0.5f, 15.5f, 0.5f});
        feedback.update(actions);
        check(feedback.status == "Cannot place: move out of the destination cell"
              && feedback.history.undoCount() == 0, "camera overlap gives useful guidance");
        feedback.camera.setPosition({0.5f, 14.5f, 0.5f});
        feedback.update(actions);
        check(feedback.status == "Cannot place: aim at a block face from outside it",
              "starting inside a block explains the missing face");
        feedback.camera.setPosition({1000, 1000, 1000});
        feedback.update(actions);
        check(feedback.status == "No block within editing reach" && feedback.history.undoCount() == 0,
              "editing empty space explains why nothing changed");
        check(feedback.statusText(0, 0, 0, 0, true, false).find("TARGET NONE | REACH 8 BLOCKS")
                  != std::string::npos, "HUD distinguishes empty aim from paused targeting");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ++failures;
    }
    std::filesystem::remove_all(directory);
    return failures ? 1 : 0;
}
