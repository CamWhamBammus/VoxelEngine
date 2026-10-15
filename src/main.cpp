#include "core/window.h"
#include "core/camera.h"
#include "core/input.h"
#include "render/world_renderer.h"
#include "render/overlay.h"
#include "render/diagnostics.h"
#include "voxel/editor.h"
#include "voxel/raycast.h"
#include "voxel/world.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <charconv>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr const char* usage = "Usage: VoxelEngine [--seed N] [--smoke-test] [--screenshot path.ppm] [--help]\n";

struct Options {
    std::uint32_t seed = 1337;
    bool smokeTest = false;
    bool help = false;
    std::string screenshot;
};

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument == "--help") options.help = true;
        else if (argument == "--smoke-test") options.smokeTest = true;
        else if (argument == "--seed" && i + 1 < argc) {
            const std::string_view value(argv[++i]);
            const auto result = std::from_chars(value.data(), value.data() + value.size(), options.seed);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
                throw std::invalid_argument("Seed must be an integer from 0 to 4294967295");
            }
        } else if (argument == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
            if (options.screenshot.empty()) throw std::invalid_argument("Screenshot path is empty");
            options.smokeTest = true;
        } else {
            throw std::invalid_argument(std::string("Unknown or incomplete option: ") + std::string(argument));
        }
    }
    return options;
}
} // namespace

int main(int argc, char** argv)
{
    Options options;
    try {
        options = parseOptions(argc, argv);
    } catch (const std::invalid_argument& error) {
        std::cerr << error.what() << '\n' << usage;
        return EXIT_FAILURE;
    }
    if (options.help) {
        std::cout << usage;
        return EXIT_SUCCESS;
    }
    const bool smokeTest = options.smokeTest;
    try {
        voxel::Window window(!smokeTest);
        if (smokeTest) voxel::verifyGpuResources();
        voxel::World world(options.seed);
        voxel::Camera camera;
        voxel::WorldRenderer renderer;
        voxel::Overlay overlay;
        // Input changes the system cursor only for interactive runs.
        std::optional<voxel::Input> input;
        if (!smokeTest) input.emplace(window.handle());
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n'
                  << "WASD fly | Mouse look | Space/Ctrl up/down | Shift boost\n"
                  << "Esc release mouse | Tab capture mouse | F wireframe | Q quit\n";
        std::cout << "Left click remove | Right click place | 1 Grass 2 Dirt 3 Stone 4 Sand\n";
        double previousTime = glfwGetTime();
        double titleTime = previousTime;
        int frames = 0;
        bool frameVerified = false;
        while (!window.shouldClose() && (!smokeTest || frames < 3)) {
            glfwPollEvents();
            const double now = glfwGetTime();
            if (input) input->update(camera, static_cast<float>(now - previousTime));
            previousTime = now;
            auto target = voxel::raycast(camera.position(), camera.forward(), 8.0f,
                [&world](glm::ivec3 p) { return world.get(p); });
            if (input && target) {
                bool edited = false;
                if (input->removeRequested()) edited = voxel::removeBlock(world, *target);
                else if (input->placeRequested()) {
                    edited = voxel::placeBlock(world, *target, input->selectedBlock(), camera.position());
                }
                if (edited) {
                    target = voxel::raycast(camera.position(), camera.forward(), 8.0f,
                        [&world](glm::ivec3 p) { return world.get(p); });
                }
            }
            int width, height;
            window.framebufferSize(width, height);
            if (width <= 0 || height <= 0) {
                if (smokeTest) throw std::runtime_error("Smoke test framebuffer is empty");
                glfwWaitEventsTimeout(0.05);
                continue;
            }
            renderer.render(world, camera, width, height, input && input->wireframe());
            if (!input || input->captured()) overlay.draw(target, camera, width, height);
            if (smokeTest && frames == 2) {
                voxel::verifyFrame(width, height, options.screenshot);
                frameVerified = true;
                std::cout << "Smoke test passed: " << renderer.triangleCount() << " triangles\n";
            }
            if (now - titleTime >= 0.25) {
                const auto p = camera.position();
                const auto selected = input ? input->selectedBlock() : voxel::Block::Grass;
                const std::string title = std::string("Voxel Engine | ") + std::string(voxel::blockName(selected))
                    + " | " + std::to_string(renderer.triangleCount())
                    + " triangles | XYZ " + std::to_string(static_cast<int>(p.x)) + ", "
                    + std::to_string(static_cast<int>(p.y)) + ", " + std::to_string(static_cast<int>(p.z))
                    + " | 1-4: material  Tab: mouse  Q: quit";
                glfwSetWindowTitle(window.handle(), title.c_str());
                titleTime = now;
            }
            window.present();
            ++frames;
        }
        if (smokeTest && !frameVerified) {
            throw std::runtime_error("Window closed before the smoke test completed");
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Voxel Engine: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
