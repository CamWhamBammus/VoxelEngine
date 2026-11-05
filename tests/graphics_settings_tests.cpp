#include "graphics_settings.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto pattern = (std::filesystem::temp_directory_path() / "voxel-graphics-test-XXXXXX").string();
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back('\0');
        if (!::mkdtemp(name.data())) throw std::runtime_error("Cannot create temporary settings folder");
        path = name.data();
    }
    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    std::filesystem::path path;
};

std::string readBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeBytes(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << bytes;
    if (!output) throw std::runtime_error("Cannot write temporary test fixture");
}
} // namespace

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; ++failures; }
    };
    const GraphicsSettings defaults;
    check(validGraphicsSettings(defaults) && graphicsPreset(defaults) == GraphicsPreset::High,
          "New players keep the existing full-quality appearance");
    for (const auto preset : {GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High}) {
        auto settings = defaults;
        settings.fullscreen = true;
        settings.vsync = false;
        settings.fpsLimit = 90;
        applyGraphicsPreset(settings, preset);
        check(validGraphicsSettings(settings) && graphicsPreset(settings) == preset,
              "Every preset has supported values and can be recognized");
        check(settings.fullscreen && !settings.vsync && settings.fpsLimit == 90,
              "Quality presets preserve display and frame pacing choices");
        const auto beforeCustom = settings;
        applyGraphicsPreset(settings, GraphicsPreset::Custom);
        check(settings == beforeCustom, "Selecting custom never changes quality choices");
        settings.msaa = settings.msaa == 4 ? 2 : 4;
        check(graphicsPreset(settings) == GraphicsPreset::Custom,
              "Changing one quality field is reported as custom");
    }
    auto invalidPreset = defaults;
    bool threw = false;
    try { applyGraphicsPreset(invalidPreset, static_cast<GraphicsPreset>(99)); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw && invalidPreset == defaults, "Unknown presets cannot partly modify preferences");
    auto invalid = defaults;
    for (const float scale : {0.0f, 0.499f, 1.001f, std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        invalid = defaults;
        invalid.renderScale = scale;
        check(!validGraphicsSettings(invalid), "Reject unsupported and non-finite render scales");
    }
    for (const int samples : {-1, 1, 3, 8}) {
        invalid = defaults;
        invalid.msaa = samples;
        check(!validGraphicsSettings(invalid), "Reject unsupported sample counts");
    }
    invalid = defaults;
    invalid.lighting = static_cast<LightingQuality>(99);
    check(!validGraphicsSettings(invalid), "Reject invalid lighting values");
    invalid = defaults;
    invalid.detail = static_cast<DetailDistance>(-1);
    check(!validGraphicsSettings(invalid), "Reject invalid detail distances");
    for (const int limit : {-1, 1, 75, 145, std::numeric_limits<int>::max()}) {
        invalid = defaults;
        invalid.fpsLimit = limit;
        check(!validGraphicsSettings(invalid), "Reject unsupported frame limits");
        check(graphicsPreset(invalid) == GraphicsPreset::High,
              "Preset recognition only considers quality fields");
    }
    for (const float scale : {0.5f, 0.65f, 0.73456789f, 0.85f, 1.0f}) {
        for (const int samples : {0, 2, 4}) {
            for (const int limit : {0, 30, 60, 90, 120, 144}) {
                auto settings = defaults;
                settings.renderScale = scale;
                settings.msaa = samples;
                settings.fpsLimit = limit;
                check(validGraphicsSettings(settings), "Supported quality and frame values remain valid together");
            }
        }
    }

    TemporaryDirectory directory;
    const auto path = directory.path / "graphics.cfg";
    auto loaded = readGraphicsSettings(path);
    check(!loaded.loaded && loaded.message.empty() && loaded.settings == defaults
          && !std::filesystem::exists(path), "Missing preferences use defaults without creating a file");
    std::string message = "old message";
    const std::array presets{GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High};
    std::size_t presetIndex = 0;
    for (const int limit : {0, 30, 60, 90, 120, 144}) {
        auto settings = defaults;
        applyGraphicsPreset(settings, presets[presetIndex++ % presets.size()]);
        settings.renderScale = 0.73456789f;
        settings.fullscreen = limit != 0;
        settings.vsync = limit == 0;
        settings.fpsLimit = limit;
        check(saveGraphicsSettings(path, settings, message) && message.empty(),
              "Saving creates or atomically replaces preferences and clears stale errors");
        loaded = readGraphicsSettings(path);
        check(loaded.loaded && loaded.message.empty() && loaded.settings == settings,
              "Every field and custom float precision survive a complete round trip");
    }
    check(saveGraphicsSettings(path, defaults, message), "Default fixture saves");
    const auto canonical = readBytes(path);
    const auto assertRejected = [&](const std::string& bytes) {
        writeBytes(path, bytes);
        const auto result = readGraphicsSettings(path);
        check(!result.loaded && !result.message.empty() && result.settings == defaults,
              "Malformed preferences return complete defaults and an explanation");
        check(readBytes(path) == bytes, "Loading invalid preferences never rewrites the user's file");
    };
    assertRejected("");
    for (std::size_t end = canonical.find('\n'); end != std::string::npos; end = canonical.find('\n', end + 1)) {
        if (end + 1 < canonical.size()) assertRejected(canonical.substr(0, end + 1));
    }
    const auto replacing = [&](const std::string& oldText, const std::string& newText) {
        auto bytes = canonical;
        const auto offset = bytes.find(oldText);
        if (offset == std::string::npos) throw std::runtime_error("Missing test fixture field");
        bytes.replace(offset, oldText.size(), newText);
        return bytes;
    };
    for (const auto& [from, to] : std::array<std::pair<const char*, const char*>, 19>{{
             {"VOXEL_GRAPHICS 1", "VOXEL_GRAPHICS 2"}, {"VOXEL_GRAPHICS 1", "OTHER_FILE 1"},
             {"render_scale 1", "render_scale nan"}, {"render_scale 1", "render_scale inf"},
             {"render_scale 1", "render_scale 0.49"}, {"render_scale 1", "render_scale 1.01"},
             {"render_scale 1", "render_scale 0.7junk"}, {"render_scale 1", "render_scale 1e999"},
             {"msaa 4", "msaa 8"}, {"msaa 4", "msaa 999999999999999999999"},
             {"msaa 4", "msaa 2 extra"}, {"lighting high", "lighting ultra"},
             {"detail far", "detail -1"}, {"fullscreen 0", "fullscreen 2"},
             {"vsync 1", "vsync true"}, {"fps_limit 0", "fps_limit -1"},
             {"fps_limit 0", "fps_limit 75"}, {"fps_limit 0", "fps_limit"},
             {"msaa 4", "msaa 4\nmsaa 2"}}}) {
        assertRejected(replacing(from, to));
    }
    assertRejected(canonical + std::string(17 * 1024, 'x'));
    writeBytes(path, canonical + "# Future client preferences\nnew_setting 4 future tokens\n\n");
    loaded = readGraphicsSettings(path);
    check(loaded.loaded && loaded.settings == defaults && loaded.message.empty(),
          "Blank lines, comments, and unknown future fields are tolerated");
    writeBytes(path, canonical.substr(0, canonical.size() - 1));
    check(readGraphicsSettings(path).loaded, "The final line does not require a newline");
    writeBytes(path, canonical);
    invalid = defaults;
    invalid.msaa = 16;
    check(!saveGraphicsSettings(path, invalid, message) && !message.empty() && readBytes(path) == canonical,
          "Invalid preferences cannot damage a previously valid file");
    const auto unavailable = directory.path / "missing" / "graphics.cfg";
    check(!saveGraphicsSettings(unavailable, defaults, message) && !message.empty()
          && !std::filesystem::exists(unavailable.parent_path()), "Save errors do not create unexpected directories");
    check(!saveGraphicsSettings({}, defaults, message) && !message.empty(), "Empty save filenames are rejected");
    check(!readGraphicsSettings({}).message.empty(), "Empty read filenames are reported");
    check(!readGraphicsSettings(directory.path).message.empty(), "Directories are not read as settings files");
    const auto blocked = directory.path / "blocked.cfg";
    std::filesystem::create_directory(blocked);
    writeBytes(blocked / "keep.txt", "keep me");
    check(!saveGraphicsSettings(blocked, defaults, message) && !message.empty()
          && readBytes(blocked / "keep.txt") == "keep me", "Failed replacement preserves the destination");
    bool temporaryLeft = false;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path)) {
        temporaryLeft |= entry.path().filename().string().starts_with(".voxel-graphics-");
    }
    check(!temporaryLeft, "Successful and failed saves clean up temporary files");
    return failures ? 1 : 0;
}
