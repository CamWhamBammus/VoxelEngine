#include "graphics_settings.h"

#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace voxel {

bool validGraphicsSettings(const GraphicsSettings& settings)
{
    return std::isfinite(settings.renderScale) && settings.renderScale >= 0.5f && settings.renderScale <= 1.0f
        && (settings.msaa == 0 || settings.msaa == 2 || settings.msaa == 4)
        && (settings.lighting == LightingQuality::Low || settings.lighting == LightingQuality::Medium
            || settings.lighting == LightingQuality::High)
        && (settings.detail == DetailDistance::Near || settings.detail == DetailDistance::Medium
            || settings.detail == DetailDistance::Far)
        && (settings.fpsLimit == 0 || settings.fpsLimit == 30 || settings.fpsLimit == 60
            || settings.fpsLimit == 90 || settings.fpsLimit == 120 || settings.fpsLimit == 144);
}

void applyGraphicsPreset(GraphicsSettings& settings, GraphicsPreset preset)
{
    switch (preset) {
    case GraphicsPreset::Low:
        settings.renderScale = 0.65f;
        settings.msaa = 0;
        settings.lighting = LightingQuality::Low;
        settings.detail = DetailDistance::Near;
        break;
    case GraphicsPreset::Medium:
        settings.renderScale = 0.85f;
        settings.msaa = 2;
        settings.lighting = LightingQuality::Medium;
        settings.detail = DetailDistance::Medium;
        break;
    case GraphicsPreset::High:
        settings.renderScale = 1.0f;
        settings.msaa = 4;
        settings.lighting = LightingQuality::High;
        settings.detail = DetailDistance::Far;
        break;
    case GraphicsPreset::Custom:
        break; // Custom describes the current choices; it is not a preset.
    default:
        throw std::invalid_argument("Unknown graphics preset");
    }
}

GraphicsPreset graphicsPreset(const GraphicsSettings& settings)
{
    if (!validGraphicsSettings(settings)) return GraphicsPreset::Custom;
    for (const auto preset : {GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High}) {
        auto expected = settings;
        applyGraphicsPreset(expected, preset);
        if (settings == expected) return preset;
    }
    return GraphicsPreset::Custom;
}

} // namespace voxel
