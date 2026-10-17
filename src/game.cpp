#include "game.h"
#include "input.h"
#include "save.h"

#include <algorithm>
#include <charconv>
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
        selectedBlock = Block::Dirt;
        if (!player.spawn(world)) throw std::runtime_error("Could not find safe ground for the player");
        camera.setPosition(player.eyePosition());
        status = "WASD walk | SPACE jump | SHIFT sprint";
    }
    findTarget();
}

void Game::update(const InputActions& actions, float deltaSeconds)
{
    if (!editorMode_) {
        if (!actions.active) return;
        if (actions.respawn || player.feetPosition().y < -16.0f) {
            if (player.spawn(world)) status = "Returned to safe ground";
        } else {
            player.update(world, actions.movement, camera.forward(), actions.jump, actions.sprint, deltaSeconds);
        }
        camera.setPosition(player.eyePosition());
    }
    if (actions.selectedBlock && isSolid(*actions.selectedBlock)) {
        selectedBlock = *actions.selectedBlock;
    } else if (actions.materialStep != 0) {
        const auto current = std::find(PlaceableBlocks.begin(), PlaceableBlocks.end(), selectedBlock);
        const int count = static_cast<int>(PlaceableBlocks.size());
        const int index = current == PlaceableBlocks.end() ? 0
            : static_cast<int>(current - PlaceableBlocks.begin());
        const int step = actions.materialStep > 0 ? 1 : -1;
        selectedBlock = PlaceableBlocks[(index + step + count) % count];
    }
    if (editorMode_) {
        handleFiles(actions);
        handleUndoRedo(actions);
    } else if (actions.save || actions.load || actions.undo || actions.redo) {
        status = "Save and undo are editor tools. Launch with --editor to use them";
    }
    findTarget();
    editTarget(actions);
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
    if (actions.pick) {
        selectedBlock = targetMaterial;
        status = "Selected " + std::string(blockName(selectedBlock));
    }

    bool changed = false;
    if (actions.remove) {
        changed = removeBlock(world, *target, editorMode_ ? &history : nullptr);
        if (changed) {
            if (!editorMode_) ++inventory_[static_cast<std::size_t>(targetMaterial) - 1];
            status = std::string(editorMode_ ? "Removed " : "Mined ") + std::string(blockName(targetMaterial));
        }
    } else if (actions.place) {
        if (!editorMode_ && blockCount(selectedBlock) == 0) {
            status = "No " + std::string(blockName(selectedBlock)) + " blocks - mine some first";
            return;
        }
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
        if (!editorMode_ && player.overlapsBlock(placement.position)) {
            status = "Cannot place: your body is in the destination cell";
            return;
        }
        changed = placeBlock(world, *target, selectedBlock, camera.position(), editorMode_ ? &history : nullptr);
        if (changed) {
            if (!editorMode_) --inventory_[static_cast<std::size_t>(selectedBlock) - 1];
            status = "Placed " + std::string(blockName(selectedBlock));
        }
    }
    if (changed) findTarget();
}

bool Game::hasUnsavedChanges() const
{
    // Compare contents, not history size: undo can return exactly to the last save.
    return editorMode_ && !world.sameContents(savedWorld_);
}

int Game::blockCount(Block block) const
{
    return isSolid(block) ? inventory_[static_cast<std::size_t>(block) - 1] : 0;
}

std::string Game::statusText(std::size_t visibleChunks, std::size_t drawnTriangles,
                             std::size_t totalTriangles, double fps,
                             bool captured, bool showHelp) const
{
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
             << "\n1-4 MATERIAL | [ PREVIOUS | ] NEXT"
             << "\nCTRL/CMD Z UNDO | CTRL/CMD Y OR SHIFT Z REDO"
             << "\nESC RELEASE MOUSE | EDIT REACH " << editReach_ << " BLOCKS";
    }
    return text.str();
}

} // namespace voxel
