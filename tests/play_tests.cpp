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
        check(options.progressPath.empty() && voxel::parseOptions({}).progressPath == "progress.vxp",
              "direct test options disable profile I/O while ordinary CLI play enables progress");
        for (const auto argument : {"--editor", "--smoke-test", "--help"}) {
            const std::array<std::string_view,1> args{argument};
            check(voxel::parseOptions(args).progressPath.empty(), "editor, smoke and help leave progress files untouched");
        }
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

        actions = {};
        actions.active = true;
        actions.toolStep = -1;
        game.update(actions);
        check(game.equipment.selectedTool() == voxel::Tool::Compass, "previous tool wraps to the compass");
        actions.toolStep = 1;
        game.update(actions);
        check(game.equipment.selectedTool() == voxel::Tool::Pickaxe, "next tool wraps to the pickaxe");
        actions.selectedTool = 1;
        game.update(actions);
        check(game.equipment.selectedTool() == voxel::Tool::Lantern, "numbered tool selection wins over scrolling");
        actions.active = false;
        actions.selectedTool = 2;
        game.update(actions);
        check(game.equipment.selectedTool() == voxel::Tool::Lantern, "paused play ignores tool selection");
        actions = {};
        actions.active = actions.interact = actions.upgrade = true;
        game.update(actions);
        check(!game.smithOpen() && game.equipment.pickaxeTier == 1 && game.equipment.copper == 12,
              "interaction and upgrade requests cannot reach the blacksmith from spawn");
        check(game.player.setFeetPosition(game.world, voxel::sceneBlacksmithApproach()), "blacksmith approach has player clearance");
        game.camera.setPosition(game.player.eyePosition());
        check(game.nearBlacksmith(), "the authored workshop approach reaches its supported workbench");
        const auto station = voxel::sceneBlacksmithStation();
        const auto support = glm::ivec3(glm::floor(station)) - glm::ivec3(0, 1, 0);
        const auto supportMaterial = game.world.get(support);
        game.world.set(support, voxel::Block::Air);
        check(!game.nearBlacksmith(), "a removed workbench support disables the service");
        game.world.set(support, supportMaterial);
        const auto obstruction = glm::ivec3(glm::floor(glm::mix(game.player.eyePosition(), station, .5f)));
        const auto originalObstruction = game.world.get(obstruction);
        game.world.set(obstruction, voxel::Block::Stone);
        check(!game.nearBlacksmith(), "a wall between player and workbench blocks interaction");
        game.world.set(obstruction, originalObstruction);
        const auto serviceWorld = game.world;
        actions = {};
        actions.active = actions.interact = true;
        game.update(actions);
        check(game.smithOpen(), "E opens the nearby blacksmith service");
        const auto standing = game.player.feetPosition();
        actions.interact = false;
        actions.movement = {1, 1};
        actions.jump = true;
        actions.selectedTool = 2;
        game.update(actions, .1f);
        check(game.player.feetPosition() == standing && game.equipment.selectedTool() == voxel::Tool::Lantern,
              "blacksmith menu freezes walking, jumping and tool selection");
        actions.upgrade = true;
        game.update(actions);
        check(game.equipment.pickaxeTier == 2 && game.equipment.copper == 4,
              "Enter upgrades the pickaxe and charges starter copper");
        const auto upgraded = game.equipment;
        game.update(actions);
        check(game.equipment == upgraded && game.status.find("Not enough") != std::string::npos,
              "another upgrade cannot spend missing iron or regrant copper");
        game.equipment.iron = 8;
        game.update(actions);
        check(game.equipment.pickaxeTier == 3 && game.equipment.iron == 0, "blacksmith supports the final iron tier");
        const auto maximum = game.equipment;
        game.update(actions);
        check(game.equipment == maximum, "max-tier requests never charge ore");
        actions = {};
        actions.closeMenu = true;
        game.update(actions);
        check(!game.smithOpen(), "close-menu works even when gameplay input is paused");
        actions.active = actions.interact = true;
        actions.closeMenu = false;
        game.update(actions);
        check(game.smithOpen(), "blacksmith can be reopened");
        actions.interact = false;
        actions.respawn = true;
        game.update(actions);
        check(!game.smithOpen() && !game.nearBlacksmith() && game.equipment == maximum,
              "return to village closes the station without resetting equipment");
        check(game.world.sameContents(serviceWorld) && game.history.undoCount() == 0,
              "blacksmith service never edits terrain or editor history");

        auto persistentOptions = options;
        persistentOptions.progressPath = (directory / "progress.vxp").string();
        const auto openSmith = [&](voxel::Game& session) {
            check(session.player.setFeetPosition(session.world, voxel::sceneBlacksmithApproach()), "profile fixture reaches workshop");
            session.camera.setPosition(session.player.eyePosition());
            voxel::InputActions open;
            open.active = open.interact = true;
            session.update(open);
            check(session.smithOpen(), "profile fixture opens the smith");
        };
        voxel::InputActions upgradeAction;
        upgradeAction.active = upgradeAction.upgrade = true;
        voxel::Game persistent(persistentOptions);
        check(!std::filesystem::exists(persistentOptions.progressPath), "new profile is written on the first successful upgrade");
        openSmith(persistent);
        persistent.update(upgradeAction);
        check(persistent.equipment.pickaxeTier == 2 && voxel::loadEquipment(persistentOptions.progressPath).copper == 4,
              "successful upgrade persists its exact remaining balance");
        voxel::Game resumed(persistentOptions);
        check(resumed.equipment.pickaxeTier == 2 && resumed.equipment.copper == 4
              && resumed.status.find("STARTER") == std::string::npos,
              "restart restores the tier and never grants starter copper twice");
        { std::ofstream file(persistentOptions.progressPath); file << "damaged profile"; }
        voxel::Game damaged(persistentOptions);
        openSmith(damaged);
        const auto beforeDamage = damaged.equipment;
        damaged.update(upgradeAction);
        std::ifstream damagedFile(persistentOptions.progressPath);
        std::string damagedText;
        std::getline(damagedFile, damagedText);
        check(damaged.equipment == beforeDamage && damagedText == "damaged profile"
              && damaged.status.find("Progress unavailable") != std::string::npos,
              "malformed profiles remain intact and cannot be overwritten by an upgrade");
        persistentOptions.progressPath = (directory / "missing" / "progress.vxp").string();
        voxel::Game failedSave(persistentOptions);
        openSmith(failedSave);
        const auto beforeSave = failedSave.equipment;
        failedSave.update(upgradeAction);
        check(failedSave.equipment == beforeSave && failedSave.status.find("Upgrade not saved") != std::string::npos,
              "failed progress writes leave in-memory tier and all balances unchanged");

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
            check(expedition.statusText(0, 0, 0, 0, true, true).find("MINING AND EXPEDITIONS COMING LATER") != std::string::npos,
                  "hub controls accurately identify the current development scope");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ++failures;
    }
    std::filesystem::remove_all(directory);
    return failures ? 1 : 0;
}
