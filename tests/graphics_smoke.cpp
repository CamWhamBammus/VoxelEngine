#include "graphics_smoke.h"
#include "water_graphics_smoke.h"
#include "hud_play_smoke.h"
#include "held_sword_smoke.h"
#include "held_pickaxe_smoke.h"
#include "ore_material_smoke.h"
#include "surface_relief_smoke.h"
#include "material_continuity_smoke.h"
#include "mining_hud_smoke.h"
#include "damage_feedback_smoke.h"
#include "inventory_hud_smoke.h"
#include "root_crossing_hud_smoke.h"
#include "section_hud_smoke.h"
#include "palette_preview_smoke.h"
#include "level_hud_smoke.h"
#include "lush_material_smoke.h"
#include "graphics/hud.h"
#include "graphics/opengl.h"
#include "graphics/post_process.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "graphics/cave_details.h"
#include "graphics/block_relief.h"
#include "graphics/voxel_lighting.h"
#include "hub_assets.h"
#include "camera.h"
#include "editing.h"
#include "game.h"
#include "input.h"
#include "save.h"
#include "scene.h"
#include "root_crossings.h"

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
    GLuint frame = 0, color = 0, depth = 0, vao = 0, snapshotFrame = 0;
    std::array<GLuint, 3> callerTextures{};
    glGenFramebuffers(1, &frame);
    glGenFramebuffers(1, &snapshotFrame);
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
        glDeleteFramebuffers(1, &snapshotFrame);
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
        const auto verifySnapshot = [&](int renderWidth, int renderHeight) {
            post.begin(renderWidth, renderHeight);
            const std::array<float, 4> opaque{0.1f,0.2f,0.8f,0};
            const std::array<float, 4> later{0.8f,0.1f,0.05f,0};
            const float opaqueDepth = 0.75f, laterDepth = 0.25f;
            glClearBufferfv(GL_COLOR, 0, opaque.data());
            glClearBufferfv(GL_DEPTH, 0, &opaqueDepth);
            GLint sceneFrame = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &sceneFrame);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            for (int unit = 0; unit < 3; ++unit) {
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, callerTextures[unit]);
            }
            glActiveTexture(GL_TEXTURE3);
            GLint texture3 = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture3);
            glEnable(GL_SCISSOR_TEST);
            glScissor(1,2,1,1);
            post.captureScene();
            GLint actualDraw = 0, actualRead = 0, actualActive = 0;
            std::array<GLint, 4> scissor{};
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &actualDraw);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &actualRead);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &actualActive);
            glGetIntegerv(GL_SCISSOR_BOX, scissor.data());
            if (actualDraw != sceneFrame || actualRead != 0 || actualActive != GL_TEXTURE3
                || !glIsEnabled(GL_SCISSOR_TEST) || scissor != std::array<GLint,4>{1,2,1,1}) {
                throw std::runtime_error("Opaque scene capture changed framebuffer or scissor state");
            }
            for (int unit = 0; unit < 4; ++unit) {
                glActiveTexture(GL_TEXTURE0 + unit);
                GLint binding = 0;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
                const auto expected = unit < 3 ? static_cast<GLint>(callerTextures[unit]) : texture3;
                if (binding != expected) throw std::runtime_error("Opaque scene capture changed a texture binding");
            }
            glActiveTexture(GL_TEXTURE3);
            if (!post.sceneColor() || !post.sceneDepth() || post.sceneColor() == post.sceneDepth())
                throw std::runtime_error("Opaque scene capture did not expose distinct color and depth textures");
            const auto checkSnapshot = [&](const std::array<float,4>& expectedColor, float expectedDepth) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, snapshotFrame);
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, post.sceneColor(), 0);
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, post.sceneDepth(), 0);
                if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    throw std::runtime_error("Opaque scene snapshot attachments are incomplete");
                // Read both corners and the center: capture must resolve the
                // full target despite a tiny active scissor or a recent resize.
                for (const auto pixel : {glm::ivec2(0,0), glm::ivec2(renderWidth / 2,renderHeight / 2),
                                         glm::ivec2(renderWidth - 1,renderHeight - 1)}) {
                    std::array<float, 4> actualColor{};
                    float actualDepth = 0;
                    glReadPixels(pixel.x, pixel.y, 1, 1, GL_RGBA, GL_FLOAT, actualColor.data());
                    glReadPixels(pixel.x, pixel.y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &actualDepth);
                    for (int channel = 0; channel < 4; ++channel)
                        if (std::abs(actualColor[channel] - expectedColor[channel]) > 0.001f)
                            throw std::runtime_error("Opaque scene snapshot contains the wrong color");
                    if (std::abs(actualDepth - expectedDepth) > 0.001f)
                        throw std::runtime_error("Opaque scene snapshot contains the wrong depth");
                }
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            };
            checkSnapshot(opaque, opaqueDepth);
            glDisable(GL_SCISSOR_TEST);
            post.captureScene();
            if (glIsEnabled(GL_SCISSOR_TEST))
                throw std::runtime_error("Repeated scene capture enabled scissoring");
            glClearBufferfv(GL_COLOR, 0, later.data());
            glClearBufferfv(GL_DEPTH, 0, &laterDepth);
            checkSnapshot(opaque, opaqueDepth);
            post.finish();
            checkState();
            checkSnapshot(later, laterDepth);
            const auto finalColor = read(renderWidth / 2, renderHeight / 2);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, frame);
            float finalDepth = 0;
            glReadPixels(renderWidth / 2, renderHeight / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &finalDepth);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            if (finalColor[0] < finalColor[2] + 50 || std::abs(finalDepth - laterDepth) > 0.001f)
                throw std::runtime_error("HDR finish did not include drawing after the opaque snapshot");
            checkOpenGl();
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
        rejects([&] { post.captureScene(); });
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
        verifySnapshot(width / 2, height / 2);
        verifySnapshot(width, height);
        rejects([&] { post.captureScene(); });
        checkState();
        draw(1);
        checkOpenGl();
    } catch (...) {
        cleanup();
        throw;
    }
    cleanup();
    std::cout << "HDR processing passed: opaque snapshots, emission bloom, scene depth, resize, guards and GL state\n";
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
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    for (const auto size : {glm::ivec2(width,height),glm::ivec2(640,360)}) {
        glViewport(0,0,size.x,size.y);
        for (const float scale : {1.0f, 2.0f, 0.0f, std::numeric_limits<float>::quiet_NaN()}) {
            glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            overlay.draw({}, camera, size.x, size.y, scale);
            glReadPixels(size.x / 2 - readSize / 2, size.y / 2 - readSize / 2,
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
            const int expectedScale = std::min(scale == 2.0f ? 2 : 1,std::max(1,std::min(size.x/640,size.y/360)));
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
    }
    glViewport(0,0,width,height);
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
    // Both sides of a vertical seam must refresh in the taller chunk grid.
    world.set({2, 31, 2}, Block::Planks);
    checkBoth(world, originalTriangles + 12);
    world.set({2, 32, 2}, Block::Brick);
    checkBoth(world, originalTriangles + 20);
    world.set({2, 31, 2}, Block::Air);
    checkBoth(world, originalTriangles + 12);
    world.set({2, 32, 2}, Block::Air);
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
    GLuint callerTexture = 0, callerLightTexture = 0;
    glGenTextures(1, &callerTexture);
    glGenTextures(1, &callerLightTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, callerTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, callerLightTexture);
    glActiveTexture(GL_TEXTURE3);
    // Sample the same part of the block at 1x and Retina framebuffer scales.
    // A fixed 128px patch at 720p also samples the dark background around it.
    const int patchSize = std::clamp(height * 128 / 1440, 32, 128);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(patchSize) * patchSize * 4);
    const auto sample = [&](Block material, float seconds = 0.0f, bool lantern = false) {
        world.set({0, 6, 0}, material);
        renderer.render(world, camera, width, height, false, true, seconds, lantern);
        checkOpenGl();
        GLint activeTexture = 0, texture = 0, lightTexture = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &texture);
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &lightTexture);
        glActiveTexture(GL_TEXTURE3);
        if (activeTexture != GL_TEXTURE3 || texture != static_cast<GLint>(callerTexture)
            || lightTexture != static_cast<GLint>(callerLightTexture)) {
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
    const auto carried = sample(Block::Stone, 0.0f, true);
    if (carried.x < stone.x + 12 || (carried.x - stone.x) < (carried.z - stone.z) + 5
        || glm::length(sample(Block::Stone) - stone) > 1.0f) {
        throw std::runtime_error("Equipped lantern lost its warm light or stayed on after switching tools");
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
    sample(Block::Dirt); // A plain replacement must remove all crystal detail.
    if (renderer.triangleCount() != 12) {
        throw std::runtime_error("Replacing a crystal retained stale decorative geometry");
    }

    const auto water = sample(Block::Water);
    const auto stillWater = pixels;
    sample(Block::Water, 1.5f);
    std::size_t movingPixels = 0;
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        movingPixels += std::abs(int(pixels[pixel + 1]) - int(stillWater[pixel + 1])) > 3;
    }
    if (water.y < water.x + 20 || water.z < water.x + 20
        || movingPixels < patchSize || renderer.triangleCount() != 12) {
        throw std::runtime_error("Water lost its rendered turquoise surface or flowing ribbons");
    }
    const auto leaves = sample(Block::Leaves);
    const auto glowLeaves = sample(Block::GlowLeaves);
    const auto roof = sample(Block::RoofTiles);
    if (leaves.y < leaves.x + 10 || leaves.y < leaves.z + 5
        || glowLeaves.y < leaves.y + 20 || glowLeaves.z < glowLeaves.x + 15
        || roof.z < roof.x + 8 || roof.y < roof.x + 5) {
        throw std::runtime_error("Foliage glow or blue-teal roof materials lost their distinct colors");
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
        for (int y = 5; y <= 9; ++y) world.set({x, y, 2}, Block::Water);
    }
    const auto throughWater = sample(Block::Stone);
    if (glm::length(throughWater - warm) > 2.0f) {
        throw std::runtime_error("Non-solid water incorrectly cast an opaque voxel shadow");
    }
    for (int x = 1; x <= 3; ++x) {
        for (int y = 5; y <= 9; ++y) world.set({x, y, 2}, Block::Leaves);
    }
    const auto throughLeaves = sample(Block::Stone);
    if (glm::length(throughLeaves - warm) > 2.0f) {
        throw std::runtime_error("Cutout foliage incorrectly cast a solid cube shadow");
    }
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
    world.set({2, 7, 3}, Block::GlowLeaves);
    const auto green = sample(Block::Stone);
    world.set({2, 7, 3}, Block::Air);
    const auto removed = sample(Block::Stone);
    if (warm.x < stone.x + 5 || warm.x - warm.z < stone.x - stone.z + 8
        || cool.z < stone.z + 5 || cool.z - cool.x < stone.z - stone.x + 8
        || green.y < stone.y + 5 || green.y - green.x < stone.y - stone.x + 5
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
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &callerLightTexture);
    glActiveTexture(GL_TEXTURE0);
    checkOpenGl();
    std::cout << "Underground materials passed: grain, ores, wood, emission, wall shadows and light edits\n";
}

// Run the production GLSL against a tiny synthetic occupancy texture. Exact
// geometry probes catch half-step shading defects without depending on paint,
// tone mapping, framebuffer scale, or a driver's anti-aliasing pattern.
void verifyVoxelShapeLighting()
{
    constexpr const char* vertex = R"glsl(#version 410 core
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0, 1);
}
)glsl";
    const auto fragment = std::string(R"glsl(#version 410 core
uniform sampler3D occupancy;
uniform vec3 worldMinimum;
uniform vec3 worldPosition;
uniform vec3 surfaceNormal;
uniform vec3 lightPosition;
uniform int decorative;
const vec4 reliefSurface = vec4(0);
out vec4 color;
)glsl") + voxelLightingShader + R"glsl(
void main() {
    color = vec4(contactShade(), visibleLight(worldPosition, lightPosition),
                 float(occupiedAt(worldPosition)), 1);
}
)glsl";
    Shader shader(vertex, fragment.c_str());
    GLuint frame = 0, color = 0, occupancy = 0, vao = 0;
    glGenFramebuffers(1, &frame);
    glGenTextures(1, &color);
    glGenTextures(1, &occupancy);
    glGenVertexArrays(1, &vao);
    const auto cleanup = [&] {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteFramebuffers(1, &frame);
        glDeleteTextures(1, &color);
        glDeleteTextures(1, &occupancy);
        glDeleteVertexArrays(1, &vao);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    };
    try {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, frame);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Voxel lighting probe framebuffer is incomplete");
        glBindTexture(GL_TEXTURE_3D, occupancy);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glViewport(0, 0, 1, 1);
        glBindVertexArray(vao);
        shader.use();
        shader.setInt("occupancy", 0);
        shader.setInt("decorative", 0);
        shader.setVec3("worldMinimum", {});
        shader.setVec3("lightPosition", {4.5f, 4.5f, 4.5f});
        constexpr int size = 5;
        std::array<unsigned char, size * size * size> voxels{};
        const auto set = [&](glm::ivec3 p, unsigned char code) {
            voxels[static_cast<std::size_t>(p.x + size * (p.y + size * p.z))] = code;
            // Width five requires byte-aligned rows when uploading R8 data.
            GLint alignment = 4;
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, size, size, size, 0, GL_RED,
                         GL_UNSIGNED_BYTE, voxels.data());
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        };
        const auto sample = [&](glm::vec3 point, glm::vec3 normal) {
            shader.setVec3("worldPosition", point);
            shader.setVec3("surfaceNormal", normal);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glm::vec4 result{};
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, &result[0]);
            checkOpenGl();
            return result;
        };
        for (int facing = 0; facing < 4; ++facing) {
            set({2, 2, 2}, static_cast<unsigned char>(200 + facing));
            const auto rotate = [facing](glm::vec3 point) {
                point -= glm::vec3(2.5f, 0, 2.5f);
                for (int turn = 0; turn < facing; ++turn)
                    point = {point.z, point.y, -point.x};
                return point + glm::vec3(2.5f, 0, 2.5f);
            };
            const auto low = rotate({2.5f, 2.5f, 2.55f});
            const auto high = rotate({2.5f, 3.0f, 2.25f});
            const float crease = sample(low, {0, 1, 0}).x;
            const float upper = sample(high, {0, 1, 0}).x;
            if (crease > 0.85f || upper < 0.999f)
                throw std::runtime_error("Stair contact shading missed its inner riser or darkened its open top");
            if (sample(low + glm::vec3(0, 0.1f, 0), {0, 1, 0}).z != 0.0f
                || sample(high - glm::vec3(0, 0.1f, 0), {0, 1, 0}).z != 1.0f)
                throw std::runtime_error("Stair lighting occupancy disagrees with its rotated half-step geometry");
        }
        set({2, 2, 2}, 0);
        // The upper half of the neighboring stair faces away from this wall.
        // Its lower slab may shade the bottom corner, but its air must stay open.
        set({2, 2, 3}, 202);
        const float openHalf = sample({1.9f, 2.9f, 3.0f}, {0, 0, 1}).x;
        set({2, 2, 3}, 200);
        const float filledHalf = sample({1.9f, 2.9f, 3.0f}, {0, 0, 1}).x;
        set({2, 2, 3}, 255);
        const float fullCube = sample({1.9f, 2.9f, 3.0f}, {0, 0, 1}).x;
        if (openHalf < filledHalf + 0.15f || std::abs(filledHalf - fullCube) > 0.001f)
            throw std::runtime_error("Stair contact shading treats an empty half as solid or differs from matching cube faces");
        set({2, 2, 3}, 0);
        const auto visibility = [&](glm::vec3 start, glm::vec3 end) {
            shader.setVec3("lightPosition", end);
            return sample(start, {0, 1, 0}).y;
        };
        // A diagonal ray only touches these side cells at one edge. The same
        // geometry must transmit light whether the side cell is a cube or stair.
        for (int axis = 0; axis < 3; ++axis) {
            const int other = (axis + 1) % 3;
            glm::vec3 start(2.75f), end(2.75f);
            start[axis] = start[other] = 0.5f;
            end[axis] = end[other] = 4.5f;
            for (int side = 0; side < 2; ++side) {
                glm::ivec3 tangent = glm::ivec3(glm::floor(start));
                tangent[side == 0 ? axis : other] += 1;
                for (const unsigned char code : {255, 200, 201, 202, 203}) {
                    set(tangent, code);
                    if (visibility(start, end) != 1.0f || visibility(end, start) != 1.0f)
                        throw std::runtime_error("A zero-length voxel edge crossing incorrectly blocked light");
                }
                set(tangent, 0);
            }
            glm::ivec3 wall = glm::ivec3(glm::floor(start));
            wall[axis] += 1;
            wall[other] += 1;
            set(wall, 255);
            if (visibility(start, end) != 0.0f || visibility(end, start) != 0.0f)
                throw std::runtime_error("Diagonal light escaped a full block it actually crossed");
            set(wall, 0);
        }
        // The high half must cast a shadow, while its missing half transmits.
        for (int facing = 0; facing < 4; ++facing) {
            set({2, 2, 2}, static_cast<unsigned char>(200 + facing));
            const int across = facing % 2 == 0 ? 0 : 2;
            const int along = facing % 2 == 0 ? 2 : 0;
            glm::vec3 start(2.75f), end(2.75f);
            start[across] = 0.5f;
            end[across] = 4.5f;
            start[along] = end[along] = facing < 2 ? 2.75f : 2.25f;
            if (visibility(start, end) != 1.0f)
                throw std::runtime_error("The missing upper half of a stair blocked light");
            start[along] = end[along] = facing < 2 ? 2.25f : 2.75f;
            if (visibility(start, end) != 0.0f)
                throw std::runtime_error("A stair's occupied upper half failed to block light");
        }
        cleanup();
    } catch (...) {
        cleanup();
        throw;
    }
    std::cout << "Voxel shape lighting passed: stair contacts, empty halves, diagonal edges and solid shadows\n";
}

void verifyStableWorldLighting(int width, int height)
{
    // Repeat across horizontal and vertical chunk seams. The upper target is
    // at (15,31,0), while its lamp is at (17,32,3) in both neighboring axes.
    for (const auto location : std::array<glm::ivec2, 3>{{{0, 0}, {Chunk::Size - 1, 0}, {Chunk::Size - 1, 25}}}) {
        const int originX = location.x;
        World world;
        for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
            for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
                for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
                    world.set({x, y, z}, Block::Air);
                }
            }
        }
        const glm::ivec3 origin(originX, location.y, 0);
        const glm::ivec3 lamp = origin + glm::ivec3(2, 7, 3);
        world.set(origin + glm::ivec3(0, 6, 0), Block::Stone);
        world.set(lamp, Block::Lantern);
        // These sources compete with the lamp when selection follows the
        // viewer. At camera z=6.75 the lamp and last crystal are equidistant.
        const std::array<glm::ivec3, 6> competitors{{{-3,6,7}, {0,9,7}, {3,6,7},
                                                    {0,3,7}, {0,7,9}, {-2,8,9}}};
        for (const auto position : competitors) {
            world.set(origin + position, Block::Crystal);
        }
        if (originX != 0) {
            // Many distant sources must neither consume this surface's lights
            // nor break packed light storage when its data spans multiple rows.
            for (int x = -31; x <= -17; x += 2) {
                for (int z = -31; z <= -17; z += 2) {
                    for (int y : {2, 5, 8, 11}) world.set({x, y, z}, Block::Lantern);
                }
            }
        }
        Camera camera;
        camera.rotate(35, 25); // Keep the same front-face center in view.
        WorldRenderer renderer;
        constexpr int patchSize = 24;
        std::array<unsigned char, patchSize * patchSize * 4> pixels{};
        const auto sample = [&](float distance) {
            camera.setPosition({originX + 0.5f, origin.y + 6.5f, distance});
            renderer.render(world, camera, width, height, false, true);
            glReadPixels(width / 2 - patchSize / 2, height / 2 - patchSize / 2,
                         patchSize, patchSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            checkOpenGl();
            glm::vec3 average(0);
            for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
                average += glm::vec3(pixels[pixel], pixels[pixel + 1], pixels[pixel + 2]);
            }
            return average / static_cast<float>(patchSize * patchSize);
        };
        const auto beforeStep = sample(6.74f);
        const auto afterStep = sample(6.76f);
        const auto distant = sample(20.5f);
        world.set(lamp, Block::Air);
        const auto distantUnlit = sample(20.5f);
        const auto nearUnlit = sample(6.76f);
        const auto movement = glm::abs(afterStep - beforeStep);
        const auto nearContribution = afterStep - nearUnlit;
        const auto distantContribution = distant - distantUnlit;
        if (std::max({movement.x, movement.y, movement.z}) > 4
            || nearContribution.x < 10 || nearContribution.x - nearContribution.z < 8
            || distantContribution.x < 10 || distantContribution.x - distantContribution.z < 8) {
            throw std::runtime_error("World light changed with the viewer or vanished at distance: target x="
                + std::to_string(originX) + ", step RGB drift " + std::to_string(movement.x)
                + "/" + std::to_string(movement.y) + "/" + std::to_string(movement.z)
                + ", lamp red contribution near=" + std::to_string(nearContribution.x)
                + ", distant=" + std::to_string(distantContribution.x));
        }
        if (originX == 0) {
            for (const auto position : competitors) world.set(position, Block::Air);
            // All six lanterns are closer to the sampled surface than the
            // lantern, but this wall blocks them. The seventh source must still
            // contribute; ranking a fixed number before shadows loses its light.
            for (int y = 5; y <= 7; ++y) {
                for (int z = 2; z <= 3; ++z) world.set({-2, y, z}, Block::Lantern);
            }
            for (int y = 4; y <= 8; ++y) {
                for (int z = 1; z <= 3; ++z) world.set({-1, y, z}, Block::Stone);
            }
            world.set({3, 6, 4}, Block::Lantern);
            const auto seventhLit = sample(6.5f);
            world.set({3, 6, 4}, Block::Air);
            const auto seventhUnlit = sample(6.5f);
            const auto contribution = seventhLit - seventhUnlit;
            if (contribution.x < 10 || contribution.x - contribution.z < 8) {
                throw std::runtime_error("Six blocked sources suppressed a farther visible lantern: red contribution "
                    + std::to_string(contribution.x));
            }
        }
    }
    std::cout << "World lighting passed: camera steps, distant views, chunk neighbors and many sources\n";
}

void verifyBlockRelief(int width, int height)
{
    World world;
    for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
                world.set({x, y, z}, Block::Air);
            }
        }
    }
    // The exposed +Z face and its covering neighbor belong to different chunks.
    const glm::ivec3 ore(0, 6, Chunk::Size - 1), cover(0, 6, Chunk::Size);
    world.set(ore, Block::CopperOre);
    Camera camera;
    camera.rotate(35, 25);
    camera.setPosition({0.5f, 6.5f, Chunk::Size + 2.5f});
    WorldRenderer renderer;
    const int patchSize = std::max(16, std::min(512, height / 4));
    const auto depth = [&](bool underground, WorldRenderer* freshRenderer = nullptr) {
        auto& view = freshRenderer ? *freshRenderer : renderer;
        view.render(world, camera, width, height, false, underground);
        std::vector<float> pixels(static_cast<std::size_t>(patchSize) * patchSize);
        glReadPixels(width / 2 - patchSize / 2, height / 2 - patchSize / 2,
                     patchSize, patchSize, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());
        checkOpenGl();
        return pixels;
    };
    const auto flat = depth(false);
    if (renderer.triangleCount() != 12) throw std::runtime_error("Editor drew underground block relief");
    const auto raised = depth(true);
    const auto inverseProjection = glm::inverse(camera.projectionMatrix(static_cast<float>(width) / height));
    const auto distance = [&](float value) {
        const auto view = inverseProjection * glm::vec4(0, 0, value * 2 - 1, 1);
        return -view.z / view.w;
    };
    std::size_t raisedPixels = 0;
    for (std::size_t pixel = 0; pixel < raised.size(); ++pixel) {
        const float displacement = distance(flat[pixel]) - distance(raised[pixel]);
        // Ore rises at most 0.070 units; allow a small margin for resolved
        // depth and projection precision without accepting the old tall caps.
        if (flat[pixel] >= 1 || displacement < -0.002f || displacement > 0.080f) {
            throw std::runtime_error("Block relief exceeded its shallow surface bounds: "
                + std::to_string(displacement));
        }
        raisedPixels += displacement > 0.01f;
    }
    if (raisedPixels < raised.size() / 100) {
        throw std::runtime_error("Ore relief did not add visible surface depth");
    }

    world.set(cover, Block::Planks);
    // From inside the cover, the hidden face's stale relief would be visible.
    // Other exposed faces' bevels can also face this interior viewpoint, so
    // compare with a fresh mesh instead of assuming every pixel is clear.
    camera.setPosition({0.5f, 6.5f, Chunk::Size + 0.95f});
    const auto covered = depth(true);
    WorldRenderer freshRenderer;
    const auto freshCovered = depth(true, &freshRenderer);
    if (renderer.triangleCount() != freshRenderer.triangleCount()) {
        throw std::runtime_error("Neighbor cover edit retained stale relief triangles");
    }
    for (std::size_t pixel = 0; pixel < covered.size(); ++pixel) {
        if (std::abs(covered[pixel] - freshCovered[pixel]) > 0.00001f) {
            throw std::runtime_error("Neighbor cover edit disagreed with freshly meshed relief depth");
        }
    }
    world.set(cover, Block::Air);
    camera.setPosition({0.5f, 6.5f, Chunk::Size + 2.5f});
    const auto revealed = depth(true);
    for (std::size_t pixel = 0; pixel < raised.size(); ++pixel) {
        if (std::abs(revealed[pixel] - raised[pixel]) > 0.00001f) {
            throw std::runtime_error("Removing a neighboring cover did not restore deterministic relief");
        }
    }
    world.set(ore, Block::Air);
    const auto mined = depth(true);
    if (renderer.triangleCount() != 0
        || std::any_of(mined.begin(), mined.end(), [](float value) { return value < 0.99999f; })) {
        throw std::runtime_error("Mining an ore block retained stale relief geometry");
    }
    // Decorative meshes also depend on the chunk immediately above them.
    world.set({0, 31, 0}, Block::Crystal);
    depth(true);
    const auto crystalTriangles = renderer.triangleCount();
    world.set({0, 32, 0}, Block::Dirt);
    depth(true);
    depth(true, &freshRenderer);
    if (renderer.triangleCount() != freshRenderer.triangleCount()
        || renderer.triangleCount() >= crystalTriangles + 12) {
        throw std::runtime_error("Upper chunk edit failed to rebuild the covered crystal details");
    }
    world.set({0, 32, 0}, Block::Air);
    depth(true);
    if (renderer.triangleCount() != crystalTriangles) {
        throw std::runtime_error("Upper chunk edit failed to restore crystal details");
    }
    std::cout << "Block relief passed: real depth, shallow bounds, neighbor cover, reveal and mining\n";
}

void verifyHubDressing(int width, int height)
{
    World world;
    buildUndergroundScene(world);
    const auto dressing = buildHubDressing(world);
    std::size_t terrainTriangles = 0, undergroundTriangles = 0, assetTriangles = 0;
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        terrainTriangles += buildMesh(world, index).indices.size() / 3;
        undergroundTriangles += buildCaveDetails(world, index).indices.size() / 3
            + buildBlockRelief(world, index).indices.size() / 3;
        assetTriangles += dressing.meshes[index].indices.size() / 3;
    }
    undergroundTriangles += terrainTriangles + assetTriangles;
    if (assetTriangles == 0 || dressing.lights.empty()) {
        throw std::runtime_error("The hub fixture did not include models and their lights");
    }
    Camera camera;
    camera.setPosition(VillageSpawn + glm::vec3(0, 1.62f, 0));
    camera.rotate(VillageYaw + 125, VillagePitch + 25);
    WorldRenderer renderer;
    renderer.render(world, camera, width, height, false, false);
    if (renderer.triangleCount() != terrainTriangles) {
        throw std::runtime_error("Editor mode included underground hub models");
    }
    renderer.render(world, camera, width, height, false, true);
    if (renderer.triangleCount() != undergroundTriangles) {
        throw std::runtime_error("Underground rendering lost planned hub geometry");
    }
    constexpr int patchSize = 48;
    const auto sample = [&] {
        std::array<unsigned char, patchSize * patchSize * 4> pixels{};
        glReadPixels(width / 2 - patchSize / 2, height / 2 - patchSize / 2,
                     patchSize, patchSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto original = sample();
    renderer.render(world, camera, width, height, false, true, 0, false, false);
    if (renderer.triangleCount() != undergroundTriangles - assetTriangles) {
        throw std::runtime_error("Root Crossing render mode retained village fixtures");
    }
    renderer.render(world, camera, width, height, false, true);
    if (renderer.triangleCount() != undergroundTriangles || sample() != original) {
        throw std::runtime_error("Returning from Root Crossing mode lost village fixtures or lighting");
    }
    renderer.render(world, camera, width, height, false, false);
    if (renderer.triangleCount() != terrainTriangles) {
        throw std::runtime_error("Switching back to editor retained hub model counts");
    }
    const auto saved = world;
    for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) world.set({x,y,z}, Block::Air);
        }
    }
    renderer.render(world, camera, width, height, false, true);
    if (renderer.triangleCount() != 0 || renderer.drawnTriangles() != 0 || renderer.visibleChunks() != 0) {
        throw std::runtime_error("Clearing the world retained stale hub model meshes");
    }
    world = saved;
    renderer.render(world, camera, width, height, false, true);
    if (renderer.triangleCount() != undergroundTriangles || sample() != original) {
        throw std::runtime_error("Restoring the hub did not restore its models and stable lighting");
    }
    checkOpenGl();
    std::cout << "Hub assets passed: exact geometry counts, editor/course mode switches, clear and restore\n";
}

void verifyRootCrossingRendering(int width, int height)
{
    Options options;
    options.rootCrossing = 0;
    options.smokeTest = true;
    Game game(options);
    WorldRenderer renderer;
    width = std::min(width, 960);
    height = std::min(height, 540);
    for (int course = 0; course < RootCrossingCount; ++course) {
        if (course != 0) {
            InputActions next;
            next.active = true;
            next.crossingStep = 1;
            game.update(next);
        }
        renderer.render(game.world, game.camera, width, height, false, true, 2.0f, false, false);
        checkOpenGl();
        if (game.rootCrossingIndex() != course || renderer.drawnTriangles() < 1000)
            throw std::runtime_error("Root Crossing level switch lost its cave geometry");
        verifyFrame(width, height, {});
    }
    InputActions back;
    back.active = back.returnToHub = true;
    game.update(back);
    renderer.render(game.world, game.camera, width, height, false, true);
    checkOpenGl();
    if (game.rootCrossingActive() || renderer.drawnTriangles() < 1000)
        throw std::runtime_error("Returning from the course collection lost the hub scene");
    std::cout << "Root Crossings passed: all ten cave views, consecutive switches, and return to hub\n";
}

void verifySectionAssets(int width, int height)
{
    width = std::min(width, 960); height = std::min(height, 540);
    World world;
    buildBlankSection(world);
    Camera camera;
    camera.setPosition({5.0f,15.0f,22.0f});
    WorldRenderer renderer;
    const auto readFrameColor = [](int w, int h) {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(w) * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    renderer.render(world, camera, width, height, false, true, 1, true, false);
    const auto baseline = renderer.triangleCount();
    std::vector<HubAsset> assets{{HubAssetKind::Forge,{0.5f,9,15.5f},2,0},
                               {HubAssetKind::FernBed,{-2.5f,9,15.5f},0,3},
                               {HubAssetKind::HangingLamp,{2.5f,13,16.5f},0,0}};
    renderer.render(world, camera, width, height, false, true, 1, true, false, assets);
    std::size_t expected = baseline;
    for (const auto& asset : assets) expected += buildHubAsset(asset).indices.size() / 3;
    if (renderer.triangleCount() != expected) throw std::runtime_error("Placed section assets were not rendered");
    const auto first = readFrameColor(width,height);
    assets[0].position.x += 5;
    assets[0].facing = 1;
    renderer.render(world, camera, width, height, false, true, 1, true, false, assets);
    if (readFrameColor(width,height) == first) throw std::runtime_error("Asset-only edit left the section render cache stale");
    Overlay overlay;
    overlay.drawSection(SectionMetadata{}, assets[1], assets[0], camera, width, height);
    renderer.render(world, camera, width, height, false, true, 1, true, false);
    if (renderer.triangleCount() != baseline) throw std::runtime_error("Removed section assets remained in the renderer");
    verifyFrame(width, height, {});
    checkOpenGl();
    std::cout << "Section assets passed: placement, rotation, asset-only refresh, markers, preview, removal\n";
}

void verifyDroppedItems(int width, int height)
{
    width=std::min(width,960);height=std::min(height,540);
    World world;buildBlankSection(world);
    Camera camera;camera.setPosition({.5f,10.2f,24.0f});
    const auto direction=glm::normalize(glm::vec3(.5f,9.55f,20.5f)-camera.position()),old=camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                  glm::degrees(std::asin(direction.y)-std::asin(old.y)));
    WorldRenderer renderer;
    std::vector<HubAsset> drops{
        {HubAssetKind::KeyPickup,{-1.0f,9,20.5f}},
        {HubAssetKind::ItemPickup,{.5f,9,20.5f},0,0,{},{ItemId::Amethyst,4}},
        {HubAssetKind::ItemPickup,{2,9,20.5f},0,0,{},{ItemId::CopperOre,12}}
    };
    const auto frame=[&](float time,std::span<const HubAsset> assets,std::span<const glm::vec3> offsets={}) {
        renderer.render(world,camera,width,height,false,true,time,true,false,assets,offsets);
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*4);
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        checkOpenGl();
        return pixels;
    };
    const auto changedPixels=[](const auto& a,const auto& b) {
        std::size_t count=0;
        for (std::size_t i=0;i<a.size();i+=4)
            count+=std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                            std::abs(int(a[i+2])-int(b[i+2]))})>15;
        return count;
    };
    frame(0,{});
    const auto terrainTriangles=renderer.triangleCount(),rebuilds=renderer.sceneRebuildCount();
    const auto first=frame(0,drops);
    std::size_t dropTriangles=0;
    for (const auto& drop:drops) dropTriangles+=buildHubAsset(drop).indices.size()/3;
    if (renderer.triangleCount()!=terrainTriangles+dropTriangles)
        throw std::runtime_error("Dynamic drops were omitted or double-counted in the scene");
    const auto moving=frame(1.75f,drops);
    if (changedPixels(first,moving)<30 || renderer.sceneRebuildCount()!=rebuilds)
        throw std::runtime_error("Dropped items did not animate independently of the static scene cache");
    if (frame(0,drops)!=first)
        throw std::runtime_error("Pickup animation depends on render order instead of its stable time and anchor");
    drops[1].pickupItem.count=99;
    if (frame(0,drops)!=first || renderer.sceneRebuildCount()!=rebuilds)
        throw std::runtime_error("Changing a pickup quantity changed its mesh or rebuilt world lighting");
    const std::array<glm::vec3,3> offsets{{{}, {.6f,.25f,.65f}, {}}};
    if (changedPixels(first,frame(0,drops,offsets))<30 || renderer.sceneRebuildCount()!=rebuilds)
        throw std::runtime_error("Magnetic pickup offsets failed to move the model independently");
    frame(0,{});
    if (renderer.triangleCount()!=terrainTriangles || renderer.sceneRebuildCount()!=rebuilds)
        throw std::runtime_error("Collecting a drop left stale geometry or invalidated static scenery");
    Overlay overlay;
    frame(0,drops);overlay.drawSection({},drops[1],drops[0],camera,width,height,0);
    std::vector<unsigned char> outlined(first.size());
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,outlined.data());
    frame(1.75f,drops);overlay.drawSection({},drops[1],drops[0],camera,width,height,1.75f);
    std::vector<unsigned char> later(first.size());
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,later.data());
    if (changedPixels(outlined,later)<30)
        throw std::runtime_error("Creative pickup previews do not follow the animated item pose");
    for (int x=-3;x<=3;++x) for (int y=9;y<=12;++y) world.set({x,y,22},Block::Stone);
    const auto wall=frame(0,{});
    if (frame(0,drops)!=wall)
        throw std::runtime_error("Floating drops ignore opaque wall depth");
    for (int x=-3;x<=3;++x) for (int y=9;y<=12;++y) world.set({x,y,22},Block::Air);
    for (int x=-3;x<=3;++x) for (int z=20;z<=22;++z) world.set({x,9,z},Block::Water);
    const auto water=frame(0,{});
    if (changedPixels(water,frame(0,drops))<10)
        throw std::runtime_error("The water scene snapshot lost the animated dropped items");
    frame(0,drops,offsets);
    checkOpenGl();
    std::cout<<"Dropped items passed: hover/spin, legacy keys, dynamic offsets, stable caches, previews, depth and water\n";
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
        if (materialMesh.indices.empty()) throw std::runtime_error("Renderable material produced an empty mesh");
        for (const auto& vertex : materialMesh.vertices) {
            if (vertex.material != static_cast<float>(stairMaterial(static_cast<Block>(id)))) {
                throw std::runtime_error("Voxel surface material was lost while meshing");
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

void verifyMonsters(int width, int height)
{
    Options options; options.monsters = true;
    Game game(options);
    WorldRenderer renderer;
    const auto pixels = [&] {
        std::vector<unsigned char> data(static_cast<std::size_t>(width)*height*4);
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,data.data());
        checkOpenGl();
        return data;
    };
    for (const auto& creature : game.monsters()) {
        auto monster = creature;
        monster.animation = MonsterAnimation::Walk;
        const auto render = [&] {
            renderer.render(game.world,game.camera,width,height,false,true,0,false,false,
                            game.sectionSceneAssets(),{},std::span<const MonsterInstance>(&monster,1));
        };
        render();
        const auto still = pixels();
        const auto rebuilds = renderer.sceneRebuildCount();
        monster.seconds = .57f;
        render();
        const auto moving = pixels();
        std::size_t changed=0;
        for(std::size_t i=0;i<still.size();i+=4)
            changed += std::abs(int(still[i])-int(moving[i]))
                +std::abs(int(still[i+1])-int(moving[i+1]))
                +std::abs(int(still[i+2])-int(moving[i+2])) > 15;
        if (changed<100 || renderer.sceneRebuildCount()!=rebuilds)
            throw std::runtime_error("Creature animation missing or rebuilding static scenery");
    }
    Hud hud;
    hud.drawMonsters(game,width,height,1);
    checkOpenGl();
    std::cout << "Creature rendering passed: three animated rigs, stable scenery cache and preview HUD\n";
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
    verifyVoxelShapeLighting();
    verifyUndergroundMaterials(width, height);
    verifyStableWorldLighting(width, height);
    verifyBlockRelief(width, height);
    verifyHubDressing(width, height);
    verifyWaterRendering(width, height);
    verifyPlayHud(width, height);
    verifyHeldSword(width, height);
    verifyHeldPickaxe(width, height);
    verifyMiningHud(width, height);
    verifyInventoryHud(width, height);
    verifyRootCrossingHud(width, height);
    verifyRootCrossingRendering(width, height);
    verifyPalettePreviews();
    verifySectionHud(width, height);
    verifySectionAssets(width, height);
    verifyDroppedItems(width, height);
    verifyLevelHud(width, height);
    verifyLushMaterials(width, height);
    verifyOreMaterials(width, height);
    verifySurfaceRelief(width, height);
    verifyMaterialContinuity();
    verifyMonsters(width, height);
    verifyDamageFeedback();

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
        renderer.render(game.world, game.camera, width, height, false, !game.editorMode(), 0.0f,
                        !game.monsterPreviewActive() && (game.devMode() || game.equipment.selectedTool() == Tool::Lantern),
                        !game.rootCrossingActive() && !game.devMode() && !game.levelActive() && !game.monsterPreviewActive(),
                        game.sectionSceneAssets(),game.sectionPickupOffsets(),game.monsters());
        float scaleX, scaleY;
        glfwGetWindowContentScale(window.handle(), &scaleX, &scaleY);
        overlay.draw(game.target, game.camera, width, height, scaleY);
        if (game.levelActive() && !game.levelMenuOpen())
            overlay.drawSection(game.levelController().meta(), {}, {}, game.camera, width, height);
        if (!game.rootCrossingActive() && !game.devMode() && !game.levelController().open() && !game.monsterPreviewActive())
            hud.draw(game.statusText(renderer.visibleChunks(), renderer.drawnTriangles(),
                                     renderer.triangleCount(), 0.0, true, false), width, height, scaleY);
        if (game.monsterPreviewActive()) hud.drawMonsters(game,width,height,scaleY);
        else if (game.devMode()) hud.drawSection(game, width, height, scaleY);
        else if (!game.editorMode() && !game.levelMenuOpen()) hud.drawPlay(game, width, height, scaleY);
        if (game.levelController().open()) hud.drawLevels(game, width, height, scaleY);
        hud.drawHealth(game,width,height,scaleY);
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
