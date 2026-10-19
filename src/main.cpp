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
        std::cout << game.status << "\nH: controls | Q: quit\n";

        double previousTime = glfwGetTime();
        double sampleStart = previousTime;
        int sampleFrames = 0;
        std::string statusText;

        // 3. Each frame: read input, update the world, then draw it.
        while (true) {
            if (window.shouldClose()) {
                if (!game.saveAuthoringDraft()) {
                    // Keep the document alive so a failed save can be corrected.
                    std::cerr << (game.devMode() ? game.sectionEditor().message() : game.levelController().message()) << '\n';
                    glfwSetWindowShouldClose(window.handle(), GLFW_FALSE);
                } else break;
            }
            glfwPollEvents();
            const double now = glfwGetTime();
            const float deltaSeconds = static_cast<float>(now - previousTime);
            previousTime = now;

            input.update(game.camera, deltaSeconds, game.editorMode(), game.menuOpen(), game.crossingMenuOpen(),
                         game.devMode() || game.levelMenuOpen(), game.devFlying(),
                         (game.devMode() && game.sectionEditor().screen() == voxel::SectionEditorScreen::Accept)
                             || game.levelController().typing());
            auto actions = input.actions();
            if ((game.devMenuOpen() || game.levelMenuOpen()) && actions.devPointerPressed) {
                int menuWidth, menuHeight;
                float menuScaleX, menuScaleY;
                window.framebufferSize(menuWidth, menuHeight);
                glfwGetWindowContentScale(window.handle(), &menuScaleX, &menuScaleY);
                actions.devClick = game.levelMenuOpen()
                    ? voxel::Hud::levelClick(game, menuWidth, menuHeight, menuScaleY,
                        actions.devPointer.x * menuWidth, actions.devPointer.y * menuHeight)
                    : voxel::Hud::sectionClick(game, menuWidth, menuHeight, menuScaleY,
                        actions.devPointer.x * menuWidth, actions.devPointer.y * menuHeight);
            }
            game.update(actions, deltaSeconds);

            int width, height;
            window.framebufferSize(width, height);
            if (width <= 0 || height <= 0) {
                glfwWaitEventsTimeout(0.05); // Avoid busy-looping when minimized.
                continue;
            }
            renderer.render(game.world, game.camera, width, height, input.wireframe(), !game.editorMode(),
                            static_cast<float>(now), !game.editorMode()
                                && (game.devMode() || game.equipment.selectedTool() == voxel::Tool::Lantern),
                            !game.rootCrossingActive() && !game.devMode() && !game.levelActive(),
                            game.devMode() ? std::span<const voxel::HubAsset>(game.sectionEditor().assets())
                                : game.levelActive() ? std::span<const voxel::HubAsset>(game.levelController().assets())
                                           : std::span<const voxel::HubAsset>{});
            float scaleX, scaleY;
            glfwGetWindowContentScale(window.handle(), &scaleX, &scaleY);
            if (input.captured() && !game.menuOpen()) {
                selection.draw(game.target, game.camera, width, height, scaleY);
            }
            if (game.devMode() && game.sectionEditor().hasSection() && !game.devMenuOpen())
                selection.drawSection(game.sectionEditor().meta(), game.sectionEditor().placementPreview(),
                                      game.devFlying() ? game.sectionEditor().targetedAsset() : std::nullopt,
                                      game.camera, width, height);
            if (game.levelActive() && !game.levelMenuOpen())
                selection.drawSection(game.levelController().meta(), {}, {}, game.camera, width, height);

            // Refresh text four times per second; draw the cached text every frame.
            ++sampleFrames;
            const double sampleSeconds = now - sampleStart;
            if (statusText.empty() || sampleSeconds >= 0.25) {
                const double fps = sampleSeconds >= 0.25 ? sampleFrames / sampleSeconds : 0.0;
                statusText = game.statusText(renderer.visibleChunks(), renderer.drawnTriangles(),
                    renderer.triangleCount(), fps, input.captured(), input.helpVisible());
                const char* title = game.levelCreatorActive() ? "Level Creator"
                    : game.levelController().open() ? "Expeditions"
                    : game.devMode() ? "Section Builder"
                    : game.editorMode()
                    ? (game.hasUnsavedChanges() ? "Voxel Editor *" : "Voxel Editor")
                    : game.rootCrossingActive() ? "Root Crossing Playtest" : "Underground Village";
                glfwSetWindowTitle(window.handle(), title);
                if (sampleSeconds >= 0.25) {
                    sampleStart = now;
                    sampleFrames = 0;
                }
            }
            if (!game.devMode() && !game.levelController().open()
                && ((!game.rootCrossingActive() && !game.crossingMenuOpen()) || !input.captured() || input.helpVisible()))
                hud.draw(statusText, width, height, scaleY);
            if (game.devMode()) hud.drawSection(game, width, height, scaleY);
            else if (!game.editorMode() && !game.levelMenuOpen()) hud.drawPlay(game, width, height, scaleY);
            if (game.levelController().open()) hud.drawLevels(game, width, height, scaleY);
            if ((game.devMode() || game.levelActive()) && !game.menuOpen() && !input.captured())
                hud.draw("PAUSED | TAB TO RESUME", width, height, scaleY);
            window.present();
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Voxel Engine: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
