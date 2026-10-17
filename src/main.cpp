#include "game.h"
#include "input.h"
#include "graphics/window.h"
#include "graphics/renderer.h"
#include "graphics/hud.h"
#include "graphics_smoke.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    // 1. Read startup settings. Graphics checks have their own loop in tests/.
    voxel::Options options;
    try {
        const std::vector<std::string_view> arguments(argv + 1, argv + argc);
        options = voxel::parseOptions(arguments);
    } catch (const std::invalid_argument& error) {
        std::cerr << error.what() << '\n' << voxel::Usage;
        return EXIT_FAILURE;
    }
    if (options.help) {
        std::cout << voxel::Usage;
        return EXIT_SUCCESS;
    }

    try {
        if (options.smokeTest) return voxel::runGraphicsSmokeTest(options);

        // 2. Create the game and its window. GPU objects must die before the window.
        voxel::Game game(options);
        voxel::Window window;
        voxel::Input input(window.handle());
        voxel::WorldRenderer renderer;
        voxel::Overlay selection;
        voxel::Hud hud;
        std::cout << game.status << "\nH: controls | F5: save | F9: reload | Q: quit\n";

        double previousTime = glfwGetTime();
        double sampleStart = previousTime;
        int sampleFrames = 0;
        std::string statusText;

        // 3. Each frame: read input, update the world, then draw it.
        while (!window.shouldClose()) {
            glfwPollEvents();
            const double now = glfwGetTime();
            const float deltaSeconds = static_cast<float>(now - previousTime);
            previousTime = now;

            input.update(game.camera, deltaSeconds);
            game.update(input.actions());

            int width, height;
            window.framebufferSize(width, height);
            if (width <= 0 || height <= 0) {
                glfwWaitEventsTimeout(0.05); // Avoid busy-looping when minimized.
                continue;
            }
            renderer.render(game.world, game.camera, width, height, input.wireframe());
            float scaleX, scaleY;
            glfwGetWindowContentScale(window.handle(), &scaleX, &scaleY);
            if (input.captured()) selection.draw(game.target, game.camera, width, height, scaleY);

            // Refresh text four times per second; draw the cached text every frame.
            ++sampleFrames;
            const double sampleSeconds = now - sampleStart;
            if (statusText.empty() || sampleSeconds >= 0.25) {
                const double fps = sampleSeconds >= 0.25 ? sampleFrames / sampleSeconds : 0.0;
                statusText = game.statusText(renderer.visibleChunks(), renderer.drawnTriangles(),
                    renderer.triangleCount(), fps, input.captured(), input.helpVisible());
                glfwSetWindowTitle(window.handle(), game.hasUnsavedChanges() ? "Voxel Engine *" : "Voxel Engine");
                if (sampleSeconds >= 0.25) {
                    sampleStart = now;
                    sampleFrames = 0;
                }
            }
            hud.draw(statusText, width, height, scaleY);
            window.present();
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Voxel Engine: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
