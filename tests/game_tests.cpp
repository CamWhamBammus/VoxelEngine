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

        voxel::InputActions actions;
        actions.remove = true;
        game.update(actions);
        check(game.world.get(surface) == voxel::Block::Air && game.hasUnsavedChanges(),
              "remove action changes world and saved status");
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
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ++failures;
    }
    std::filesystem::remove_all(directory);
    return failures ? 1 : 0;
}
