#pragma once

#include <optional>
#include <vector>

namespace voxel {
enum class PauseScreen;

struct GraphicsMenuRect {
    float x = 0, y = 0, width = 0, height = 0;
    bool contains(float px, float py) const;
};

struct GraphicsMenuControl {
    GraphicsMenuRect rect;
    int action = 0;
    int selection = 0;
};

// Drawing and pointer targets use the same logical coordinates. Physical mouse
// coordinates are converted here exactly once, including Retina/small windows.
struct GraphicsMenuLayout {
    float scale = 0, width = 0, height = 0;
    GraphicsMenuRect panel;
    std::vector<GraphicsMenuControl> controls;
    GraphicsMenuRect row(int index) const;
    std::optional<int> hit(float physicalX, float physicalY) const;
};

GraphicsMenuLayout pauseMenuLayout(PauseScreen screen, int framebufferWidth,
                                  int framebufferHeight, float contentScale = 1);

} // namespace voxel
