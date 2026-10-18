#include "game.h"
#include "input.h"
#include "save.h"
#include "scene.h"

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
        else if (argument == "--smoke-test") options.smokeTest = true;
        else if (argument == "--seed") {
            options.seedExplicit = true;
            const auto seed = value();
            const auto result = std::from_chars(seed.data(), seed.data() + seed.size(), options.seed);
            if (result.ec != std::errc{} || result.ptr != seed.data() + seed.size()) {
                throw std::invalid_argument("Seed must be an integer from 0 to 4294967295");
            }
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
    if (options.worldExplicit && !options.editor && !options.help) {
        throw std::invalid_argument("Use --editor with --world; game runs start with fresh terrain");
    }
    return options;
}

// An explicit --seed starts fresh unless an explicit existing world file was requested.
Game::Game(const Options& options)
    : world(options.seed), savedWorld_(world), savePath_(options.worldPath), editReach_(options.editReach),
      editorMode_(options.editor)
{
    if (editReach_ < 1 || editReach_ > 64) {
        throw std::invalid_argument("Editing reach must be an integer from 1 to 64");
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
        buildUndergroundScene(world);
        if (!returnToVillage()) throw std::runtime_error("Village spawn has no room for the player");
        status = "Explore Glowroot Haven: tree island, waterfront homes, and the north expedition gate";
    }
    if (editorMode_) findTarget();
}

void Game::update(const InputActions& actions, float deltaSeconds)
{
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

bool Game::returnToVillage()
{
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
                const auto offset = feet - VillageSpawn;
                const float distance = glm::dot(offset, offset);
                if (distance >= nearest) continue;
                Player candidate = player;
                if (candidate.setFeetPosition(world, feet) && candidate.grounded()) {
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
    if (!actions.active) return;
    if (actions.respawn || player.feetPosition().y < -16.0f) {
        status = returnToVillage() ? "Returned to the village"
                                   : "No safe space at the village entrance";
    } else {
        player.update(world, actions.movement, camera.forward(), actions.jump, actions.sprint, deltaSeconds);
        camera.setPosition(player.eyePosition());
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
    return editorMode_ && !world.sameContents(savedWorld_);
}

std::string Game::playStatusText(bool captured, bool showHelp) const
{
    std::ostringstream text;
    text << "GLOWROOT HAVEN | VILLAGE HUB";
    if (!captured) text << "\nPAUSED | TAB TO RESUME";
    text << "\n" << status
         << "\nH CONTROLS | R RETURN TO VILLAGE | ESC PAUSE | Q QUIT";
    if (showHelp) {
        text << "\nWASD WALK | SPACE JUMP | SHIFT SPRINT | MOUSE LOOK"
             << "\nSCROLL ZOOM | TAB RESUME"
             << "\nFOUR BRIDGES JOIN THE HEART TREE AND WATERFRONT"
             << "\nHUB RESETS ON LAUNCH | EXPEDITIONS AND BUILDING SERVICES COMING LATER";
    }
    return text.str();
}

std::string Game::statusText(std::size_t visibleChunks, std::size_t drawnTriangles,
                             std::size_t totalTriangles, double fps,
                             bool captured, bool showHelp) const
{
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
