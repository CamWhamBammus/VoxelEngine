#include "core/window.h"
#include "core/camera.h"
#include "core/input.h"
#include "render/world_renderer.h"
#include "voxel/world.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    const bool smokeTest = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
    if (argc > 1 && !smokeTest) {
        std::cerr << "Usage: VoxelEngine [--smoke-test]\n";
        return EXIT_FAILURE;
    }
    try {
        voxel::Window window(!smokeTest);
        voxel::World world;
        voxel::Camera camera;
        voxel::WorldRenderer renderer;
        // Input changes the system cursor only for interactive runs.
        std::optional<voxel::Input> input;
        if (!smokeTest) input.emplace(window.handle());
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n'
                  << "WASD fly | Mouse look | Space/Ctrl up/down | Shift boost\n"
                  << "Esc release mouse | Tab capture mouse | F wireframe | Q quit\n";
        double previousTime = glfwGetTime();
        double titleTime = previousTime;
        int frames = 0;
        while (!window.shouldClose() && (!smokeTest || frames < 3)) {
            glfwPollEvents();
            const double now = glfwGetTime();
            if (input) input->update(camera, static_cast<float>(now - previousTime));
            previousTime = now;
            int width, height;
            window.framebufferSize(width, height);
            if (width <= 0 || height <= 0) {
                glfwWaitEventsTimeout(0.05);
                continue;
            }
            renderer.render(world, camera, width, height, input && input->wireframe());
            if (smokeTest && frames == 2) {
                std::vector<float> depth(static_cast<std::size_t>(width) * height);
                glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
                const auto covered = std::count_if(depth.begin(), depth.end(), [](float d) { return d < 1.0f; });
                if (static_cast<std::size_t>(covered) < depth.size() / 100 || glGetError() != GL_NO_ERROR) {
                    throw std::runtime_error("Rendering smoke test failed: missing terrain or OpenGL error");
                }
                std::cout << "Smoke test passed: " << renderer.triangleCount() << " triangles, "
                          << covered << " terrain pixels\n";
            }
            if (now - titleTime >= 0.25) {
                const auto p = camera.position();
                const std::string title = "Voxel Engine | " + std::to_string(renderer.triangleCount())
                    + " triangles | XYZ " + std::to_string(static_cast<int>(p.x)) + ", "
                    + std::to_string(static_cast<int>(p.y)) + ", " + std::to_string(static_cast<int>(p.z))
                    + " | Tab: mouse  F: wireframe  Q: quit";
                glfwSetWindowTitle(window.handle(), title.c_str());
                titleTime = now;
            }
            window.present();
            ++frames;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Voxel Engine: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
