#include "core/options.h"

#include <charconv>
#include <stdexcept>

namespace voxel {

Options parseOptions(std::span<const std::string_view> arguments)
{
    Options options;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const auto argument = arguments[i];
        const auto value = [&]() {
            if (++i == arguments.size() || arguments[i].empty() || arguments[i].starts_with("--")) {
                throw std::invalid_argument("Missing value for " + std::string(argument));
            }
            return arguments[i];
        };
        if (argument == "--help") options.help = true;
        else if (argument == "--smoke-test") options.smokeTest = true;
        else if (argument == "--seed") {
            const auto seed = value();
            const auto result = std::from_chars(seed.data(), seed.data() + seed.size(), options.seed);
            if (result.ec != std::errc{} || result.ptr != seed.data() + seed.size()) {
                throw std::invalid_argument("Seed must be an integer from 0 to 4294967295");
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

} // namespace voxel
