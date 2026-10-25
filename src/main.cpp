#include "game.h"
#include "input.h"
#include "graphics/window.h"
#include "graphics/renderer.h"
#include "graphics/hud.h"
#include "graphics_smoke.h"

#include <GLFW/glfw3.h>
#include <cmath>
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
        std::string statusText = game.statusText(0, 0, 0, 0, input.captured(), input.helpVisible());
        bool previousHelpVisible = input.helpVisible();
        bool previousCaptured = input.captured();
        float carriedToolTime = 0;

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
                         (game.devMode() && game.sectionEditor().typing())
                             || game.levelController().typing(), game.inventoryOpen());
            auto actions = input.actions();
            if (game.inventoryOpen() && actions.inventoryPointerPressed) {
                int menuWidth, menuHeight;
                float menuScaleX, menuScaleY;
                window.framebufferSize(menuWidth, menuHeight);
                glfwGetWindowContentScale(window.handle(), &menuScaleX, &menuScaleY);
                const auto hit = voxel::Hud::inventoryClick(game, menuWidth, menuHeight, menuScaleY,
                    actions.inventoryPointer.x * menuWidth, actions.inventoryPointer.y * menuHeight);
                if (hit) {
                    if (*hit >= voxel::InventoryUiSlotBase && *hit < voxel::InventoryUiSlotBase + 20)
                        actions.inventoryClick = *hit - voxel::InventoryUiSlotBase;
                    else if (*hit >= voxel::InventoryUiLootBase && *hit < voxel::InventoryUiSort)
                        actions.lootClick = *hit - voxel::InventoryUiLootBase;
                    else if (*hit == voxel::InventoryUiSort) actions.inventorySort = true;
                    else if (*hit == voxel::InventoryUiTakeAll) actions.lootAll = true;
                    else if (*hit == voxel::InventoryUiClose) actions.closeMenu = true;
                }
            }
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
            const bool holdLantern = game.healthVisible() && !game.health().dead()
                && game.equipment.selectedTool() == voxel::Tool::Lantern;
            const bool holdCompass = game.healthVisible() && !game.health().dead()
                && game.equipment.selectedTool() == voxel::Tool::Compass;
            if ((holdLantern || holdCompass) && actions.active)
                carriedToolTime = std::fmod(carriedToolTime + deltaSeconds, 8.0f);
            std::optional<voxel::HeldCompassState> heldCompass;
            if (holdCompass) heldCompass = {
                voxel::compassBearing(game.camera.position(), game.camera.forward(), game.compassTarget()),
                carriedToolTime};

            int width, height;
            window.framebufferSize(width, height);
            if (width <= 0 || height <= 0) {
                glfwWaitEventsTimeout(0.05); // Avoid busy-looping when minimized.
                continue;
            }
            const auto viewCamera = game.viewCamera();
            renderer.render(game.world, viewCamera, width, height, input.wireframe(), !game.editorMode(),
                            static_cast<float>(now), game.carriedLantern(),
                            !game.rootCrossingActive() && !game.devMode() && !game.levelActive() && !game.monsterPreviewActive(),
                            game.sectionSceneAssets(), game.sectionPickupOffsets(), game.monsters(),
                            game.encounters().projectiles(),
                            game.healthVisible() && !game.health().dead()
                                && game.equipment.selectedTool() == voxel::Tool::Sword
                                ? std::optional<float>(game.encounters().swordSwing()) : std::nullopt,
                            game.encounters().hits(),
                            game.healthVisible() && !game.health().dead()
                                && game.equipment.selectedTool() == voxel::Tool::Pickaxe
                                ? std::optional<float>(game.mining().swing()) : std::nullopt,
                            holdLantern ? std::optional<float>(carriedToolTime) : std::nullopt,
                            game.darkCave(), heldCompass);
            float scaleX, scaleY;
            glfwGetWindowContentScale(window.handle(), &scaleX, &scaleY);
            if (input.captured() && !game.menuOpen()) {
                selection.draw(game.target, viewCamera, width, height, scaleY,
                               game.target ? game.world.get(game.target->position) : voxel::Block::Air);
            }
            if (game.devMode() && game.sectionEditor().hasSection() && !game.devMenuOpen())
                selection.drawSection(game.sectionEditor().meta(), game.sectionEditor().placementPreview(),
                                      game.devFlying() ? game.sectionEditor().targetedAsset() : std::nullopt,
                                      viewCamera, width, height, static_cast<float>(now));
            if (game.devFlying() && !game.devMenuOpen())
                selection.drawBuildTools(game.sectionEditor(), viewCamera, width, height,
                                         static_cast<float>(now));
            if (game.levelActive() && !game.levelMenuOpen())
                selection.drawSection(game.levelController().meta(), {}, {}, viewCamera, width, height,
                                      static_cast<float>(now));

            // Refresh text four times per second; draw the cached text every frame.
            ++sampleFrames;
            const double sampleSeconds = now - sampleStart;
            if (sampleSeconds >= 0.25 || input.helpVisible() != previousHelpVisible
                || input.captured() != previousCaptured) {
                const double fps = sampleSeconds >= 0.25 ? sampleFrames / sampleSeconds : 0.0;
                statusText = game.statusText(renderer.visibleChunks(), renderer.drawnTriangles(),
                    renderer.triangleCount(), fps, input.captured(), input.helpVisible());
                previousHelpVisible = input.helpVisible();
                previousCaptured = input.captured();
                const char* title = game.monsterPreviewActive() ? "Creature Preview"
                    : game.levelCreatorActive() ? "Level Creator"
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
            hud.drawCombat(game,width,height,scaleY);
            hud.drawMining(game,width,height,scaleY);
            if (game.monsterPreviewActive()) hud.drawMonsters(game, width, height, scaleY);
            else if (game.devMode()) hud.drawSection(game, width, height, scaleY);
            else if (!game.editorMode() && !game.levelMenuOpen()) hud.drawPlay(game, width, height, scaleY);
            if (game.levelController().open()) hud.drawLevels(game, width, height, scaleY);
            hud.drawInventory(game, width, height, scaleY);
            hud.drawHealth(game,width,height,scaleY);
            if (!game.menuOpen() && (game.editorMode() || input.helpVisible() || !input.captured()))
                hud.draw(statusText, width, height, scaleY);
            window.present();
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Voxel Engine: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
