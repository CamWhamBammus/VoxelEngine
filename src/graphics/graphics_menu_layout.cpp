#include "graphics/graphics_menu_layout.h"
#include "pause_menu.h"

#include <algorithm>
#include <cmath>

namespace voxel {

bool GraphicsMenuRect::contains(float px, float py) const
{
    return px >= x && px < x + width && py >= y && py < y + height;
}

GraphicsMenuRect GraphicsMenuLayout::row(int index) const
{
    return {panel.x + 28, panel.y + 100 + index * 43, panel.width - 56, 36};
}

std::optional<int> GraphicsMenuLayout::hit(float physicalX, float physicalY) const
{
    if (scale <= 0 || !std::isfinite(physicalX) || !std::isfinite(physicalY)) return std::nullopt;
    for (const auto& control : controls)
        if (control.rect.contains(physicalX / scale, physicalY / scale)) return control.action;
    return std::nullopt;
}

GraphicsMenuLayout pauseMenuLayout(PauseScreen screen, int framebufferWidth,
                                   int framebufferHeight, float contentScale)
{
    GraphicsMenuLayout result;
    if (screen == PauseScreen::Closed || framebufferWidth <= 0 || framebufferHeight <= 0) return result;
    if (!std::isfinite(contentScale) || contentScale <= 0) contentScale = 1;
    const bool graphics = screen == PauseScreen::Graphics;
    const float panelWidth = graphics ? 660.f : 420.f;
    const float panelHeight = graphics ? 632.f : 326.f;
    result.scale = std::min({std::clamp(contentScale, .5f, 4.f),
                            framebufferWidth / (panelWidth + 32),
                            framebufferHeight / (panelHeight + 32)});
    result.width = framebufferWidth / result.scale;
    result.height = framebufferHeight / result.scale;
    result.panel = {(result.width - panelWidth) * .5f, (result.height - panelHeight) * .5f,
                    panelWidth, panelHeight};
    const float x = result.panel.x, y = result.panel.y;
    if (!graphics) {
        const int actions[] = {PauseUiResume, PauseUiGraphics, PauseUiQuit};
        for (int index = 0; index < 3; ++index)
            result.controls.push_back({{x + 38, y + 112 + index * 54.f, panelWidth - 76, 40},
                                       actions[index], index});
        return result;
    }
    for (int index = 0; index < 8; ++index) {
        const auto row = result.row(index);
        // Labels only select; arrow/value clicks change one step. Nonoverlapping
        // targets avoid double changes at control boundaries.
        result.controls.push_back({{row.x, row.y, 284, row.height}, PauseUiRowBase + index, index});
        result.controls.push_back({{row.x + 284, row.y, 36, row.height}, PauseUiDecreaseBase + index, index});
        result.controls.push_back({{row.x + 320, row.y, 248, row.height}, PauseUiIncreaseBase + index, index});
        result.controls.push_back({{row.x + 568, row.y, 36, row.height}, PauseUiIncreaseBase + index, index});
    }
    result.controls.push_back({{x + 28, y + 552, 204, 40}, PauseUiDefaults, 8});
    result.controls.push_back({{x + 310, y + 552, 132, 40}, PauseUiBack, 9});
    result.controls.push_back({{x + 454, y + 552, 178, 40}, PauseUiApply, 10});
    return result;
}

} // namespace voxel
