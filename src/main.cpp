#include "core/window.h"
#include "core/camera.h"
#include "core/input.h"
#include "core/options.h"
#include "render/world_renderer.h"
#include "render/overlay.h"
#include "render/diagnostics.h"
#include "render/hud.h"
#include "voxel/editor.h"
#include "voxel/edit_history.h"
#include "voxel/raycast.h"
#include "voxel/world.h"
#include "voxel/world_io.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>


int main(int argc, char** argv)
{
    voxel::Options options;
    try {
        std::vector<std::string_view> arguments(argv + 1, argv + argc);
        options = voxel::parseOptions(arguments);
    } catch (const std::invalid_argument& error) {
        std::cerr << error.what() << '\n' << voxel::Usage;
        return EXIT_FAILURE;
    }
    if (options.help) {
        std::cout << voxel::Usage;
        return EXIT_SUCCESS;
    }
    const bool smokeTest = options.smokeTest;
    try {
        const bool loadExisting = (options.worldExplicit || (!smokeTest && !options.seedExplicit))
            && std::filesystem::exists(options.worldPath);
        voxel::World world = loadExisting ? voxel::loadWorld(options.worldPath) : voxel::World(options.seed);
        voxel::World savedWorld = world;
        bool unsaved = false;
        voxel::EditHistory history;
        std::string status = loadExisting ? "Loaded " + options.worldPath : "New world - F5 to save";
        voxel::Window window(!smokeTest);
        if (smokeTest) {
            voxel::verifyGpuResources();
            int width, height;
            window.framebufferSize(width, height);
            voxel::verifyRenderTransitions(width, height);
        }
        voxel::Camera camera;
        voxel::WorldRenderer renderer;
        voxel::Overlay overlay;
        voxel::Hud hud;
        // Input changes the system cursor only for interactive runs.
        std::optional<voxel::Input> input;
        if (!smokeTest) input.emplace(window.handle());
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n'
                  << "WASD fly | Mouse look | Space/Ctrl up/down | Shift boost\n"
                  << "Esc release mouse | Tab capture mouse | F wireframe | Q quit\n";
        std::cout << "Left click remove | Right click place | 1 Grass 2 Dirt 3 Stone 4 Sand\n";
        std::cout << "F5 save | F9 reload | World file: " << options.worldPath << '\n' << status << '\n';
        std::cout << "Ctrl/Cmd Z undo | Ctrl/Cmd Y or Shift Z redo | Middle click pick material\n";
        std::cout << "Scroll zoom | R reset camera | H help overlay\n";
        double previousTime = glfwGetTime();
        double titleTime = previousTime;
        double sampleTime = previousTime;
        int sampleFrames = 0;
        double framesPerSecond = 0.0;
        std::string hudText;
        int frames = 0;
        bool frameVerified = false;
        while (!window.shouldClose() && (!smokeTest || frames < 3)) {
            glfwPollEvents();
            const double now = glfwGetTime();
            ++sampleFrames;
            if (now - sampleTime >= 0.25) {
                framesPerSecond = sampleFrames / (now - sampleTime);
                sampleTime = now;
                sampleFrames = 0;
            }
            if (input) input->update(camera, static_cast<float>(now - previousTime));
            previousTime = now;
            if (input && (input->saveRequested() || input->loadRequested())) {
                try {
                    if (input->saveRequested()) {
                        voxel::saveWorld(world, options.worldPath);
                        status = "Saved " + options.worldPath;
                    } else {
                        world = voxel::loadWorld(options.worldPath);
                        history.clear();
                        status = "Loaded " + options.worldPath;
                    }
                    savedWorld = world;
                    unsaved = false;
                    std::cout << status << '\n';
                } catch (const std::exception& error) {
                    status = std::string("File error: ") + error.what();
                    std::cerr << status << '\n';
                }
            }
            if (input && (input->undoRequested() || input->redoRequested())) {
                const bool undo = input->undoRequested();
                const bool changed = undo ? history.undo(world) : history.redo(world);
                if (changed) unsaved = !world.sameContents(savedWorld);
                status = changed ? (undo ? "Undid edit" : "Redid edit") : "No edit available";
            }
            auto target = voxel::raycast(camera.position(), camera.forward(), 8.0f,
                [&world](glm::ivec3 p) { return world.get(p); });
            if (input && target) {
                if (input->pickRequested()) input->selectBlock(world.get(target->position));
                bool edited = false;
                if (input->removeRequested()) edited = voxel::removeBlock(world, *target, &history);
                else if (input->placeRequested()) {
                    edited = voxel::placeBlock(world, *target, input->selectedBlock(), camera.position(), &history);
                }
                if (edited) {
                    unsaved = !world.sameContents(savedWorld);
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
            if (frames == 0 || now - titleTime >= 0.25 || (smokeTest && frames == 2)) {
                const auto p = camera.position();
                const auto selected = input ? input->selectedBlock() : voxel::Block::Grass;
                const std::string title = std::string(unsaved ? "Voxel Engine * | " : "Voxel Engine | ")
                    + std::string(voxel::blockName(selected))
                    + " | " + std::to_string(renderer.triangleCount())
                    + " triangles | XYZ " + std::to_string(static_cast<int>(p.x)) + ", "
                    + std::to_string(static_cast<int>(p.y)) + ", " + std::to_string(static_cast<int>(p.z))
                    + " | " + status;
                glfwSetWindowTitle(window.handle(), title.c_str());
                std::ostringstream text;
                text << std::fixed << std::setprecision(1)
                     << "VOXEL ENGINE | " << voxel::blockName(selected) << " | SEED " << world.seed()
                     << (unsaved ? " | UNSAVED" : "")
                     << "\nCHUNKS " << renderer.visibleChunks() << "/" << voxel::World::ChunkCount
                     << " | TRIANGLES " << renderer.drawnTriangles() << "/" << renderer.triangleCount();
                if (framesPerSecond > 0.0) text << " | FPS " << framesPerSecond << " | " << 1000.0 / framesPerSecond << " MS";
                text << "\nXYZ " << p.x << ", " << p.y << ", " << p.z
                     << " | FOV " << camera.fieldOfView() << " | UNDO " << history.undoCount()
                     << " REDO " << history.redoCount()
                     << "\n" << status << "\nH HELP | F5 SAVE | F9 RELOAD | TAB MOUSE | Q QUIT";
                if (input && !input->captured()) text << " | MOUSE RELEASED";
                if (input && input->helpVisible()) {
                    text << "\nWASD FLY | SPACE/CTRL UP/DOWN | SHIFT BOOST"
                         << "\nMOUSE LOOK | SCROLL ZOOM | R RESET | F WIREFRAME"
                         << "\nLEFT REMOVE | RIGHT PLACE | MIDDLE PICK | 1-4 MATERIAL"
                         << "\nCTRL/CMD Z UNDO | CTRL/CMD Y OR SHIFT Z REDO"
                         << "\nESC RELEASE MOUSE | EDIT REACH 8 BLOCKS";
                }
                hudText = text.str();
                titleTime = now;
            }
            float scaleX, scaleY;
            glfwGetWindowContentScale(window.handle(), &scaleX, &scaleY);
            hud.draw(hudText, width, height, scaleY);
            if (smokeTest && frames == 2) {
                voxel::verifyFrame(width, height, options.screenshot);
                frameVerified = true;
                std::cout << "Smoke test passed: " << renderer.triangleCount() << " triangles, "
                          << renderer.visibleChunks() << " visible chunks\n";
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
