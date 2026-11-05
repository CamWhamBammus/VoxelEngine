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
    voxel::GraphicsSettings saved;
    voxel::applyGraphicsPreset(saved, voxel::GraphicsPreset::Low);
    saved.fullscreen = true;
    saved.fpsLimit = 60;
    check(voxel::startupGraphicsSettings(defaults, saved) == saved, "ordinary launches retain saved preferences");
    const std::vector<std::string_view> overrideArgs{"--render-scale", "1", "--msaa", "4"};
    const auto overridden = voxel::startupGraphicsSettings(voxel::parseOptions(overrideArgs), saved);
    check(overridden.renderScale == 1 && overridden.msaa == 4 && overridden.fullscreen
        && overridden.fpsLimit == 60 && overridden.lighting == saved.lighting,
        "explicit full-quality arguments override saved values without resetting unrelated settings");
    for (auto flag : {"--reset-graphics", "--smoke-test", "--benchmark"}) {
        const std::vector<std::string_view> args{flag};
        check(voxel::startupGraphicsSettings(voxel::parseOptions(args), saved) == voxel::GraphicsSettings{},
              "recovery and automated checks start with deterministic defaults");
    }
    const std::vector<std::string_view> recoveryArgs{"--reset-graphics", "--render-scale", "0.75"};
    const auto recovery = voxel::startupGraphicsSettings(voxel::parseOptions(recoveryArgs), saved);
    check(!recovery.fullscreen && recovery.renderScale == .75f && recovery.msaa == 4,
          "recovery can still use explicit command-line graphics overrides");
    check(defaults.seed == 1337 && defaults.editReach == 8 && !defaults.smokeTest && !defaults.worldExplicit
          && !defaults.editor && defaults.rootCrossing == -1 && !defaults.benchmark
          && defaults.renderScale == 1.0f && defaults.msaa == 4, "default options start a fresh game at native resolution");
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
            {"--reach", "1.5"}, {"--reach", "NaN"}, {"--reach", "999999999999"},
            {"--render-scale"}, {"--render-scale", "0.49"}, {"--render-scale", "1.01"},
            {"--render-scale", "NaN"}, {"--render-scale", "inf"}, {"--render-scale", "-inf"},
            {"--render-scale", "0.75tail"}, {"--render-scale", " 0.75"}, {"--render-scale", "0.75 "},
            {"--render-scale", "1e999"}, {"--msaa"}, {"--msaa", "1"}, {"--msaa", "8"},
            {"--msaa", "-1"}, {"--msaa", "2.0"}, {"--msaa", "2tail"}}) {
        bool rejected = false;
        try { voxel::parseOptions(args); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "malformed options must fail before starting graphics or touching saves");
    }
    const std::vector<std::string_view> zero{"--seed", "0", "--smoke-test", "--help"};
    const auto flags = voxel::parseOptions(zero);
    check(flags.seed == 0 && flags.smokeTest && flags.help, "zero seed and flags work");
    const std::vector<std::string_view> shortHelp{"-h"};
    check(voxel::parseOptions(shortHelp).help, "-h opens command-line help");
    for (const auto scale : {"0.5", "0.75", "1"}) {
        const std::vector<std::string_view> args{"--render-scale", scale};
        check(voxel::parseOptions(args).renderScale == std::stof(scale), "render scale accepts endpoints and intermediate sizes");
    }
    for (const auto samples : {"0", "2", "4"}) {
        const std::vector<std::string_view> args{"--msaa", samples};
        check(voxel::parseOptions(args).msaa == std::stoi(samples), "MSAA accepts off and supported quality levels");
    }
    const std::vector<std::string_view> benchmark{"--benchmark", "--render-scale", "0.75", "--msaa", "0"};
    const auto measured = voxel::parseOptions(benchmark);
    check(measured.benchmark && measured.progressPath.empty() && measured.renderScale == 0.75f && measured.msaa == 0,
          "benchmark keeps graphics settings and never opens the player profile");
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
