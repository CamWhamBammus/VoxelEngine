#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace voxel {

inline constexpr int SkillUiTabBase = 0;
inline constexpr int SkillUiEntryBase = 100;
inline constexpr int SkillUiClose = 200;

struct JournalRect {
    float x, y, width, height;
    bool contains(float px, float py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

// Every page, card and pointer target uses this one logical book coordinate
// system. A small window scales the entire spread without hiding a tab.
struct JournalLayout {
    float scale = 1;
    float width = 1080, height = 660;
    float x = 0, y = 0;
    JournalRect book() const { return {x + 28, y + 76, 1024, 536}; }
    JournalRect tab(int index) const { return {x + 137 + index * 204, y + 28, 199, 62}; }
    JournalRect card(int index) const {
        return {x + 83 + (index % 3) * 143, y + 189 + (index / 3) * 182, 135, 174};
    }
    JournalRect close() const { return {x + 466, y + 624, 148, 30}; }
    std::optional<int> hit(float px, float py) const {
        if (!std::isfinite(px) || !std::isfinite(py) || scale <= 0) return std::nullopt;
        px /= scale;
        py /= scale;
        for (int index = 0; index < 4; ++index)
            if (tab(index).contains(px, py)) return SkillUiTabBase + index;
        for (int index = 0; index < 6; ++index)
            if (card(index).contains(px, py)) return SkillUiEntryBase + index;
        if (close().contains(px, py)) return SkillUiClose;
        return std::nullopt;
    }
};

inline JournalLayout journalLayout(int framebufferWidth, int framebufferHeight, float contentScale)
{
    if (!std::isfinite(contentScale) || contentScale <= 0) contentScale = 1;
    JournalLayout result;
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        result.scale = 0;
        return result;
    }
    result.scale = std::min({std::clamp(contentScale, .5f, 4.f),
                            framebufferWidth / 1080.f, framebufferHeight / 660.f});
    result.width = framebufferWidth / result.scale;
    result.height = framebufferHeight / result.scale;
    result.x = (result.width - 1080) * .5f;
    result.y = (result.height - 660) * .5f;
    return result;
}

} // namespace voxel
