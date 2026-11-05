#include "graphics_settings.h"

#include <array>
#include <cerrno>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <unistd.h>

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
    for (const auto preset : {GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High}) {
        auto expected = settings;
        applyGraphicsPreset(expected, preset);
        if (settings == expected) return preset;
    }
    return GraphicsPreset::Custom;
}

namespace {
constexpr std::size_t MaxSettingsBytes = 16 * 1024;

GraphicsSettingsRead rejected(const char* message)
{
    return {GraphicsSettings{}, message, false};
}

bool validFilename(const std::filesystem::path& path)
{
    return path.has_filename() && path.filename() != "." && path.filename() != "..";
}

template<class T>
bool readValue(std::istringstream& fields, T& value)
{
    if (!(fields >> value)) return false;
    fields >> std::ws;
    return fields.eof();
}

const char* lightingName(LightingQuality quality)
{
    switch (quality) {
    case LightingQuality::Low: return "low";
    case LightingQuality::Medium: return "medium";
    case LightingQuality::High: return "high";
    }
    return "invalid";
}

const char* detailName(DetailDistance detail)
{
    switch (detail) {
    case DetailDistance::Near: return "near";
    case DetailDistance::Medium: return "medium";
    case DetailDistance::Far: return "far";
    }
    return "invalid";
}
} // namespace

GraphicsSettingsRead readGraphicsSettings(const std::filesystem::path& path)
{
    if (!validFilename(path)) return rejected("Graphics settings need a filename.");
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (error == std::errc::no_such_file_or_directory || (!error && !std::filesystem::exists(status)))
        return {};
    if (error || !std::filesystem::is_regular_file(status))
        return rejected("Could not read graphics settings. Using defaults.");

    std::ifstream input(path, std::ios::binary);
    if (!input) return rejected("Could not read graphics settings. Using defaults.");
    std::array<char, MaxSettingsBytes + 1> bytes{};
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (input.bad() || !input.eof()) return rejected("Invalid graphics settings. Using defaults.");
    std::istringstream contents(std::string(bytes.data(), static_cast<std::size_t>(input.gcount())));
    contents.imbue(std::locale::classic());
    std::string line;
    if (!std::getline(contents, line)) return rejected("Incomplete graphics settings. Using defaults.");
    std::istringstream header(line);
    header.imbue(std::locale::classic());
    std::string signature;
    int version = 0;
    if (!(header >> signature) || signature != "VOXEL_GRAPHICS" || !readValue(header, version) || version != 1)
        return rejected("Unsupported graphics settings. Using defaults.");

    GraphicsSettings parsed;
    unsigned seen = 0;
    while (std::getline(contents, line)) {
        std::istringstream fields(line);
        fields.imbue(std::locale::classic());
        std::string key;
        if (!(fields >> key) || key.front() == '#') continue;
        unsigned bit = 0;
        bool valid = true;
        if (key == "render_scale") {
            bit = 1u << 0;
            valid = readValue(fields, parsed.renderScale);
        } else if (key == "msaa") {
            bit = 1u << 1;
            valid = readValue(fields, parsed.msaa);
        } else if (key == "lighting") {
            bit = 1u << 2;
            std::string value;
            valid = readValue(fields, value);
            if (value == "low") parsed.lighting = LightingQuality::Low;
            else if (value == "medium") parsed.lighting = LightingQuality::Medium;
            else if (value == "high") parsed.lighting = LightingQuality::High;
            else valid = false;
        } else if (key == "detail") {
            bit = 1u << 3;
            std::string value;
            valid = readValue(fields, value);
            if (value == "near") parsed.detail = DetailDistance::Near;
            else if (value == "medium") parsed.detail = DetailDistance::Medium;
            else if (value == "far") parsed.detail = DetailDistance::Far;
            else valid = false;
        } else if (key == "fullscreen" || key == "vsync") {
            bit = 1u << (key == "fullscreen" ? 4 : 5);
            int value = -1;
            valid = readValue(fields, value) && (value == 0 || value == 1);
            if (key == "fullscreen") parsed.fullscreen = value == 1;
            else parsed.vsync = value == 1;
        } else if (key == "fps_limit") {
            bit = 1u << 6;
            valid = readValue(fields, parsed.fpsLimit);
        } else {
            continue; // New optional fields do not break older clients.
        }
        if (!valid || (seen & bit)) return rejected("Invalid graphics settings. Using defaults.");
        seen |= bit;
    }
    if (seen != 0x7fu) return rejected("Incomplete graphics settings. Using defaults.");
    if (!validGraphicsSettings(parsed)) return rejected("Invalid graphics settings. Using defaults.");
    return {parsed, {}, true};
}

bool saveGraphicsSettings(const std::filesystem::path& path, const GraphicsSettings& settings,
                          std::string& message)
{
    message.clear();
    if (!validGraphicsSettings(settings)) {
        message = "These graphics settings are invalid.";
        return false;
    }
    if (!validFilename(path)) {
        message = "Graphics settings need a filename.";
        return false;
    }
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    std::error_code error;
    if (!std::filesystem::is_directory(parent, error)) {
        message = "The graphics settings folder is unavailable.";
        return false;
    }

    std::ostringstream encoded;
    encoded.imbue(std::locale::classic());
    encoded << std::setprecision(std::numeric_limits<float>::max_digits10)
            << "VOXEL_GRAPHICS 1\nrender_scale " << settings.renderScale
            << "\nmsaa " << settings.msaa << "\nlighting " << lightingName(settings.lighting)
            << "\ndetail " << detailName(settings.detail) << "\nfullscreen " << (settings.fullscreen ? 1 : 0)
            << "\nvsync " << (settings.vsync ? 1 : 0) << "\nfps_limit " << settings.fpsLimit << '\n';
    const auto bytes = encoded.str();
    const auto pattern = (parent / ".voxel-graphics-XXXXXX").string();
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back('\0');
    int descriptor = ::mkstemp(temporary.data());
    if (descriptor < 0) {
        message = "Could not create the graphics settings file.";
        return false;
    }
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw std::runtime_error("write");
            offset += static_cast<std::size_t>(written);
        }
        if (::fsync(descriptor) != 0) throw std::runtime_error("flush");
        const int closed = ::close(descriptor);
        descriptor = -1;
        if (closed != 0) throw std::runtime_error("close");
        std::filesystem::rename(std::filesystem::path(temporary.data()), path);
        return true;
    } catch (const std::exception&) {
        if (descriptor >= 0) ::close(descriptor);
        ::unlink(temporary.data());
        message = "Could not save graphics settings. The old file was kept.";
        return false;
    }
}

} // namespace voxel
