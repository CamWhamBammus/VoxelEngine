#include "game.h"
#include "input.h"
#include "scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}

int main()
{
    int failures = 0;
    const auto check = [&](bool passed, const std::string& message) {
        if (!passed) { std::cerr << message << '\n'; ++failures; }
    };
    char name[] = "/tmp/voxel-crossing-game-XXXXXX";
    if (!mkdtemp(name)) return 1;
    const std::filesystem::path directory(name);
    try {
        for (const int invalid : {-2, voxel::RootCrossingCount}) {
            voxel::Options invalidOptions;
            invalidOptions.rootCrossing = invalid;
            bool rejected = false;
            try { voxel::Game invalidGame(invalidOptions); }
            catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "invalid direct course indices fail before loading a scene");
        }
        for (const bool editor : {false, true}) {
            voxel::Options conflict;
            conflict.rootCrossing = 0;
            conflict.editor = editor;
            conflict.worldExplicit = !editor;
            bool rejected = false;
            try { voxel::Game invalidGame(conflict); }
            catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "direct options reject course/editor and course/world conflicts");
        }

        voxel::Equipment profile;
        profile.pickaxeTier = 2;
        profile.copper = 27;
        profile.iron = 13;
        profile.crystal = 5;
        const auto profilePath = directory / "progress.vxp";
        voxel::saveEquipment(profile, profilePath);
        const auto savedProfile = readFile(profilePath);
        voxel::Options options;
        options.rootCrossing = 0;
        options.progressPath = profilePath.string();
        voxel::Game game(options);
        voxel::World fixture(options.seed);
        auto course = voxel::buildRootCrossing(fixture, 0);
        check(game.rootCrossingActive() && game.rootCrossingIndex() == 0 && !game.editorMode()
              && game.player.grounded() && game.player.feetPosition() == course.spawn,
              "direct course startup places a grounded player at the authored entrance");
        check(!game.smithOpen() && !game.crossingMenuOpen() && !game.nearBlacksmith() && !game.nearExpeditionGate(),
              "test courses do not expose village services");
        check(game.compassTarget() == course.finish, "course compass guides toward its finish");
        check(game.equipment == profile, "direct course launch loads existing equipment");
        game.equipment.selectedSlot = 2;
        const auto carriedEquipment = game.equipment;
        const auto spawn = game.player.feetPosition();
        voxel::InputActions request;
        request.crossingMenu = request.returnToHub = request.respawn = request.restartCrossing = true;
        request.crossingStep = 1;
        request.selectedCrossing = 5;
        request.upgrade = true;
        request.movement = {1, 1};
        game.update(request, .1f);
        check(game.rootCrossingIndex() == 0 && !game.menuOpen() && game.player.feetPosition() == spawn
              && game.crossingElapsed() == 0.0f && game.equipment == carriedEquipment,
              "paused actions cannot change courses, open menus, spend ore, move, or advance the timer");

        request = {};
        request.active = true;
        game.update(request, .05f);
        check(std::abs(game.crossingElapsed() - .05f) < .0001f, "active course play advances its timer");
        for (const float invalid : {-1.0f, std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity()}) game.update(request, invalid);
        check(std::abs(game.crossingElapsed() - .05f) < .0001f, "invalid frame durations cannot corrupt the timer");
        game.update(request, 1000.0f);
        check(std::abs(game.crossingElapsed() - .15f) < .0001f, "course timer follows the same long-frame cap as physics");

        request.crossingMenu = true;
        game.update(request);
        check(game.crossingMenuOpen() && game.menuOpen() && game.crossingMenuSelection() == 0,
              "F6 opens a picker focused on the current course");
        const float pausedTime = game.crossingElapsed();
        request.crossingMenu = false;
        request.movement = {1, 1};
        request.jump = true;
        request.selectedTool = 0;
        request.respawn = true;
        request.restartCrossing = true;
        request.crossingStep = 1;
        game.update(request, .1f);
        check(game.player.feetPosition() == spawn && game.crossingElapsed() == pausedTime
              && game.equipment == carriedEquipment && game.rootCrossingIndex() == 0,
              "picker pauses gameplay and consumes tool, reset, and next-course actions");
        request = {};
        request.active = true;
        request.selectedCrossing = -1;
        game.update(request);
        request.selectedCrossing = voxel::RootCrossingCount;
        game.update(request);
        check(game.crossingMenuSelection() == 0, "out-of-range direct picker selections are ignored");
        request.selectedCrossing.reset();
        request.crossingMenuStep = -1;
        game.update(request);
        check(game.crossingMenuSelection() == voxel::RootCrossingCount - 1, "up wraps the picker to course ten");
        request.crossingMenuStep = 1;
        game.update(request);
        check(game.crossingMenuSelection() == 0, "down wraps the picker to course one");
        request.selectedCrossing = 3;
        request.selectedTool = 0;
        request.upgrade = true;
        game.update(request);
        check(game.rootCrossingIndex() == 3 && !game.menuOpen() && game.crossingElapsed() == 0.0f
              && game.equipment == carriedEquipment, "Enter launches the selected test without changing equipment");

        // Cover menu close while paused and physical next/previous wrap, with
        // fresh supported landings on every course transition.
        request = {};
        request.active = request.crossingMenu = true;
        game.update(request);
        request = {};
        request.closeMenu = true;
        game.update(request);
        check(!game.menuOpen() && game.rootCrossingIndex() == 3, "Esc/Tab can close a paused picker without leaving its course");
        request = {};
        request.active = true;
        request.crossingStep = 1;
        for (int i = 0; i < voxel::RootCrossingCount; ++i) {
            game.update(request);
            check(game.rootCrossingIndex() == (4 + i) % voxel::RootCrossingCount
                  && game.player.grounded() && game.crossingElapsed() == 0.0f
                  && game.equipment == carriedEquipment, "next cycles all ten supported courses without resetting equipment");
        }
        request.crossingStep = -1;
        for (int i = 0; i < 3; ++i) game.update(request);
        check(game.rootCrossingIndex() == 0, "previous cycles back to the first course");
        course = voxel::buildRootCrossing(fixture, 0);
        request = {};
        request.active = true;
        check(game.player.setFeetPosition(game.world, course.checkpoint), "checkpoint is a clear supported landing");
        game.update(request, .05f);
        check(game.crossingCheckpointReached() && !game.crossingCompleted(), "standing at the checkpoint activates it");
        const auto checkpointTime = game.crossingElapsed();
        check(game.player.setFeetPosition(game.world, {course.spawn.x, -2.0f, course.spawn.z}), "fall fixture can leave the course");
        game.update(request, .1f);
        check(game.player.feetPosition() == course.checkpoint && game.player.grounded() && game.crossingFalls() == 1
              && game.crossingCheckpointReached() && game.crossingElapsed() >= checkpointTime,
              "falling resets to the activated checkpoint and counts a fall without clearing run time");
        request.respawn = true;
        game.update(request);
        check(game.player.feetPosition() == course.checkpoint && game.crossingFalls() == 1,
              "manual checkpoint reset never increments the fall count");
        request.restartCrossing = true;
        game.update(request);
        check(game.player.feetPosition() == course.spawn && game.crossingFalls() == 0
              && !game.crossingCheckpointReached() && game.crossingElapsed() == 0.0f,
              "Shift+R takes precedence over R and starts a wholly fresh attempt");

        request = {};
        request.active = true;
        check(game.player.setFeetPosition(game.world, course.finish + glm::vec3(0, .1f, 0)), "airborne finish fixture fits");
        game.update(request);
        check(!game.crossingCompleted(), "passing above the finish without landing does not complete a crossing");
        check(game.player.setFeetPosition(game.world, course.finish), "finish has a grounded landing");
        game.update(request, .05f);
        check(game.crossingCompleted() && game.crossingBestTime() == game.crossingElapsed()
              && game.crossingBestTime() > 0.0f, "landing at the finish records a positive session best");
        const float best = game.crossingBestTime();
        game.update(request, .1f);
        check(game.crossingElapsed() == best && game.equipment == carriedEquipment
              && readFile(profilePath) == savedProfile, "completion freezes time and neither grants loot nor writes equipment");
        request.respawn = true;
        game.update(request);
        check(!game.crossingCompleted() && game.crossingElapsed() == best && game.crossingBestTime() == best,
              "R continues the attempt with its existing clock and retains the best result");
        request.restartCrossing = true;
        game.update(request);
        check(game.crossingElapsed() == 0.0f && game.crossingBestTime() == best,
              "full restart clears current time while preserving the course best");
        request = {};
        request.active = true;
        request.crossingStep = 1;
        game.update(request);
        check(game.crossingBestTime() == 0.0f, "uncompleted courses have their own empty session best");
        request.crossingStep = -1;
        game.update(request);
        check(game.crossingBestTime() == best, "returning to a completed course retains its session best");

        // The spiral changes heading substantially by its middle landing.
        // Returning there must face the next root, while a fresh attempt keeps
        // the authored entrance heading.
        request = {};
        request.active = request.crossingMenu = true;
        game.update(request);
        request.crossingMenu = false;
        request.selectedCrossing = 4;
        request.upgrade = true;
        game.update(request);
        const auto spiral = voxel::buildRootCrossing(fixture, 4);
        const auto horizontalFacing = [&] {
            const auto facing = game.camera.forward();
            return glm::normalize(glm::vec3(facing.x, 0, facing.z));
        };
        const glm::vec3 entranceFacing{std::cos(glm::radians(spiral.spawnYaw)), 0,
                                      std::sin(glm::radians(spiral.spawnYaw))};
        check(glm::dot(horizontalFacing(), entranceFacing) > .999f,
              "a new spiral attempt faces its authored entrance direction");
        check(game.player.setFeetPosition(game.world, spiral.checkpoint), "spiral checkpoint fixture fits");
        request = {};
        request.active = true;
        game.update(request, .05f);
        game.camera.rotate(123, 0);
        request.respawn = true;
        game.update(request);
        const auto waypoint = std::find(spiral.route.begin(), spiral.route.end(), spiral.checkpoint);
        check(waypoint != spiral.route.end() && waypoint + 1 != spiral.route.end(),
              "spiral checkpoint is followed by another route waypoint");
        if (waypoint != spiral.route.end() && waypoint + 1 != spiral.route.end()) {
            auto expected = *(waypoint + 1) - spiral.checkpoint;
            expected.y = 0;
            check(glm::dot(horizontalFacing(), glm::normalize(expected)) > .999f,
                  "checkpoint reset faces the next spiral waypoint instead of the original entrance heading");
        }
        request.restartCrossing = true;
        game.update(request);
        check(glm::dot(horizontalFacing(), entranceFacing) > .999f,
              "full restart restores the entrance heading after a checkpoint reset");

        request = {};
        request.active = request.returnToHub = true;
        game.update(request);
        check(!game.rootCrossingActive() && game.rootCrossingIndex() == -1 && !game.menuOpen()
              && game.player.feetPosition() == voxel::VillageSpawn && game.player.grounded()
              && game.compassTarget() == voxel::HeartTreeCenter && game.equipment == carriedEquipment,
              "B rebuilds the village, safely spawns, restores the compass target and retains all equipment");
        check(game.player.setFeetPosition(game.world, voxel::ExpeditionDeparture), "expedition gate approach fits");
        check(game.nearExpeditionGate(), "the northern departure area offers the test picker");
        request = {};
        request.active = request.interact = true;
        game.update(request);
        check(game.crossingMenuOpen() && !game.smithOpen(), "E at the expedition gate opens the course picker");
        request = {};
        request.active = true;
        request.selectedCrossing = 0;
        request.upgrade = true;
        game.update(request);
        check(game.crossingBestTime() == best && game.equipment == carriedEquipment,
              "the village visit never erases session bests or carried equipment");

        request = {};
        request.active = request.returnToHub = true;
        game.update(request);
        check(game.player.setFeetPosition(game.world, voxel::sceneBlacksmithApproach()), "return restores the blacksmith approach");
        request = {};
        request.active = request.interact = true;
        game.update(request);
        check(game.smithOpen(), "blacksmith works again after returning from a crossing");
        request.interact = false;
        request.crossingMenu = true;
        game.update(request);
        check(game.crossingMenuOpen() && !game.smithOpen(), "F6 replaces the smith panel with the test picker");
        check(readFile(profilePath) == savedProfile, "all traversal, test switches, and hub visits leave the profile untouched");

        options.smokeTest = true;
        voxel::Game smoke(options);
        check(smoke.equipment == voxel::Equipment{} && readFile(profilePath) == savedProfile,
              "even explicitly supplied progress paths are ignored by smoke runs");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ++failures;
    }
    std::filesystem::remove_all(directory);
    return failures ? 1 : 0;
}
