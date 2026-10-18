#include "game.h"

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
    check(defaults.seed == 1337 && defaults.editReach == 8 && !defaults.smokeTest && !defaults.worldExplicit
          && !defaults.editor && defaults.rootCrossing == -1, "default options start a fresh game");
    const std::vector<std::string_view> complete{
        "--editor", "--seed", "4294967295", "--world", "my world.vxl", "--screenshot", "capture.ppm"};
    const auto parsed = voxel::parseOptions(complete);
    check(parsed.seed == 4294967295u && parsed.seedExplicit && parsed.worldExplicit && parsed.worldPath == "my world.vxl"
          && parsed.editor && parsed.smokeTest && parsed.screenshot == "capture.ppm", "combined options parse");
    for (const std::vector<std::string_view>& args : std::vector<std::vector<std::string_view>>{
            {"--world", "-h"}, {"--screenshot", "-h"},
            {"--world", "game.vxl"},
            {"--seed"}, {"--seed", "-1"}, {"--seed", "4294967296"}, {"--seed", "1tail"},
            {"--seed", ""}, {"--seed", " 1"}, {"--world"}, {"--world", ""},
            {"--world", "--help"}, {"--screenshot"}, {"--screenshot", "--seed", "4"}, {"--unknown"},
            {"--root-crossing"}, {"--root-crossing", "0"}, {"--root-crossing", "11"},
            {"--root-crossing", "-1"}, {"--root-crossing", "2tail"}, {"--root-crossing", "1.5"},
            {"--root-crossing", "999999999999"}, {"--root-crossing", "--smoke-test"},
            {"--root-crossing", "1", "--editor"}, {"--editor", "--root-crossing", "10"},
            {"--root-crossing", "3", "--world", "saved.vxl"},
            {"--editor", "--world", "saved.vxl", "--root-crossing", "3"},
            {"--reach"}, {"--reach", "0"}, {"--reach", "65"}, {"--reach", "-1"},
            {"--reach", "1.5"}, {"--reach", "NaN"}, {"--reach", "999999999999"}}) {
        bool rejected = false;
        try { voxel::parseOptions(args); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "malformed options must fail before starting graphics or touching saves");
    }
    const std::vector<std::string_view> zero{"--seed", "0", "--smoke-test", "--help"};
    const auto flags = voxel::parseOptions(zero);
    check(flags.seed == 0 && flags.smokeTest && flags.help, "zero seed and flags work");
    const std::vector<std::string_view> shortHelp{"-h"};
    check(voxel::parseOptions(shortHelp).help, "-h opens command-line help");
    for (const auto limit : {"1", "64"}) {
        const std::vector<std::string_view> reach{"--reach", limit};
        check(voxel::parseOptions(reach).editReach == std::stoi(limit), "reach accepts both range limits");
    }
    for (const auto number : {"1", "10"}) {
        const std::vector<std::string_view> args{"--root-crossing", number};
        const auto crossing = voxel::parseOptions(args);
        check(crossing.rootCrossing == std::stoi(number) - 1 && !crossing.editor
              && crossing.progressPath == "progress.vxp", "crossing CLI maps 1-based choices to test indices");
        const std::vector<std::string_view> smokeArgs{"--root-crossing", number, "--smoke-test"};
        check(voxel::parseOptions(smokeArgs).progressPath.empty(), "crossing smoke never opens a progress profile");
    }
    return failures ? 1 : 0;
}
