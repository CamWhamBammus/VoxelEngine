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
    int editReach = 8;
    bool seedExplicit = false;
    bool smokeTest = false;
    bool help = false;
    bool worldExplicit = false;
    std::string screenshot;
    std::string worldPath = "world.vxl";
};

inline constexpr const char* Usage =
    "Usage: VoxelEngine [options]\n\n"
    "  --seed N           Generate terrain from an unsigned 32-bit seed (default: 1337).\n"
    "  --world path.vxl   Choose the save file (default: world.vxl).\n"
    "  --reach N          Editing distance in blocks, 1 to 64 (default: 8).\n"
    "  --smoke-test       Check rendering in a hidden window, then exit.\n"
    "  --screenshot path.ppm\n"
    "                     Run the smoke test and write a rendered image.\n"
    "  -h, --help         Show this help without opening a window.\n\n"
    "Startup loads an existing world.vxl unless --seed is given.\n"
    "An explicit existing --world file takes precedence over --seed.\n"
    "Smoke tests only load files explicitly selected with --world.\n"
    "Press H for controls, F5 to save, F9 to reload, and Q to quit.\n"
    "Changes are not saved automatically on exit.\n\n"
    "Examples:\n"
    "  VoxelEngine --seed 42\n"
    "  VoxelEngine --world my-world.vxl\n"
    "  VoxelEngine --seed 42 --screenshot terrain.ppm\n";
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
    int editReach_;
};

} // namespace voxel
