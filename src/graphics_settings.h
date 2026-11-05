#pragma once

#include <filesystem>
#include <string>

namespace voxel {

enum class LightingQuality { Low, Medium, High };
enum class DetailDistance { Near, Medium, Far };
enum class GraphicsPreset { Low, Medium, High, Custom };

// Shared by the settings menu, renderer, and preferences file. Display choices
// stay independent of the quality presets.
struct GraphicsSettings {
    float renderScale = 1.0f;
    int msaa = 4;
    LightingQuality lighting = LightingQuality::High;
    DetailDistance detail = DetailDistance::Far;
    bool fullscreen = false;
    bool vsync = true;
    int fpsLimit = 0; // Zero means unlimited (VSync may still limit frames).

    bool operator==(const GraphicsSettings&) const = default;
};

bool validGraphicsSettings(const GraphicsSettings& settings);
GraphicsPreset graphicsPreset(const GraphicsSettings& settings);
void applyGraphicsPreset(GraphicsSettings& settings, GraphicsPreset preset);

struct GraphicsSettingsRead {
    GraphicsSettings settings;
    std::string message;
    bool loaded = false;
};

// A missing file quietly uses defaults. A damaged or unreadable file also uses
// defaults and supplies a short message; reading never changes the file.
GraphicsSettingsRead readGraphicsSettings(const std::filesystem::path& path);

// Write a complete sibling file, then atomically replace the old preferences.
// On failure, keep the old file and return a short message for the menu.
bool saveGraphicsSettings(const std::filesystem::path& path, const GraphicsSettings& settings,
                          std::string& message);

} // namespace voxel
