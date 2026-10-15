#pragma once

#include "camera.h"
#include "editing.h"
#include "world.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace voxel {

// Startup settings. The command line is read once, before opening the window.
struct Options {
    std::uint32_t seed = 1337;
    bool seedExplicit = false;
    bool smokeTest = false;
    bool help = false;
    bool worldExplicit = false;
    std::string screenshot;
    std::string worldPath = "world.vxl";
};

inline constexpr const char* Usage =
    "Usage: VoxelEngine [--seed N] [--world path.vxl] [--smoke-test] [--screenshot path.ppm] [--help]\n";
Options parseOptions(std::span<const std::string_view> arguments);

struct InputActions;

// Everything the player is working with. OpenGL and keyboard polling live elsewhere.
class Game {
public:
    explicit Game(const Options& options);
    void update(const InputActions& actions);
    bool hasUnsavedChanges() const;
    std::string statusText(std::size_t visibleChunks, std::size_t drawnTriangles,
                           std::size_t totalTriangles, double fps,
                           bool captured, bool showHelp) const;

    World world;
    Camera camera;
    EditHistory history;
    Block selectedBlock = Block::Grass;
    std::optional<RayHit> target;
    std::string status;

private:
    void handleFiles(const InputActions& actions);
    void handleUndoRedo(const InputActions& actions);
    void findTarget();
    void editTarget(const InputActions& actions);

    World savedWorld_;
    std::string savePath_;
};

} // namespace voxel
