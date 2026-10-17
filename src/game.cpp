#include "game.h"
#include "input.h"
#include "save.h"

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
    return options;
}

// An explicit --seed starts fresh unless an explicit existing world file was requested.
Game::Game(const Options& options)
    : world(options.seed), savedWorld_(world), savePath_(options.worldPath), editReach_(options.editReach)
{
    if (editReach_ < 1 || editReach_ > 64) {
        throw std::invalid_argument("Editing reach must be an integer from 1 to 64");
    }
    const bool loadExisting = (options.worldExplicit || (!options.smokeTest && !options.seedExplicit))
        && std::filesystem::exists(savePath_);
    if (loadExisting) {
        world = loadWorld(savePath_);
        savedWorld_ = world;
        status = "Loaded " + savePath_;
    } else {
        status = "New world - F5 to save";
    }
    findTarget();
}

void Game::update(const InputActions& actions)
{
    if (actions.selectedBlock && isSolid(*actions.selectedBlock)) {
        selectedBlock = *actions.selectedBlock;
    }
    handleFiles(actions);
    handleUndoRedo(actions);
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
    if (!target) return;
    if (actions.pick) selectedBlock = world.get(target->position);

    bool changed = false;
    if (actions.remove) {
        changed = removeBlock(world, *target, &history);
    } else if (actions.place) {
        changed = placeBlock(world, *target, selectedBlock, camera.position(), &history);
    }
    if (changed) findTarget();
}

bool Game::hasUnsavedChanges() const
{
    // Compare contents, not history size: undo can return exactly to the last save.
    return !world.sameContents(savedWorld_);
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
         << " REDO " << history.redoCount()
         << "\n" << status << "\nH HELP | F5 SAVE | F9 RELOAD | TAB MOUSE | Q QUIT";
    if (!captured) text << " | MOUSE RELEASED";
    if (showHelp) {
        text << "\nWASD FLY | SPACE/CTRL UP/DOWN | SHIFT BOOST"
             << "\nMOUSE LOOK | SCROLL ZOOM | R RESET | F WIREFRAME"
             << "\nLEFT REMOVE | RIGHT PLACE | MIDDLE PICK | 1-4 MATERIAL"
             << "\nCTRL/CMD Z UNDO | CTRL/CMD Y OR SHIFT Z REDO"
             << "\nESC RELEASE MOUSE | EDIT REACH " << editReach_ << " BLOCKS";
    }
    return text.str();
}

} // namespace voxel
