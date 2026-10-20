#include "game.h"
#include "input.h"
#include "save.h"
#include "scene.h"
#include "asset_collision.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace voxel {

Options parseOptions(std::span<const std::string_view> arguments)
{
    Options options;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const auto argument = arguments[i];
        const auto value = [&]() {
            if (++i == arguments.size() || arguments[i].empty()
                || arguments[i].starts_with("--") || arguments[i] == "-h") {
                throw std::invalid_argument("Missing value for " + std::string(argument));
            }
            return arguments[i];
        };
        if (argument == "--help" || argument == "-h") options.help = true;
        else if (argument == "--editor") options.editor = true;
        else if (argument == "--dev") options.dev = true;
        else if (argument == "--level-creator") options.levelCreator = true;
        else if (argument == "--smoke-test") options.smokeTest = true;
        else if (argument == "--seed") {
            options.seedExplicit = true;
            const auto seed = value();
            const auto result = std::from_chars(seed.data(), seed.data() + seed.size(), options.seed);
            if (result.ec != std::errc{} || result.ptr != seed.data() + seed.size()) {
                throw std::invalid_argument("Seed must be an integer from 0 to 4294967295");
            }
        } else if (argument == "--root-crossing") {
            int number = 0;
            const auto crossing = value();
            const auto result = std::from_chars(crossing.data(), crossing.data() + crossing.size(), number);
            if (result.ec != std::errc{} || result.ptr != crossing.data() + crossing.size()
                || number < 1 || number > RootCrossingCount) {
                throw std::invalid_argument("Root Crossing must be an integer from 1 to 10");
            }
            options.rootCrossing = number - 1;
        } else if (argument == "--reach") {
            const auto reach = value();
            const auto result = std::from_chars(reach.data(), reach.data() + reach.size(), options.editReach);
            if (result.ec != std::errc{} || result.ptr != reach.data() + reach.size()
                || options.editReach < 1 || options.editReach > 64) {
                throw std::invalid_argument("Editing reach must be an integer from 1 to 64");
            }
        } else if (argument == "--world") {
            options.worldPath = value();
            options.worldExplicit = true;
        } else if (argument == "--screenshot") {
            options.screenshot = value();
            options.smokeTest = true;
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }
    if (options.levelCreator && (options.dev || options.editor || options.rootCrossing >= 0 || options.worldExplicit) && !options.help) {
        throw std::invalid_argument("Level creator cannot be combined with --dev, --editor, --world or --root-crossing");
    }
    if (options.dev && (options.editor || options.rootCrossing >= 0 || options.worldExplicit) && !options.help) {
        throw std::invalid_argument("Section dev mode cannot be combined with --editor, --world or --root-crossing");
    }
    if (options.rootCrossing >= 0 && (options.editor || options.worldExplicit) && !options.help) {
        throw std::invalid_argument("Root Crossing tests cannot be combined with --editor or --world");
    }
    if (options.worldExplicit && !options.editor && !options.help) {
        throw std::invalid_argument("Use --editor with --world; game runs start with fresh terrain");
    }
    if (!options.editor && !options.smokeTest && !options.help) options.progressPath = "progress.vxp";
    return options;
}

// An explicit --seed starts fresh unless an explicit existing world file was requested.
Game::Game(const Options& options)
    : world(options.seed), sectionEditor_(options.sectionLibrary), levelController_(options.sectionLibrary, options.levelLibrary), savedWorld_(world), savePath_(options.worldPath), editReach_(options.editReach),
      editorMode_(options.editor), progressPath_(options.smokeTest || options.editor ? "" : options.progressPath)
{
    if (options.levelCreator && (options.dev || options.editor || options.rootCrossing >= 0 || options.worldExplicit)) {
        throw std::invalid_argument("Level creator cannot be combined with --dev, --editor, --world or --root-crossing");
    }
    if (options.dev && (options.editor || options.rootCrossing >= 0 || options.worldExplicit)) {
        throw std::invalid_argument("Section dev mode cannot be combined with --editor, --world or --root-crossing");
    }
    if (editReach_ < 1 || editReach_ > 64) {
        throw std::invalid_argument("Editing reach must be an integer from 1 to 64");
    }
    if (options.rootCrossing < -1 || options.rootCrossing >= RootCrossingCount) {
        throw std::invalid_argument("Root Crossing index must be from 0 to 9, or -1 for the hub");
    }
    if (options.rootCrossing >= 0 && (editorMode_ || options.worldExplicit)) {
        throw std::invalid_argument("Root Crossing tests cannot be combined with --editor or --world");
    }
    if (options.worldExplicit && !editorMode_) {
        throw std::invalid_argument("Use --editor with --world");
    }
    const bool loadExisting = editorMode_
        && (options.worldExplicit || (!options.smokeTest && !options.seedExplicit))
        && std::filesystem::exists(savePath_);
    if (loadExisting) {
        world = loadWorld(savePath_);
        savedWorld_ = world;
        status = "Loaded " + savePath_;
    } else {
        status = "New world - F5 to save";
    }
    if (!editorMode_) {
        if (options.rootCrossing >= 0) startRootCrossing(options.rootCrossing);
        else {
            buildVillage();
            if (!returnToVillage()) throw std::runtime_error("Village spawn has no room for the player");
            status = "F6 ROOT CROSSING TESTS | BLACKSMITH SOUTHWEST | 12 STARTER COPPER";
        }
        if (!progressPath_.empty()) {
            try {
                if (std::filesystem::exists(progressPath_)) {
                    equipment = loadEquipment(progressPath_);
                    if (!rootCrossingActive())
                        status = "EQUIPMENT LOADED | F6 ROOT CROSSING TESTS | BLACKSMITH SOUTHWEST";
                }
            } catch (const std::exception& error) {
                progressError_ = std::string("Progress unavailable: ") + error.what();
                status = progressError_;
            }
        }
    }
    if (editorMode_) findTarget();
    if (options.dev) enterDevMode();
    if (options.levelCreator) enterLevelMode(true);
}

void Game::enterDevMode()
{
    if (levelController_.open() && !levelController_.leave(*this)) { status = levelController_.message(); return; }
    smithOpen_ = crossingMenuOpen_ = false;
    rootCrossingIndex_ = -1;
    endSectionLoot();
    devMode_ = true;
    sectionEditor_.enter(*this);
}

void Game::leaveDevMode()
{
    if (!sectionEditor_.leave(*this)) { status = sectionEditor_.message(); return; }
    devMode_ = false;
    buildVillage();
    if (!returnToVillage()) throw std::runtime_error("Village spawn has no room for the player");
    status = "DRAFT SAVED | F7 SECTION BUILDER | F6 ROOT CROSSING TESTS";
    target.reset();
}

void Game::enterLevelMode(bool creator)
{
    if (devMode_) {
        if (!sectionEditor_.leave(*this)) { status = sectionEditor_.message(); return; }
        devMode_ = false;
    }
    if (levelController_.open() && !levelController_.leave(*this)) { status = levelController_.message(); return; }
    // The menu always sits over the village, never over an abandoned test room.
    buildVillage();
    if (!returnToVillage()) throw std::runtime_error("Village spawn has no room for the player");
    rootCrossingIndex_ = -1;
    smithOpen_ = crossingMenuOpen_ = false;
    levelController_.enter(*this, creator);
}

void Game::leaveLevelMode()
{
    if (!levelController_.leave(*this)) { status = levelController_.message(); return; }
    buildVillage();
    if (!returnToVillage()) throw std::runtime_error("Village spawn has no room for the player");
    status = "RETURNED TO GLOWROOT HAVEN | F4 LEVELS | F11 LEVEL CREATOR";
    target.reset();
}

bool Game::saveAuthoringDraft()
{
    if (devMode_) return !sectionEditor_.dirty() || sectionEditor_.saveDraft(*this);
    return !levelController_.dirty() || levelController_.saveDraft();
}

void Game::update(const InputActions& actions, float deltaSeconds)
{
    if (updateInventory(actions)) return;
    if (!editorMode_ && actions.active && (actions.levelCreator || actions.levelMenu
        || (actions.devClick && *actions.devClick == SectionUiLevelCreator))) {
        if (actions.levelMenu && !levelController_.creator()
            && (levelController_.screen() == LevelScreen::Worlds || levelController_.screen() == LevelScreen::Library)) leaveLevelMode();
        else if (actions.levelCreator && levelCreatorActive()) leaveLevelMode();
        else enterLevelMode(!actions.levelMenu);
        return;
    }
    if (!editorMode_ && actions.active && actions.devToggle) {
        if (devMode_) leaveDevMode();
        else enterDevMode();
        return;
    }
    if (levelController_.open()) {
        levelController_.update(*this, actions, deltaSeconds);
        return;
    }
    if (devMode_) {
        const bool worldBack = sectionEditor_.screen() == SectionEditorScreen::Worlds
            && (actions.closeMenu || (actions.devClick && *actions.devClick == SectionUiBack));
        if ((actions.active && actions.returnToHub) || worldBack) leaveDevMode();
        else sectionEditor_.update(*this, actions, deltaSeconds);
        return;
    }
    if (!editorMode_) {
        updatePlay(actions, deltaSeconds);
        return;
    }
    if (actions.selectedBlock && isRenderable(*actions.selectedBlock) && *actions.selectedBlock != Block::Bedrock) {
        selectedBlock = *actions.selectedBlock;
    } else if (actions.materialStep != 0) {
        const auto current = std::find(PlaceableBlocks.begin(), PlaceableBlocks.end(), selectedBlock);
        const int count = static_cast<int>(PlaceableBlocks.size());
        const int index = current == PlaceableBlocks.end() ? 0
            : static_cast<int>(current - PlaceableBlocks.begin());
        const int step = actions.materialStep > 0 ? 1 : -1;
        selectedBlock = PlaceableBlocks[(index + step + count) % count];
    }
    handleFiles(actions);
    handleUndoRedo(actions);
    findTarget();
    editTarget(actions);
}

void Game::buildVillage()
{
    endSectionLoot();
    buildUndergroundScene(world);
    auto collision = std::make_shared<AssetCollision>();
    collision->rebuild(planHubAssets(world));
    player.setAssetCollision(std::move(collision));
}

bool Game::returnToVillage()
{
    smithOpen_ = false;
    // Search only inside the cavern. The surface spawn helper would put us
    // above its roof. Nearby supported spaces also handle an obstructed arrival.
    Player destination = player;
    float nearest = std::numeric_limits<float>::max();
    for (int z = static_cast<int>(std::floor(VillageSpawn.z)) - 6;
         z <= static_cast<int>(std::floor(VillageSpawn.z)) + 6; ++z) {
        for (int x = static_cast<int>(std::floor(VillageSpawn.x)) - 6;
             x <= static_cast<int>(std::floor(VillageSpawn.x)) + 6; ++x) {
            for (int y = 1; y < World::MaxExclusive.y - Player::Height; ++y) {
                const glm::vec3 feet(x + 0.5f, static_cast<float>(y), z + 0.5f);
                const auto horizontal = glm::vec2(feet.x - VillageSpawn.x, feet.z - VillageSpawn.z);
                if (glm::dot(horizontal, horizontal) >= nearest) continue;
                Player candidate = player;
                // Paving and other models sit slightly above the voxel floor.
                // Settle onto their real top instead of spawning inside them.
                if (!candidate.setFeetPosition(world, feet + glm::vec3(0, 1, 0))) continue;
                candidate.move(world, {0, -1, 0});
                const auto offset = candidate.feetPosition() - VillageSpawn;
                const float distance = glm::dot(offset, offset);
                if (candidate.grounded() && distance < nearest) {
                    destination = candidate;
                    nearest = distance;
                }
            }
        }
    }
    if (nearest == std::numeric_limits<float>::max()) return false;
    player = destination;
    camera.reset();
    const auto facing = camera.forward();
    camera.rotate(VillageYaw - glm::degrees(std::atan2(facing.z, facing.x)),
                  VillagePitch - glm::degrees(std::asin(facing.y)));
    camera.setPosition(player.eyePosition());
    return true;
}

void Game::updatePlay(const InputActions& actions, float deltaSeconds)
{
    if (actions.closeMenu) { smithOpen_ = crossingMenuOpen_ = false; return; }
    if (!actions.active) return;
    if (actions.crossingMenu) {
        if (crossingMenuOpen_) crossingMenuOpen_ = false;
        else openCrossingMenu();
        return;
    }
    if (actions.returnToHub && rootCrossingActive()) {
        leaveRootCrossing();
        return;
    }
    if (crossingMenuOpen_) {
        if (actions.selectedCrossing && *actions.selectedCrossing >= 0
            && *actions.selectedCrossing < RootCrossingCount) {
            crossingMenuSelection_ = *actions.selectedCrossing;
        } else if (actions.crossingMenuStep != 0) {
            crossingMenuSelection_ = (crossingMenuSelection_
                + (actions.crossingMenuStep > 0 ? 1 : RootCrossingCount - 1)) % RootCrossingCount;
        }
        if (actions.upgrade) startRootCrossing(crossingMenuSelection_);
        return; // The picker never moves the player or selects equipment.
    }
    if (rootCrossingActive()) {
        if (actions.crossingStep != 0) {
            startRootCrossing((rootCrossingIndex_
                + (actions.crossingStep > 0 ? 1 : RootCrossingCount - 1)) % RootCrossingCount);
            return;
        }
        if (actions.restartCrossing || actions.respawn) {
            resetRootCrossing(actions.restartCrossing);
            return;
        }
        if (player.feetPosition().y < crossingCourse_.resetY) {
            resetRootCrossing(false, true);
            return;
        }
    } else if (actions.respawn || player.feetPosition().y < -16.0f) {
        status = returnToVillage() ? "Returned to the village"
                                   : "No safe space at the village entrance";
        return;
    }
    if (smithOpen_ && !nearBlacksmith()) {
        smithOpen_ = false;
        status = "Blacksmith is out of reach";
    }
    if (smithOpen_) {
        if (actions.interact) smithOpen_ = false;
        else if (actions.upgrade) upgradeEquipment();
        return;
    }
    if (actions.selectedTool && *actions.selectedTool >= 0 && *actions.selectedTool < ToolCount)
        equipment.selectedSlot = *actions.selectedTool;
    else if (actions.toolStep != 0)
        equipment.selectedSlot = ((equipment.selectedSlot + actions.toolStep % ToolCount) % ToolCount + ToolCount) % ToolCount;
    if (actions.interact && nearExpeditionGate()) {
        enterLevelMode(false);
        return;
    }
    if (actions.interact && nearBlacksmith()) {
        smithOpen_ = true;
        status = "Spend ore from your satchel to strengthen your pickaxe";
        return;
    }
    player.update(world, actions.movement, camera.forward(), actions.jump, actions.sprint, deltaSeconds, actions.descend);
    camera.setPosition(player.eyePosition());
    if (rootCrossingActive()) updateRootCrossing(deltaSeconds);
}

void Game::openCrossingMenu()
{
    smithOpen_ = false;
    crossingMenuOpen_ = true;
    if (rootCrossingActive()) crossingMenuSelection_ = rootCrossingIndex_;
}

void Game::positionForCrossing(glm::vec3 feet, bool checkpoint)
{
    if (!player.setFeetPosition(world, feet) || !player.grounded())
        throw std::runtime_error("Root Crossing landing has no supported space for the player");
    float yaw = crossingCourse_.spawnYaw;
    if (checkpoint) {
        // A winding course may have turned since its entrance. Point a reset
        // toward the next landing so the player can immediately continue.
        auto direction = crossingCourse_.finish - feet;
        const auto waypoint = std::find(crossingCourse_.route.begin(), crossingCourse_.route.end(),
                                        crossingCourse_.checkpoint);
        if (waypoint != crossingCourse_.route.end() && waypoint + 1 != crossingCourse_.route.end())
            direction = *(waypoint + 1) - feet;
        if (std::hypot(direction.x, direction.z) > .001f)
            yaw = glm::degrees(std::atan2(direction.z, direction.x));
    }
    camera.reset();
    const auto facing = camera.forward();
    camera.rotate(yaw - glm::degrees(std::atan2(facing.z, facing.x)),
                  -8.0f - glm::degrees(std::asin(facing.y)));
    camera.setPosition(player.eyePosition());
}

void Game::startRootCrossing(int index)
{
    if (index < 0 || index >= RootCrossingCount) return;
    endSectionLoot();
    crossingCourse_ = buildRootCrossing(world, index);
    player.setAssetCollision(nullptr);
    rootCrossingIndex_ = crossingMenuSelection_ = index;
    smithOpen_ = crossingMenuOpen_ = false;
    crossingCompleted_ = crossingCheckpointReached_ = false;
    crossingElapsed_ = 0.0f;
    crossingFalls_ = 0;
    target.reset();
    positionForCrossing(crossingCourse_.spawn);
    status = std::string(rootCrossingCatalog()[index].name) + " | REACH THE GOLD FINISH MARKER";
}

void Game::leaveRootCrossing()
{
    buildVillage();
    rootCrossingIndex_ = -1;
    crossingCompleted_ = crossingCheckpointReached_ = crossingMenuOpen_ = false;
    crossingElapsed_ = 0.0f;
    crossingFalls_ = 0;
    if (!returnToVillage()) throw std::runtime_error("Village spawn has no room for the player");
    status = "RETURNED TO GLOWROOT HAVEN | F6 ROOT CROSSING TESTS";
}

void Game::resetRootCrossing(bool fullRestart, bool fallen)
{
    if (fullRestart) {
        crossingCheckpointReached_ = false;
        crossingElapsed_ = 0.0f;
        crossingFalls_ = 0;
    } else if (fallen) {
        ++crossingFalls_;
    }
    crossingCompleted_ = false;
    positionForCrossing(crossingCheckpointReached_ ? crossingCourse_.checkpoint : crossingCourse_.spawn,
                        crossingCheckpointReached_);
    status = fullRestart ? "NEW ATTEMPT | REACH THE GOLD FINISH MARKER"
        : crossingCheckpointReached_ ? "RETURNED TO CHECKPOINT | SHIFT+R STARTS A NEW ATTEMPT"
                                     : "RETURNED TO START | SHIFT+R STARTS A NEW ATTEMPT";
}

void Game::updateRootCrossing(float deltaSeconds)
{
    if (crossingCompleted_) return;
    // Match the physics step cap. Pauses and long background frames cannot
    // inflate a run while the player is not actually being simulated.
    if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0f)
        crossingElapsed_ += std::min(deltaSeconds, 0.1f);
    if (player.feetPosition().y < crossingCourse_.resetY) {
        resetRootCrossing(false, true);
        return;
    }
    const auto onLanding = [&](glm::vec3 landing, float radius) {
        const auto offset = player.feetPosition() - landing;
        return player.grounded() && std::abs(offset.y) < 0.75f
            && offset.x * offset.x + offset.z * offset.z < radius * radius;
    };
    if (!crossingCheckpointReached_ && onLanding(crossingCourse_.checkpoint, 2.75f)) {
        crossingCheckpointReached_ = true;
        status = "CHECKPOINT REACHED | R RETURNS HERE | SHIFT+R RESTARTS";
    }
    if (onLanding(crossingCourse_.finish, 2.0f)) {
        crossingCompleted_ = true;
        auto& best = crossingBestTimes_[rootCrossingIndex_];
        if (crossingElapsed_ > 0.0f && (best == 0.0f || crossingElapsed_ < best)) best = crossingElapsed_;
        status = "CROSSING COMPLETE | N NEXT | P PREVIOUS | SHIFT+R RETRY | B HUB";
    }
}

float Game::crossingBestTime() const
{
    return rootCrossingActive() ? crossingBestTimes_[rootCrossingIndex_] : 0.0f;
}

bool Game::nearExpeditionGate() const
{
    return !editorMode_ && !devMode_ && !levelController_.open() && !rootCrossingActive()
        && glm::distance(player.feetPosition(), ExpeditionDeparture) <= 8.0f;
}

glm::vec3 Game::compassTarget() const
{
    if (devMode_ && sectionEditor_.hasSection()) return sectionEditor_.meta().finish;
    if (levelActive()) return levelController_.meta().finish;
    return rootCrossingActive() ? crossingCourse_.finish : HeartTreeCenter;
}

bool Game::nearBlacksmith() const
{
    if (editorMode_ || devMode_ || levelController_.open() || rootCrossingActive()) return false;
    const auto station = sceneBlacksmithStation();
    const auto support = glm::ivec3(glm::floor(station)) - glm::ivec3(0, 1, 0);
    if (!isSolid(world.get(support)) || !isSolid(world.get(support - glm::ivec3(0, 1, 0)))) return false;
    const auto delta = station - player.eyePosition();
    const float distance = glm::length(delta);
    if (distance > 4.0f || glm::distance(player.feetPosition(), sceneBlacksmithApproach()) > 4.0f) return false;
    if (distance < .001f) return true;
    const auto obstruction = raycast(player.eyePosition(), delta, distance,
        [this](glm::ivec3 position) { return world.get(position); });
    return !obstruction || obstruction->distance >= distance - .001f;
}

void Game::upgradeEquipment()
{
    if (!progressError_.empty()) { status = progressError_; return; }
    auto upgraded = equipment;
    switch (upgradePickaxe(upgraded)) {
    case UpgradeResult::MaxTier: status = "Pickaxe is already tier 3"; return;
    case UpgradeResult::InsufficientResources: status = "Not enough ore for this upgrade"; return;
    case UpgradeResult::Upgraded: break;
    }
    try {
        // Commit the profile before charging the in-memory inventory. A failed
        // write leaves both the old equipment and existing save intact.
        if (!progressPath_.empty()) saveEquipment(upgraded, progressPath_);
        equipment = upgraded;
        status = "Pickaxe upgraded to tier " + std::to_string(equipment.pickaxeTier);
    } catch (const std::exception& error) {
        status = std::string("Upgrade not saved: ") + error.what();
    }
}

void Game::handleFiles(const InputActions& actions)
{
    if (!actions.save && !actions.load) return;
    try {
        if (actions.save) {
            saveWorld(world, savePath_);
            status = "Saved " + savePath_;
        } else {
            // loadWorld builds a new world first. Failure leaves this one intact.
            world = loadWorld(savePath_);
            history.clear();
            status = "Loaded " + savePath_;
        }
        savedWorld_ = world;
        std::cout << status << '\n';
    } catch (const std::exception& error) {
        status = std::string("File error: ") + error.what();
        std::cerr << status << '\n';
    }
}

void Game::handleUndoRedo(const InputActions& actions)
{
    if (!actions.undo && !actions.redo) return;
    bool changed;
    if (actions.undo) {
        changed = history.undo(world);
        status = changed ? "Undid edit" : "No edit available";
    } else {
        changed = history.redo(world);
        status = changed ? "Redid edit" : "No edit available";
    }
}

void Game::findTarget()
{
    target = raycast(camera.position(), camera.forward(), static_cast<float>(editReach_),
        [this](glm::ivec3 position) { return world.get(position); });
}

void Game::editTarget(const InputActions& actions)
{
    if (!actions.pick && !actions.remove && !actions.place) return;
    if (!target) {
        status = "No block within editing reach";
        return;
    }
    const auto targetMaterial = world.get(target->position);
    if (actions.pick && targetMaterial != Block::Bedrock) {
        selectedBlock = targetMaterial;
        status = "Selected " + std::string(blockName(selectedBlock));
    }

    bool changed = false;
    if (actions.remove) {
        changed = removeBlock(world, *target, &history);
        if (changed) status = "Removed " + std::string(blockName(targetMaterial));
    } else if (actions.place) {
        const auto placement = checkPlacement(world, *target, selectedBlock, camera.position());
        switch (placement.failure) {
        case PlacementFailure::None: break;
        case PlacementFailure::OutsideWorld:
            status = "Cannot place: outside world bounds";
            return;
        case PlacementFailure::Occupied:
            status = "Cannot place: cell is occupied";
            return;
        case PlacementFailure::CameraOverlap:
            status = "Cannot place: move out of the destination cell";
            return;
        case PlacementFailure::InvalidFace:
            status = "Cannot place: aim at a block face from outside it";
            return;
        case PlacementFailure::InvalidMaterial:
            status = "Cannot place: select a solid material";
            return;
        case PlacementFailure::MissingTarget:
            status = "Cannot place: target is no longer solid";
            return;
        case PlacementFailure::InvalidCamera:
            status = "Cannot place: reset the camera with R";
            return;
        }
        changed = placeBlock(world, *target, selectedBlock, camera.position(), &history);
        if (changed) status = "Placed " + std::string(blockName(selectedBlock));
    }
    if (changed) findTarget();
}

bool Game::hasUnsavedChanges() const
{
    // Compare contents, not history size: undo can return exactly to the last save.
    return devMode_ ? sectionEditor_.dirty() : levelController_.open() ? levelController_.dirty()
        : editorMode_ && !world.sameContents(savedWorld_);
}

std::string Game::playStatusText(bool captured, bool showHelp) const
{
    std::ostringstream text;
    if (rootCrossingActive()) {
        text << "ROOT CROSSING " << rootCrossingIndex_ + 1 << "/" << RootCrossingCount
             << " | " << rootCrossingCatalog()[rootCrossingIndex_].name;
    } else text << "GLOWROOT HAVEN | VILLAGE HUB";
    if (!captured) text << "\nPAUSED | TAB TO RESUME";
    if (!crossingMenuOpen_) text << "\n" << status;
    text << "\nH CONTROLS | F4 LEVELS | F7 SECTION BUILDER | F11 LEVEL CREATOR";
    if (showHelp) {
        text << "\nWASD WALK | SPACE JUMP | SHIFT SPRINT | MOUSE LOOK"
             << "\nWATER: HOLD SPACE RISE | CTRL DIVE | SHIFT SWIM FASTER"
             << "\nSCROLL / [ ] CHANGE TOOL | TAB RESUME"
             << "\n1 PICKAXE | 2 LANTERN | 3 COMPASS"
             << "\nI INVENTORY | E OPEN CHEST | ENTER TAKE ALL LOOT"
             << "\nF6 ROOT CROSSING TESTS | F4 EXPEDITIONS | F11 LEVEL CREATOR";
        if (rootCrossingActive()) {
            text << "\nN NEXT TEST | P PREVIOUS | B RETURN TO HUB"
                 << "\nR CHECKPOINT | SHIFT+R RESTART ATTEMPT"
                 << "\nCOMPASS POINTS TO THE FINISH | FALLS RESET TO CHECKPOINT"
                 << "\nESC PAUSE | Q QUIT | TESTS DO NOT AWARD ORE";
        } else {
            text << "\nE BLACKSMITH / EXPEDITION GATE | ENTER CONFIRM | ESC CLOSE / PAUSE"
                 << "\nR RETURN TO VILLAGE | Q QUIT"
                 << "\nFOUR BRIDGES JOIN THE HEART TREE AND WATERFRONT"
             << "\nTERRAIN RESETS ON LAUNCH | INVENTORY AND UPGRADES ARE SAVED"
                 << "\nMINING AND FULL EXPEDITIONS COMING LATER";
        }
    }
    return text.str();
}

std::string Game::statusText(std::size_t visibleChunks, std::size_t drawnTriangles,
                             std::size_t totalTriangles, double fps,
                             bool captured, bool showHelp) const
{
    if (levelController_.open()) {
        std::ostringstream text;
        text << (levelCreatorActive() ? "LEVEL CREATOR" : "EXPEDITIONS") << " | " << levelController_.message();
        if (!captured && !levelMenuOpen()) text << "\nPAUSED | TAB TO RESUME";
        if (showHelp) text << "\nWASD WALK | SPACE JUMP | SHIFT SPRINT | R CHECKPOINT | SHIFT+R RESTART LEVEL | B HUB"
            << "\nWATER: HOLD SPACE RISE | CTRL DIVE | SHIFT SWIM FASTER";
        return text.str();
    }
    if (devMode_) {
        std::ostringstream text;
        text << "SECTION BUILDER | " << sectionEditor_.message();
        if (!captured && !devMenuOpen()) text << "\nPAUSED | TAB TO RESUME";
        if (showHelp) {
            if (sectionEditor_.screen() == SectionEditorScreen::Test) text << "\nWASD WALK | SPACE JUMP | SHIFT SPRINT | F8 RETURN TO BUILD"
                << "\nWATER: HOLD SPACE RISE | CTRL DIVE | SHIFT SWIM FASTER"
                << "\nR CHECKPOINT | SHIFT+R RESTART TEST | B HUB";
            else text << "\nE PALETTE | F8 WALK TEST | F10 ACCEPT | F5 DRAFT | F9 LIBRARY"
                << "\n1-9 HOTBAR | LEFT REMOVE | RIGHT PLACE | MIDDLE PICK | T ROTATE | Y VARIANT"
                << "\nWASD FLY | SPACE UP | CTRL DOWN | SHIFT BOOST | CTRL/CMD Z UNDO | B HUB";
        }
        return text.str();
    }
    if (!editorMode_) return playStatusText(captured, showHelp);
    const auto position = camera.position();
    std::ostringstream text;
    text << std::fixed << std::setprecision(1)
         << "VOXEL ENGINE | " << blockName(selectedBlock) << " | SEED " << world.seed();
    if (hasUnsavedChanges()) text << " | UNSAVED";
    text << "\nCHUNKS " << visibleChunks << "/" << World::ChunkCount
         << " | TRIANGLES " << drawnTriangles << "/" << totalTriangles;
    if (fps > 0.0) text << " | FPS " << fps << " | " << 1000.0 / fps << " MS";
    text << "\nXYZ " << position.x << ", " << position.y << ", " << position.z
         << " | FOV " << camera.fieldOfView() << " | UNDO " << history.undoCount()
         << " REDO " << history.redoCount();
    if (!captured) {
        text << "\nTARGET PAUSED | TAB TO CAPTURE MOUSE";
    } else if (target) {
        text << "\nTARGET " << blockName(world.get(target->position))
             << " | XYZ " << target->position.x << ", " << target->position.y << ", " << target->position.z
             << " | DIST " << target->distance << "/" << editReach_ << " BLOCKS";
    } else {
        text << "\nTARGET NONE | REACH " << editReach_ << " BLOCKS";
    }
    text << "\n" << status << "\nH HELP | F5 SAVE | F9 RELOAD | TAB MOUSE | Q QUIT";
    if (!captured) text << " | MOUSE RELEASED";
    if (showHelp) {
        text << "\nWASD FLY | SPACE/CTRL UP/DOWN"
             << "\nSHIFT BOOST | ALT PRECISION"
             << "\nMOUSE LOOK | SCROLL ZOOM | R RESET | F WIREFRAME"
             << "\nLEFT REMOVE | RIGHT PLACE | MIDDLE PICK"
             << "\n1-9 MATERIAL | [ PREVIOUS | ] NEXT"
             << "\nCTRL/CMD Z UNDO | CTRL/CMD Y OR SHIFT Z REDO"
             << "\nESC RELEASE MOUSE | EDIT REACH " << editReach_ << " BLOCKS";
    }
    return text.str();
}

} // namespace voxel
