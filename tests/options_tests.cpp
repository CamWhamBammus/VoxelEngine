#include "core/options.h"

#include <iostream>
#include <stdexcept>
#include <vector>

int main()
{
    int failures = 0;
    const auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; ++failures; }
    };
    const auto defaults = voxel::parseOptions({});
    check(defaults.seed == 1337 && !defaults.smokeTest && !defaults.worldExplicit,
          "default options preserve interactive startup");
    const std::vector<std::string_view> complete{
        "--seed", "4294967295", "--world", "my world.vxl", "--screenshot", "capture.ppm"};
    const auto parsed = voxel::parseOptions(complete);
    check(parsed.seed == 4294967295u && parsed.seedExplicit && parsed.worldExplicit && parsed.worldPath == "my world.vxl"
          && parsed.smokeTest && parsed.screenshot == "capture.ppm", "combined options parse");
    for (const std::vector<std::string_view>& args : std::vector<std::vector<std::string_view>>{
            {"--seed"}, {"--seed", "-1"}, {"--seed", "4294967296"}, {"--seed", "1tail"},
            {"--seed", ""}, {"--seed", " 1"}, {"--world"}, {"--world", ""},
            {"--world", "--help"}, {"--screenshot"}, {"--screenshot", "--seed", "4"}, {"--unknown"}}) {
        bool rejected = false;
        try { voxel::parseOptions(args); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "malformed options must fail before starting graphics or touching saves");
    }
    const std::vector<std::string_view> zero{"--seed", "0", "--smoke-test", "--help"};
    const auto flags = voxel::parseOptions(zero);
    check(flags.seed == 0 && flags.smokeTest && flags.help, "zero seed and flags work");
    return failures ? 1 : 0;
}
