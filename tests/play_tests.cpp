#include "game.h"
#include "input.h"
#include "scene.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <unistd.h>

namespace {
void look(voxel::Game& game, glm::vec3 direction)
{
    direction = glm::normalize(direction);
    const auto current = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(current.z, current.x)),
                       glm::degrees(std::asin(direction.y) - std::asin(current.y)));
}
}

int main()
{
    int failures = 0;
    const auto check = [&](bool passed, const std::string& message) {
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
        check(!game.editorMode() && game.world.seed() == 42, "default mode starts the underground hub");
        check(game.player.grounded() && game.player.feetPosition() == voxel::VillageSpawn,
              "player starts on the village floor, not on the cavern roof");
        check(game.camera.position() == game.player.eyePosition(), "camera follows the player's eyes");
        check(!game.target, "normal play starts without an editor ray target");
        const auto spawn = game.player.feetPosition();
        game.update({}, 1000.0f);
        check(game.player.feetPosition() == spawn, "released mouse pauses the player");
        check(game.statusText(0, 0, 0, 0, false, false).find("PAUSED") != std::string::npos,
              "released mouse displays pause instructions");

        voxel::InputActions actions;
        actions.active = true;
        actions.jump = true;
        game.update(actions, 1.0f / 60.0f);
        check(game.player.feetPosition().y > spawn.y && !game.player.grounded(), "space starts a grounded jump");
        actions.jump = false;
        for (int frame = 0; frame < 120; ++frame) game.update(actions, 1.0f / 60.0f);
        check(game.player.grounded() && std::abs(game.player.feetPosition().y - spawn.y) < 0.01f,
              "gravity lands the player back on the village floor");
        actions.movement = {1, 0};
        game.update(actions, 0.1f);
        check(game.player.feetPosition() != spawn, "WASD drives the physical player");
        check(!game.target, "moving and looking do not enable editor targeting");

        for (const bool captured : {false, true}) {
            const auto hud = game.statusText(0, 0, 0, 0, captured, true);
            for (const auto prompt : {"WASD WALK", "SPACE JUMP", "SHIFT SPRINT", "MOUSE LOOK", "R RETURN"})
                check(hud.find(prompt) != std::string::npos, std::string("play help includes ") + prompt);
            for (const auto prompt : {"SELECTED", "BACKPACK", "INVENTORY", "LEFT REMOVE", "LEFT MINE", "RIGHT PLACE", "RIGHT BUILD",
                                      "MIDDLE PICK", "1-9 MATERIAL", "F5", "F9", "UNDO", "REDO", "TARGET", "COAL:", "COPPER:"})
                check(hud.find(prompt) == std::string::npos, std::string("play HUD excludes editor/inventory prompt ") + prompt);
        }

        // Real ore, an unobstructed ray, and an empty neighboring cell make
        // these actions effective if normal play accidentally reaches the editor.
        voxel::Game workshop(options);
        for (int z = voxel::World::Min.z; z < voxel::World::MaxExclusive.z; ++z) {
            for (int x = voxel::World::Min.x; x < voxel::World::MaxExclusive.x; ++x) {
                for (int y = 0; y < voxel::World::MaxExclusive.y; ++y)
                    workshop.world.set({x, y, z}, y == 0 ? voxel::Block::Stone : voxel::Block::Air);
            }
        }
        check(workshop.player.setFeetPosition(workshop.world, {0.5f, 1.0f, 0.5f}), "action fixture fits the player");
        workshop.camera.setPosition(workshop.player.eyePosition());
        look(workshop, {1, 0, 0});
        workshop.world.set({3, 2, 0}, voxel::Block::CopperOre);
        const auto aimedOre = voxel::raycast(workshop.camera.position(), workshop.camera.forward(), 8,
            [&workshop](glm::ivec3 position) { return workshop.world.get(position); });
        check(aimedOre && aimedOre->position == glm::ivec3(3, 2, 0), "regression fixture really aims at exposed ore");
        check(aimedOre && voxel::checkPlacement(workshop.world, *aimedOre, workshop.selectedBlock,
                                               workshop.camera.position()).failure == voxel::PlacementFailure::None,
              "regression fixture has an empty usable placement destination");

        // Seed both stacks so incorrectly processing undo or redo would alter content.
        workshop.history.apply(workshop.world, {6, 1, 0}, voxel::Block::Dirt);
        workshop.history.apply(workshop.world, {7, 1, 0}, voxel::Block::Planks);
        workshop.history.undo(workshop.world);
        check(workshop.history.undoCount() == 1 && workshop.history.redoCount() == 1, "both edit-history regression stacks are populated");
        workshop.world.takeDirtyChunks();
        const auto untouched = workshop.world;
        const auto selected = workshop.selectedBlock;
        const auto status = workshop.status;
        const std::array<std::pair<const char*, voxel::InputActions>, 10> editorActions{{
            {"remove", {.remove = true}}, {"place", {.place = true}}, {"pick", {.pick = true}},
            {"select material", {.selectedBlock = voxel::Block::RoofTiles}},
            {"next material", {.materialStep = 1}}, {"previous material", {.materialStep = -1}},
            {"save", {.save = true}}, {"load", {.load = true}},
            {"undo", {.undo = true}}, {"redo", {.redo = true}},
        }};
        for (const bool active : {false, true}) {
            for (const auto& [name, request] : editorActions) {
                auto ignored = request;
                ignored.active = active;
                workshop.update(ignored, 1.0f / 60.0f);
                const auto label = std::string(active ? "active " : "paused ") + name;
                check(workshop.world.sameContents(untouched) && workshop.world.takeDirtyChunks().empty(),
                      label + " cannot modify or dirty the play world");
                check(workshop.history.undoCount() == 1 && workshop.history.redoCount() == 1,
                      label + " cannot consume or clear editor history");
                check(workshop.selectedBlock == selected && workshop.status == status,
                      label + " cannot change selected material or play status");
                check(!workshop.target, label + " cannot create an editor ray target");
            }
        }
        std::ifstream file(options.worldPath);
        std::string contents;
        std::getline(file, contents);
        check(contents == "existing editor save", "play actions never load or overwrite the editor save");
        check(!workshop.hasUnsavedChanges(), "play mode never reports editor save changes");

        for (const std::uint32_t seed : {42u, 1337u, 4294967295u}) {
            options.seed = seed;
            voxel::Game expedition(options);
            const auto original = expedition.world;
            // A blocked spawn exercises safe relocation without rebuilding the scene.
            check(expedition.player.setFeetPosition(expedition.world, voxel::VillageSpawn + glm::vec3(0, 1, 0)),
                  "village has vertical space for the return fixture");
            const glm::ivec3 occupiedSpawn(glm::floor(voxel::VillageSpawn));
            expedition.world.set(occupiedSpawn, voxel::Block::Planks);
            const auto obstructed = expedition.world;
            actions = {};
            actions.active = actions.respawn = true;
            expedition.update(actions, 0.1f);
            check(expedition.player.grounded() && expedition.player.feetPosition().y < voxel::World::MaxExclusive.y
                  && !expedition.player.overlapsBlock(occupiedSpawn), "return finds safe space below the ceiling when spawn is obstructed");
            check(expedition.world.sameContents(obstructed) && !expedition.world.sameContents(original),
                  "return relocates the player without mutating or resetting the scene");
            actions.respawn = false;
            const auto feet = expedition.player.feetPosition();
            expedition.update(actions, 1000.0f);
            check(expedition.player.feetPosition() == feet, "exploration has no challenge countdown or time limit");
            check(!expedition.target, "returning to the village does not enable editor targeting");
            check(expedition.statusText(0, 0, 0, 0, true, true).find("EXPEDITIONS AND BUILDING SERVICES COMING LATER") != std::string::npos,
                  "hub controls accurately identify the current development scope");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ++failures;
    }
    std::filesystem::remove_all(directory);
    return failures ? 1 : 0;
}
