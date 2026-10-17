#include "graphics_smoke.h"
#include "graphics/hud.h"
#include "graphics/opengl.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "camera.h"
#include "editing.h"
#include "game.h"
#include "input.h"
#include "save.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <vector>

namespace voxel {
namespace {

void checkOpenGl()
{
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        throw std::runtime_error("OpenGL smoke test error: " + std::to_string(error));
    }
}

void verifyOverlayState()
{
    Shader program("#version 410 core\nvoid main() { gl_Position = vec4(0); }",
                   "#version 410 core\nout vec4 color; void main() { color = vec4(1); }");
    GLuint vao = 0;
    GLuint buffer = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &buffer);
    const auto cleanup = [&] {
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDeleteBuffers(1, &buffer);
        glDeleteVertexArrays(1, &vao);
    };
    try {
        program.use();
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        GLint expectedProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &expectedProgram);
        const auto checkBindings = [&] {
            GLint activeProgram = 0, activeVao = 0, activeBuffer = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &activeVao);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &activeBuffer);
            if (activeProgram != expectedProgram || activeVao != static_cast<GLint>(vao)
                || activeBuffer != static_cast<GLint>(buffer)) {
                throw std::runtime_error("Overlay changed the caller's OpenGL bindings");
            }
        };
        Overlay overlay;
        checkBindings();
        Camera camera;
        for (const bool depthEnabled : {false, true}) {
            for (const bool depthWrites : {false, true}) {
                if (depthEnabled) glEnable(GL_DEPTH_TEST);
                else glDisable(GL_DEPTH_TEST);
                glDepthMask(depthWrites ? GL_TRUE : GL_FALSE);
                overlay.draw(RayHit{{0, 0, 0}, {0, 1, 0}, 0.0f}, camera, 640, 360);
                checkBindings();
                GLboolean depthMask = GL_TRUE;
                glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
                if ((glIsEnabled(GL_DEPTH_TEST) == GL_TRUE) != depthEnabled
                    || (depthMask == GL_TRUE) != depthWrites) {
                    throw std::runtime_error("Overlay changed the caller's depth state");
                }
            }
        }
        checkOpenGl();
    } catch (...) {
        cleanup();
        throw;
    }
    cleanup();
}

void verifyCrosshair(int width, int height)
{
    constexpr int readSize = 64;
    if (width < readSize || height < readSize) {
        throw std::runtime_error("Framebuffer is too small for the crosshair smoke test");
    }
    Overlay overlay;
    Camera camera;
    std::vector<unsigned char> pixels(readSize * readSize * 3);
    glViewport(0, 0, width, height);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    for (const float scale : {1.0f, 2.0f, 0.0f, std::numeric_limits<float>::quiet_NaN()}) {
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        overlay.draw({}, camera, width, height, scale);
        glReadPixels(width / 2 - readSize / 2, height / 2 - readSize / 2,
                     readSize, readSize, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        int white = 0, black = 0;
        int minimumX = readSize, minimumY = readSize, maximumX = -1, maximumY = -1;
        for (int y = 0; y < readSize; ++y) {
            for (int x = 0; x < readSize; ++x) {
                const auto offset = static_cast<std::size_t>((y * readSize + x) * 3);
                const bool isWhite = pixels[offset] == 255 && pixels[offset + 1] == 255
                    && pixels[offset + 2] == 255;
                const bool isBlack = pixels[offset] == 0 && pixels[offset + 1] == 0
                    && pixels[offset + 2] == 0;
                white += isWhite;
                black += isBlack;
                if (isWhite || isBlack) {
                    minimumX = std::min(minimumX, x);
                    minimumY = std::min(minimumY, y);
                    maximumX = std::max(maximumX, x);
                    maximumY = std::max(maximumY, y);
                }
            }
        }
        const int expectedScale = scale == 2.0f ? 2 : 1;
        if (white != 60 * expectedScale * expectedScale || black != 68 * expectedScale * expectedScale
            || minimumX != readSize / 2 - 9 * expectedScale || minimumY != minimumX
            || maximumX != readSize / 2 + 9 * expectedScale - 1 || maximumY != maximumX) {
            throw std::runtime_error("Crosshair lost its centered DPI-scaled white bars or dark border");
        }
        std::array<GLint, 2> polygonMode{};
        glGetIntegerv(GL_POLYGON_MODE, polygonMode.data());
        if (glIsEnabled(GL_CULL_FACE) != GL_TRUE || polygonMode[0] != GL_LINE
            || polygonMode[1] != GL_LINE) {
            throw std::runtime_error("Crosshair changed the caller's culling or polygon mode");
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    checkOpenGl();
}

void verifyRendererCaches(int width, int height)
{
    World world;
    Camera camera;
    WorldRenderer first;
    first.render(world, camera, width, height, false);
    const auto originalTriangles = first.triangleCount();
    if (world.takeDirtyChunks().size() != World::ChunkCount) {
        throw std::runtime_error("Renderer consumed the world's pending chunk updates");
    }
    WorldRenderer second;
    second.render(world, camera, width, height, false);
    if (second.triangleCount() != originalTriangles) {
        throw std::runtime_error("Second renderer failed to build an already-rendered world");
    }
    const auto checkBoth = [&](const World& scene, std::size_t triangles) {
        first.render(scene, camera, width, height, false);
        second.render(scene, camera, width, height, false);
        if (first.triangleCount() != triangles || second.triangleCount() != triangles) {
            throw std::runtime_error("Independent renderers did not refresh their cached chunk meshes");
        }
    };

    // These isolated blocks straddle x=0. Their shared face must disappear in
    // both chunk meshes, then reappear when either block is removed.
    world.set({-1, 15, 0}, Block::Stone);
    world.takeDirtyChunks();
    checkBoth(world, originalTriangles + 12);
    world.set({0, 15, 0}, Block::Stone);
    checkBoth(world, originalTriangles + 20);
    world.set({-1, 15, 0}, Block::Air);
    checkBoth(world, originalTriangles + 12);

    World replacement = world;
    replacement.set({0, 15, 0}, Block::Air);
    replacement.takeDirtyChunks();
    checkBoth(replacement, originalTriangles);
    checkBoth(world, originalTriangles + 12);
    world = replacement;
    checkBoth(world, originalTriangles);
    checkBoth(world, originalTriangles);
    checkOpenGl();
    std::cout << "Renderer caches passed: multiple views, seam edits, switching and replacing worlds\n";
}

} // namespace

void verifyGpuResources()
{
    bool rejected = false;
    try {
        Shader invalid("#version 410 core\nthis is intentionally invalid GLSL",
                       "#version 410 core\nout vec4 color; void main() { color = vec4(1); }");
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("compilation failed") != std::string::npos;
    }
    if (!rejected) throw std::runtime_error("Invalid shader was not diagnosed");

    GpuMesh mesh;
    mesh.upload({});
    mesh.draw();
    Chunk chunk;
    chunk.set(0, 0, 0, Block::Stone);
    mesh.upload(buildMesh(chunk));
    if (mesh.indexCount() != 36) throw std::runtime_error("GPU mesh upload lost indices");
    MeshData invalid;
    invalid.indices.push_back(0);
    rejected = false;
    try {
        mesh.upload(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected || mesh.indexCount() != 36) {
        throw std::runtime_error("Invalid mesh upload changed the existing GPU mesh");
    }
    mesh.upload({});
    mesh.draw();
    if (mesh.indexCount() != 0) throw std::runtime_error("Empty upload retained geometry");
    verifyOverlayState();
    checkOpenGl();
}

void verifyRenderTransitions(int width, int height)
{
    if (width <= 0 || height <= 0) throw std::runtime_error("Smoke test framebuffer is empty");
    World world;
    Camera camera;
    WorldRenderer renderer;
    EditHistory history;
    std::stringstream snapshot(std::ios::in | std::ios::out | std::ios::binary);
    encodeWorld(world, snapshot);
    renderer.render(world, camera, width, height, false);
    const auto triangles = renderer.triangleCount();
    if (renderer.visibleChunks() == 0 || triangles == 0) {
        throw std::runtime_error("Initial terrain was incorrectly culled");
    }
    // This floating block is isolated above the highest possible terrain.
    if (!history.apply(world, {0, 15, 0}, Block::Stone)) {
        throw std::runtime_error("Smoke test edit failed");
    }
    renderer.render(world, camera, width, height, false);
    if (renderer.triangleCount() != triangles + 12 || !history.undo(world)) {
        throw std::runtime_error("Edited chunk did not rebuild correctly");
    }
    renderer.render(world, camera, width, height, false);
    if (renderer.triangleCount() != triangles || !history.redo(world)) {
        throw std::runtime_error("Undo did not restore the rendered mesh");
    }
    renderer.render(world, camera, width, height, true);
    if (renderer.triangleCount() != triangles + 12) {
        throw std::runtime_error("Redo did not rebuild the rendered mesh");
    }
    world = decodeWorld(snapshot);
    renderer.render(world, camera, width, height, false);
    if (renderer.triangleCount() != triangles) {
        throw std::runtime_error("Loading a world did not refresh the GPU meshes");
    }
    camera.setPosition({200, 80, 200});
    camera.rotate(180, 0);
    renderer.render(world, camera, width, height, false);
    if (renderer.visibleChunks() != 0 || renderer.drawnTriangles() != 0) {
        throw std::runtime_error("Chunks behind the camera were not culled");
    }
    camera.reset();
    renderer.render(world, camera, width, height, false);
    verifyFrame(width, height, {});
    checkOpenGl();
    std::cout << "Render transitions passed: edit, undo, redo, reload, wireframe, culling\n";
}

void verifyFrame(int width, int height, const std::string& screenshotPath)
{
    if (width <= 0 || height <= 0) throw std::runtime_error("Smoke test framebuffer is empty");
    std::vector<float> depth(static_cast<std::size_t>(width) * height);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    checkOpenGl();
    const auto covered = std::count_if(depth.begin(), depth.end(), [](float d) { return d < 1.0f; });
    if (static_cast<std::size_t>(covered) < depth.size() / 100) {
        throw std::runtime_error("Rendering smoke test failed: missing terrain");
    }
    std::cout << "Framebuffer verified: " << covered << " terrain pixels\n";
    if (screenshotPath.empty()) return;

    const auto rowSize = static_cast<std::size_t>(width) * 3;
    std::vector<unsigned char> pixels(rowSize * height);
    GLint alignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    checkOpenGl();
    std::ofstream output(screenshotPath, std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        output.write(reinterpret_cast<const char*>(pixels.data() + y * rowSize),
                     static_cast<std::streamsize>(rowSize));
    }
    output.close();
    if (!output) throw std::runtime_error("Could not write screenshot: " + screenshotPath);
    std::cout << "Screenshot: " << screenshotPath << '\n';
}

int runGraphicsSmokeTest(const Options& options)
{
    Game game(options);
    Window window(false);
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
    verifyGpuResources();
    int width, height;
    window.framebufferSize(width, height);
    verifyCrosshair(width, height);
    verifyRenderTransitions(width, height);
    verifyRendererCaches(width, height);

    // GPU resources are scoped after Window so their destructors run while its
    // context is still current. Smoke tests never capture the user's mouse.
    WorldRenderer renderer;
    Overlay overlay;
    Hud hud;
    bool frameVerified = false;
    for (int frame = 0; frame < 3 && !window.shouldClose(); ++frame) {
        glfwPollEvents();
        game.update({});
        window.framebufferSize(width, height);
        if (width <= 0 || height <= 0) {
            throw std::runtime_error("Smoke test framebuffer is empty");
        }
        renderer.render(game.world, game.camera, width, height, false);
        float scaleX, scaleY;
        glfwGetWindowContentScale(window.handle(), &scaleX, &scaleY);
        overlay.draw(game.target, game.camera, width, height, scaleY);
        hud.draw(game.statusText(renderer.visibleChunks(), renderer.drawnTriangles(),
                                 renderer.triangleCount(), 0.0, true, false), width, height, scaleY);
        if (frame == 2) {
            verifyFrame(width, height, options.screenshot);
            frameVerified = true;
            std::cout << "Smoke test passed: " << renderer.triangleCount() << " triangles, "
                      << renderer.visibleChunks() << " visible chunks\n";
        }
        window.present();
    }
    if (!frameVerified) {
        throw std::runtime_error("Window closed before the smoke test completed");
    }
    return EXIT_SUCCESS;
}

} // namespace voxel
