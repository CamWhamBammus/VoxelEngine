#include "graphics_smoke.h"
#include "graphics/hud.h"
#include "graphics/opengl.h"
#include "graphics/post_process.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "camera.h"
#include "editing.h"
#include "game.h"
#include "input.h"
#include "save.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <utility>
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

void verifyPostProcessing()
{
    constexpr int width = 320, height = 180;
    Shader callerProgram("#version 410 core\nvoid main() { gl_Position = vec4(0); }",
                         "#version 410 core\nout vec4 color; void main() { color = vec4(1); }");
    PostProcess post;
    GLuint frame = 0, color = 0, depth = 0, vao = 0;
    std::array<GLuint, 3> callerTextures{};
    glGenFramebuffers(1, &frame);
    glGenTextures(1, &color);
    glGenRenderbuffers(1, &depth);
    glGenVertexArrays(1, &vao);
    glGenTextures(3, callerTextures.data());
    const auto cleanup = [&] {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        for (int unit = 0; unit < 3; ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glEnable(GL_MULTISAMPLE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDeleteFramebuffers(1, &frame);
        glDeleteTextures(1, &color);
        glDeleteRenderbuffers(1, &depth);
        glDeleteVertexArrays(1, &vao);
        glDeleteTextures(3, callerTextures.data());
    };
    try {
        glBindFramebuffer(GL_FRAMEBUFFER, frame);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        // A different depth format catches accidental framebuffer depth blits.
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Post-processing test framebuffer is incomplete");
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        callerProgram.use();
        glBindVertexArray(vao);
        GLint expectedProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &expectedProgram);
        for (int unit = 0; unit < 3; ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, callerTextures[unit]);
        }
        glActiveTexture(GL_TEXTURE3);
        glViewport(7, 9, 123, 87);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_GREATER);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_SCISSOR_TEST);
        glEnable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_MULTISAMPLE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        const auto checkState = [&] {
            const std::array<std::pair<GLenum, GLint>, 7> bindings{{
                {GL_DRAW_FRAMEBUFFER_BINDING, static_cast<GLint>(frame)},
                {GL_READ_FRAMEBUFFER_BINDING, 0}, {GL_CURRENT_PROGRAM, expectedProgram},
                {GL_VERTEX_ARRAY_BINDING, static_cast<GLint>(vao)},
                {GL_RENDERBUFFER_BINDING, static_cast<GLint>(depth)},
                {GL_ACTIVE_TEXTURE, GL_TEXTURE3}, {GL_DEPTH_FUNC, GL_GREATER}}};
            for (const auto& [name, expected] : bindings) {
                GLint actual = 0;
                glGetIntegerv(name, &actual);
                if (actual != expected) throw std::runtime_error("Post-processing changed an OpenGL binding");
            }
            std::array<GLint, 4> viewport{};
            std::array<GLint, 2> polygon{};
            GLboolean depthWrite = GL_TRUE;
            glGetIntegerv(GL_VIEWPORT, viewport.data());
            glGetIntegerv(GL_POLYGON_MODE, polygon.data());
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
            if (viewport != std::array<GLint, 4>{7, 9, 123, 87}
                || polygon[0] != GL_LINE || polygon[1] != GL_LINE || depthWrite
                || glIsEnabled(GL_DEPTH_TEST) || glIsEnabled(GL_MULTISAMPLE)
                || !glIsEnabled(GL_CULL_FACE) || !glIsEnabled(GL_BLEND)
                || !glIsEnabled(GL_SCISSOR_TEST) || !glIsEnabled(GL_FRAMEBUFFER_SRGB)) {
                throw std::runtime_error("Post-processing changed the caller's rendering state");
            }
            for (int unit = 0; unit < 3; ++unit) {
                GLint texture = 0;
                glActiveTexture(GL_TEXTURE0 + unit);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
                if (texture != static_cast<GLint>(callerTextures[unit])) {
                    throw std::runtime_error("Post-processing changed a texture binding");
                }
            }
            glActiveTexture(GL_TEXTURE3);
            checkOpenGl();
        };
        const auto draw = [&](float emission, int renderWidth = width, int renderHeight = height) {
            post.begin(renderWidth, renderHeight);
            GLint samples = 0, maximumSamples = 0;
            glGetIntegerv(GL_SAMPLES, &samples);
            glGetIntegerv(GL_MAX_SAMPLES, &maximumSamples);
            if (samples < 2 || samples != std::min(4, maximumSamples)
                || !glIsEnabled(GL_MULTISAMPLE)) {
                throw std::runtime_error("HDR scene target did not enable multisample antialiasing");
            }
            const std::array<float, 4> background{0, 0, 0, 0};
            const std::array<float, 4> glowing{2.5f, 0.6f, 0.1f, emission};
            const float farDepth = 1, surfaceDepth = 0.4f;
            glClearBufferfv(GL_COLOR, 0, background.data());
            glClearBufferfv(GL_DEPTH, 0, &farDepth);
            glEnable(GL_SCISSOR_TEST);
            glScissor(renderWidth / 2 - 32, renderHeight / 2 - 32, 64, 64);
            glClearBufferfv(GL_COLOR, 0, glowing.data());
            glClearBufferfv(GL_DEPTH, 0, &surfaceDepth);
            post.finish();
            checkState();
        };
        const auto read = [&](int x, int y) {
            std::array<unsigned char, 4> pixel{};
            glBindFramebuffer(GL_READ_FRAMEBUFFER, frame);
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            return pixel;
        };
        draw(0);
        const auto unlitOutside = read(width / 2 + 40, height / 2);
        const auto unlitCenter = read(width / 2, height / 2);
        draw(1);
        const auto glowingOutside = read(width / 2 + 40, height / 2);
        const auto glowingCenter = read(width / 2, height / 2);
        if (unlitOutside[0] > 1 || glowingOutside[0] < unlitOutside[0] + 8
            || glowingOutside[0] <= glowingOutside[2] || unlitCenter[0] < 200
            || glowingCenter[0] < unlitCenter[0]) {
            throw std::runtime_error("Emission-tagged HDR surfaces did not produce a warm, bounded bloom");
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, frame);
        float centerDepth = 0, outsideDepth = 0;
        glReadPixels(width / 2, height / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &centerDepth);
        glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &outsideDepth);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        if (std::abs(centerDepth - 0.4f) > 0.001f || std::abs(outsideDepth - 1) > 0.001f) {
            throw std::runtime_error("HDR composition did not preserve scene depth for selection outlines");
        }
        const auto rejects = [&](const auto& action) {
            bool rejected = false;
            try { action(); } catch (const std::logic_error&) { rejected = true; }
            if (!rejected) throw std::runtime_error("Post-processing accepted an invalid frame transition");
        };
        rejects([&] { post.finish(); });
        rejects([&] { post.begin(0, height); });
        rejects([&] { post.begin(width, -1); });
        GLint maximumSize = 0;
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maximumSize);
        rejects([&] { post.begin(maximumSize + 1, height); });
        checkState();
        post.begin(width, height);
        rejects([&] { post.begin(width, height); });
        post.finish();
        checkState();
        draw(1, width / 2, height / 2);
        draw(1);
        checkOpenGl();
    } catch (...) {
        cleanup();
        throw;
    }
    cleanup();
    std::cout << "HDR processing passed: emission bloom, scene depth, resize, guards and GL state\n";
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

void verifyUndergroundMaterials(int width, int height)
{
    World world;
    for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
                world.set({x, y, z}, Block::Air);
            }
        }
    }
    Camera camera;
    camera.rotate(35, 25); // Look straight at the front of the test block.
    camera.setPosition({0.5f, 6.5f, 6.5f});
    WorldRenderer renderer;
    GLuint callerTexture = 0;
    glGenTextures(1, &callerTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, callerTexture);
    glActiveTexture(GL_TEXTURE3);
    constexpr int patchSize = 128;
    std::array<unsigned char, patchSize * patchSize * 4> pixels{};
    const auto sample = [&](Block material) {
        world.set({0, 6, 0}, material);
        renderer.render(world, camera, width, height, false, true);
        checkOpenGl();
        GLint activeTexture = 0, texture = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &texture);
        glActiveTexture(GL_TEXTURE3);
        if (activeTexture != GL_TEXTURE3 || texture != static_cast<GLint>(callerTexture)) {
            throw std::runtime_error("Voxel lighting changed the caller's texture bindings");
        }
        glReadPixels(width / 2 - patchSize / 2, height / 2 - patchSize / 2,
                     patchSize, patchSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glm::vec3 average(0);
        for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
            average += glm::vec3(pixels[pixel], pixels[pixel + 1], pixels[pixel + 2]);
        }
        return average / static_cast<float>(patchSize * patchSize);
    };
    const auto stone = sample(Block::Stone);
    if (stone.x < 40 || stone.y < 40 || stone.z < 40) {
        throw std::runtime_error("Headlamp left nearby underground stone unreadable");
    }
    unsigned char minimum = 255, maximum = 0;
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        minimum = std::min(minimum, pixels[pixel]);
        maximum = std::max(maximum, pixels[pixel]);
    }
    if (maximum - minimum < 5) throw std::runtime_error("Stone lost its voxel-space pixel grain");
    sample(Block::CopperOre);
    std::size_t copperFlecks = 0;
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        copperFlecks += pixels[pixel] > pixels[pixel + 1] * 1.2f
            && pixels[pixel] > pixels[pixel + 2] * 1.3f;
    }
    sample(Block::CoalOre);
    std::size_t coalFlecks = 0;
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        coalFlecks += pixels[pixel] < stone.x * 0.6f;
    }
    const auto planks = sample(Block::Planks);
    const auto brick = sample(Block::Brick);
    if (copperFlecks < patchSize || coalFlecks < patchSize
        || (planks.x - planks.z) - (stone.x - stone.z) < 20
        || (brick.x - brick.y) - (stone.x - stone.y) < 15) {
        throw std::runtime_error("Ore, wood and brick materials lost their distinct surface colors: "
            + std::to_string(copperFlecks) + " copper, " + std::to_string(coalFlecks)
            + " coal, wood contrast " + std::to_string(planks.x - planks.z)
            + ", brick contrast " + std::to_string(brick.x - brick.y));
    }
    const auto lantern = sample(Block::Lantern);
    std::size_t glass = 0, metal = 0;
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        glass += pixels[pixel] > 220 && pixels[pixel + 1] > 150 && pixels[pixel + 2] < 150;
        // Bloom lifts the grille slightly; it must retain strong contrast
        // against the glass rather than meet a pre-HDR absolute byte value.
        metal += pixels[pixel] < lantern.x - 100;
    }
    const auto crystal = sample(Block::Crystal);
    if (renderer.triangleCount() <= 12) {
        throw std::runtime_error("Crystal detail geometry did not render in the underground view");
    }
    if (glass < patchSize || metal < patchSize || crystal.z < 120 || crystal.y < 100
        || crystal.z - crystal.x < 30) {
        throw std::runtime_error("Lantern or crystal lost its emissive surface: glass "
            + std::to_string(glass) + ", metal " + std::to_string(metal)
            + ", lantern RGB " + std::to_string(lantern.x) + "/" + std::to_string(lantern.y)
            + "/" + std::to_string(lantern.z) + ", crystal RGB " + std::to_string(crystal.x)
            + "/" + std::to_string(crystal.y) + "/" + std::to_string(crystal.z));
    }
    sample(Block::Stone);
    if (renderer.triangleCount() != 12) {
        throw std::runtime_error("Replacing a crystal retained stale decorative geometry");
    }

    // Light edits must refresh the source cache, even with the same renderer.
    world.set({2, 7, 3}, Block::Lantern);
    const auto warm = sample(Block::Stone);
    // This wall crosses the light ray but stays outside both the camera ray
    // and the surface's contact-shading neighborhood.
    for (int x = 1; x <= 3; ++x) {
        for (int y = 5; y <= 9; ++y) world.set({x, y, 2}, Block::Stone);
    }
    const auto blocked = sample(Block::Stone);
    for (int x = 1; x <= 3; ++x) {
        for (int y = 5; y <= 9; ++y) world.set({x, y, 2}, Block::Air);
    }
    const auto reopened = sample(Block::Stone);
    if (blocked.x > warm.x - 10 || glm::length(blocked - stone) > 2.0f
        || glm::length(reopened - warm) > 1.0f) {
        throw std::runtime_error("Voxel wall failed to block light or reopening failed to restore it");
    }
    world.set({2, 7, 3}, Block::Crystal);
    const auto cool = sample(Block::Stone);
    world.set({2, 7, 3}, Block::Air);
    const auto removed = sample(Block::Stone);
    if (warm.x < stone.x + 5 || warm.x - warm.z < stone.x - stone.z + 8
        || cool.z < stone.z + 5 || cool.z - cool.x < stone.z - stone.x + 8
        || glm::length(removed - stone) > 1.0f) {
        throw std::runtime_error("Underground light colors or edit invalidation failed");
    }
    for (int x = 1; x <= 3; ++x) {
        for (int y = 5; y <= 8; ++y) world.set({x, y, 3}, Block::Crystal);
    }
    const auto pooled = sample(Block::Stone);
    if (pooled.x > 220 || pooled.z > 245 || pooled.z < pooled.x + 10) {
        throw std::runtime_error("Clustered crystals washed out the surrounding rock");
    }
    for (int x = 1; x <= 3; ++x) {
        for (int y = 5; y <= 8; ++y) world.set({x, y, 3}, Block::Air);
    }
    camera.setPosition({0.78f, 9.5f, 0.5f});
    camera.rotate(0, -89);
    const auto floor = sample(Block::Stone);
    world.set({1, 7, 0}, Block::Stone);
    const auto contact = sample(Block::Stone);
    if (floor.x < 40 || contact.x < 30 || contact.x > floor.x - 3) {
        throw std::runtime_error("Floor contact shading lost its readable, darker corner");
    }
    std::array<unsigned char, 4> background{};
    glReadPixels(0, height - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, background.data());
    // Linear cave color is tone-mapped and gamma encoded; it should still be
    // dark violet-blue, clearly separate from the editor's daylight sky.
    if (background[0] > 80 || background[1] > 80 || background[2] > 80
        || background[2] < background[0] + 8) {
        throw std::runtime_error("Underground scene retained the outdoor sky");
    }
    renderer.render(world, camera, width, height, false);
    glReadPixels(0, height - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, background.data());
    if (background[0] < 100 || background[2] < 180) {
        throw std::runtime_error("Outdoor editor lost its sky when switching render modes");
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glDeleteTextures(1, &callerTexture);
    checkOpenGl();
    std::cout << "Underground materials passed: grain, ores, wood, emission, wall shadows and light edits\n";
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

    // A destroyed bound program must not remain pending deletion: another
    // pass cannot safely save it, bind a new program, then restore the old ID.
    {
        Shader temporary("#version 410 core\nvoid main() { gl_Position = vec4(0); }",
                         "#version 410 core\nout vec4 color; void main() { color = vec4(1); }");
        temporary.use();
    }
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    if (currentProgram != 0) throw std::runtime_error("Destroyed shader remained bound for a later rendering pass");

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
    for (std::size_t id = 1; id < BlockCount; ++id) {
        chunk.set(0, 0, 0, static_cast<Block>(id));
        const auto materialMesh = buildMesh(chunk);
        for (const auto& vertex : materialMesh.vertices) {
            if (vertex.material != static_cast<float>(id)) {
                throw std::runtime_error("Voxel material ID was lost while meshing");
            }
        }
        mesh.upload(materialMesh);
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
    verifyPostProcessing();
    int width, height;
    window.framebufferSize(width, height);
    verifyCrosshair(width, height);
    verifyRenderTransitions(width, height);
    verifyRendererCaches(width, height);
    verifyUndergroundMaterials(width, height);

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
        renderer.render(game.world, game.camera, width, height, false, !game.editorMode());
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
