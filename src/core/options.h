#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace voxel {

inline constexpr const char* Usage =
    "Usage: VoxelEngine [--seed N] [--world path.vxl] [--smoke-test] [--screenshot path.ppm] [--help]\n";

struct Options {
    std::uint32_t seed = 1337;
    bool smokeTest = false;
    bool help = false;
    bool worldExplicit = false;
    std::string screenshot;
    std::string worldPath = "world.vxl";
};

Options parseOptions(std::span<const std::string_view> arguments);

} // namespace voxel
