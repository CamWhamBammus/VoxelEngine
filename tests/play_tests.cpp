#include "game.h"
#include "input.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

int main()
{
    int failures = 0;
    const auto check = [&](bool passed, const char* message) {
        if (!passed) { std::cerr << message << '\n'; ++failures; }
    };
    char name[] = "/tmp/voxel-play-XXXXXX";
    if (!mkdtemp(name)) return 1;
    const std::filesystem::path directory(name);
    try {
        voxel::Options options;
        options.seed = 42;
        options.worldPath = (directory / "world.vxl").string();
        { std::ofstream file(options.worldPath); file << "existing editor save"; }
        voxel::Game game(options);
        check(!game.editorMode() && game.world.seed() == 42, "default mode starts a fresh game");
        check(game.player.grounded(), "player starts standing on solid ground");
        check(game.camera.position() == game.player.eyePosition(), "camera follows the player's eyes");
        const auto spawn = game.player.feetPosition();
        game.update({}, 0.1f);
        check(game.player.feetPosition() == spawn, "released mouse pauses the player");

        voxel::InputActions actions;
        actions.active = true;
        actions.jump = true;
        game.update(actions, 1.0f / 60.0f);
        check(game.player.feetPosition().y > spawn.y && !game.player.grounded(), "space starts a grounded jump");
        actions.jump = false;
        for (int frame = 0; frame < 120; ++frame) game.update(actions, 1.0f / 60.0f);
        check(game.player.grounded() && std::abs(game.player.feetPosition().y - spawn.y) < 0.01f,
              "gravity lands the player back on terrain");
        actions.movement = {1, 0};
        game.update(actions, 0.1f);
        check(game.player.feetPosition() != spawn, "WASD drives the physical player");
        actions = {};
        actions.active = true;
        actions.save = actions.load = actions.undo = actions.redo = true;
        game.update(actions);
        std::ifstream file(options.worldPath);
        std::string contents;
        std::getline(file, contents);
        check(contents == "existing editor save", "game controls never overwrite or load editor saves");
        check(!game.hasUnsavedChanges(), "temporary game edits do not claim to be an editor save");

        // A flat workshop makes the aimed face and player body positions explicit.
        voxel::Game workshop(options);
        for (int z = voxel::World::Min.z; z < voxel::World::MaxExclusive.z; ++z) {
            for (int x = voxel::World::Min.x; x < voxel::World::MaxExclusive.x; ++x) {
                for (int y = 0; y < voxel::World::MaxExclusive.y; ++y) {
                    workshop.world.set({x, y, z}, y == 0 ? voxel::Block::Stone : voxel::Block::Air);
                }
            }
        }
        check(workshop.player.setFeetPosition(workshop.world, {0.5f, 1.0f, 0.5f}), "workshop player fits");
        workshop.camera.rotate(125.0f, 25.0f); // Face +X horizontally.
        workshop.world.set({3, 2, 0}, voxel::Block::Stone);
        actions = {};
        actions.active = true;
        actions.remove = true;
        workshop.update(actions);
        check(workshop.world.get({3, 2, 0}) == voxel::Block::Air && workshop.blockCount(voxel::Block::Stone) == 1,
              "mining adds exactly one block of the mined material");
        check(workshop.history.undoCount() == 0, "game mining cannot duplicate resources through editor undo");
        workshop.world.set({4, 2, 0}, voxel::Block::Stone);
        actions.remove = false;
        actions.place = true;
        actions.selectedBlock = voxel::Block::Stone;
        workshop.update(actions);
        check(workshop.world.get({3, 2, 0}) == voxel::Block::Stone && workshop.blockCount(voxel::Block::Stone) == 0,
              "placing consumes exactly one available block");
        workshop.update(actions);
        check(workshop.world.get({2, 2, 0}) == voxel::Block::Air && workshop.blockCount(voxel::Block::Stone) == 0,
              "empty inventory cannot create blocks or go negative");
        workshop.world.set({1, 1, 0}, voxel::Block::Stone);
        workshop.camera.rotate(0, -60.0f); // The adjacent side face would place into our legs.
        actions.selectedBlock = voxel::Block::Dirt;
        workshop.update(actions);
        check(workshop.world.get({0, 1, 0}) == voxel::Block::Air && workshop.blockCount(voxel::Block::Dirt) == 12
              && workshop.status == "Cannot place: your body is in the destination cell",
              "placing into the player's body is rejected without consuming stock");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ++failures;
    }
    std::filesystem::remove_all(directory);
    return failures ? 1 : 0;
}
