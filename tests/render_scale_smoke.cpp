#include "render_scale_smoke.h"
#include "camera.h"
#include "graphics/post_process.h"
#include "graphics/renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Target {
    static constexpr int width = 321, height = 181;
    GLuint frame = 0, color = 0, depth = 0, snapshot = 0, vao = 0;
    Target()
    {
        glGenFramebuffers(1, &frame); glGenFramebuffers(1, &snapshot);
        glGenTextures(1, &color); glGenRenderbuffers(1, &depth); glGenVertexArrays(1, &vao);
        glBindFramebuffer(GL_FRAMEBUFFER, frame);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "Render scale test target is incomplete");
    }
    ~Target()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0); glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindVertexArray(0); glUseProgram(0); glActiveTexture(GL_TEXTURE0);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LESS); glEnable(GL_MULTISAMPLE);
        glDeleteFramebuffers(1, &frame); glDeleteFramebuffers(1, &snapshot);
        glDeleteTextures(1, &color); glDeleteRenderbuffers(1, &depth); glDeleteVertexArrays(1, &vao);
    }
    std::array<unsigned char, 4> pixel(int x, int y) const
    {
        std::array<unsigned char, 4> value{};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, value.data());
        return value;
    }
    float depthAt(int x, int y) const
    {
        float value = 0;
        glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &value);
        return value;
    }
};

} // namespace

void verifyRenderScale()
{
    Target target;
    PostProcess post;
    Shader overlay(
        "#version 410 core\nuniform float depth; void main() { vec2 uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
        "gl_Position=vec4(uv*2-1,depth*2-1,1); }",
        "#version 410 core\nout vec4 color; void main() {color=vec4(1,0,0,1);}");
    for (const float scale : {-1.f, 0.49f, 1.01f, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected = false;
        try { post.setRenderScale(scale); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Post-processing accepted an invalid render scale");
    }
    for (const int samples : {-1, 1, 3, 8}) {
        bool rejected = false;
        try { post.setSamples(samples); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Post-processing accepted an invalid MSAA count");
    }
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    // Reuse one instance through allocation changes, including returning to
    // full resolution. Odd dimensions catch truncation and missed edge pixels.
    for (const float scale : {0.5f, 0.75f, 1.f}) for (const int samples : {0, 2, 4}) {
        post.setRenderScale(scale); post.setSamples(samples);
        glBindFramebuffer(GL_FRAMEBUFFER, target.frame);
        glViewport(7, 9, 123, 87);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
        post.begin(Target::width, Target::height);
        const int width = static_cast<int>(std::lround(Target::width * scale));
        const int height = static_cast<int>(std::lround(Target::height * scale));
        std::array<GLint, 4> viewport{};
        GLint actualSamples = 0;
        glGetIntegerv(GL_VIEWPORT, viewport.data()); glGetIntegerv(GL_SAMPLES, &actualSamples);
        const int expectedSamples = samples >= 4 && maxSamples >= 4 ? 4 : samples >= 2 && maxSamples >= 2 ? 2 : 0;
        require(viewport == std::array<GLint, 4>{0, 0, width, height}
                    && post.sceneWidth() == width && post.sceneHeight() == height,
                "Render scale did not resize the scene viewport and water snapshot together");
        require(actualSamples == expectedSamples && bool(glIsEnabled(GL_MULTISAMPLE)) == (expectedSamples != 0),
                "MSAA setting did not change the scene framebuffer");
        bool scaleRejected = false, samplesRejected = false;
        try { post.setRenderScale(1); } catch (const std::logic_error&) { scaleRejected = true; }
        try { post.setSamples(0); } catch (const std::logic_error&) { samplesRejected = true; }
        require(scaleRejected && samplesRejected, "Graphics settings changed halfway through an active frame");

        constexpr std::array<float, 4> green{0.05f, 0.5f, 0.1f, 0}, blue{0.03f, 0.05f, 0.8f, 0};
        const float farDepth = 0.7f, nearDepth = 0.25f;
        glClearBufferfv(GL_COLOR, 0, green.data()); glClearBufferfv(GL_DEPTH, 0, &farDepth);
        glEnable(GL_SCISSOR_TEST); glScissor(0, 0, width / 2, height);
        glClearBufferfv(GL_COLOR, 0, blue.data()); glClearBufferfv(GL_DEPTH, 0, &nearDepth);
        glDisable(GL_SCISSOR_TEST);
        post.captureScene();
        GLint scene = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &scene);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target.snapshot);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, post.sceneColor(), 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, post.sceneDepth(), 0);
        require(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "Scaled opaque snapshot is incomplete");
        require(target.pixel(width - 2, height - 2)[1] > 100
                    && std::abs(target.depthAt(width - 2, height - 2) - farDepth) < 0.0001f,
                "Scaled water snapshot lost opaque color or depth");
        // Subsequent water drawing must never read from its own draw attachment,
        // even when antialiasing is disabled.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(scene));
        constexpr std::array<float, 4> red{0.8f, 0.03f, 0.02f, 0};
        glEnable(GL_SCISSOR_TEST); glScissor(width * 3 / 4, height * 3 / 4, width, height);
        glClearBufferfv(GL_COLOR, 0, red.data());
        require(target.pixel(width - 2, height - 2)[1] > 100,
                "Drawing water changed the opaque texture it was sampling");
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(scene));
        post.finish();
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        require(viewport == std::array<GLint, 4>{7, 9, 123, 87}, "Scaled rendering changed the caller's viewport");
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target.frame);
        require(target.pixel(1, 1)[2] > 150 && target.pixel(Target::width - 2, Target::height - 2)[0] > 150,
                "Scaled color did not cover the entire native output");
        require(std::abs(target.depthAt(4, 4) - nearDepth) < 0.0001f
                    && std::abs(target.depthAt(Target::width - 4, Target::height - 4) - farDepth) < 0.0001f,
                "Scaled world depth was not restored at native resolution");

        // A one-pixel overlay remains sharp, and native world depth still hides
        // fragments behind a foreground surface.
        glViewport(0, 0, Target::width, Target::height);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glBindVertexArray(target.vao);
        overlay.use(); overlay.setFloat("depth", 0.5f);
        glEnable(GL_SCISSOR_TEST); glScissor(8, 8, 1, 1); glDrawArrays(GL_TRIANGLES, 0, 3);
        require(target.pixel(8, 8)[2] > 150, "Native overlay drew through scaled world depth");
        overlay.setFloat("depth", 0.1f); glDrawArrays(GL_TRIANGLES, 0, 3);
        require(target.pixel(8, 8)[0] == 255 && target.pixel(9, 8)[2] > 150,
                "Render scale blurred or enlarged a native one-pixel overlay");
        glDisable(GL_SCISSOR_TEST);
        require(glGetError() == GL_NO_ERROR, "Render scale checks generated an OpenGL error");
    }

    // Exercise the actual water shader, not just the snapshot API. Its screen UV
    // must use the scaled scene size instead of the native output dimensions.
    World world;
    for (int z = -6; z < 8; ++z) for (int x = -7; x < 8; ++x) {
        world.set({x, 0, z}, (x / 2 + z / 2) % 2 ? Block::Stone : Block::Moss);
        world.set({x, 1, z}, Block::Water);
    }
    world.set({-3, 0, -1}, Block::Crystal);
    Camera camera;
    camera.setPosition({0.5f, 5.5f, 6.5f}); camera.rotate(35, -12);
    WorldRenderer renderer;
    renderer.setSamples(0);
    const auto frame = [&](float scale) {
        renderer.setRenderScale(scale);
        glBindFramebuffer(GL_FRAMEBUFFER, target.frame);
        glViewport(0, 0, Target::width, Target::height);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        renderer.render(world, camera, Target::width, Target::height, false, true, 1, false, false);
        std::vector<unsigned char> pixels(static_cast<std::size_t>(Target::width) * Target::height * 4);
        glReadPixels(0, 0, Target::width, Target::height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto full = frame(1);
    const auto rebuilds = renderer.sceneRebuildCount();
    for (const float scale : {0.5f, 0.75f}) {
        const auto reduced = frame(scale);
        double error = 0, brightness = 0;
        for (std::size_t i = 0; i < full.size(); i += 4) for (int channel = 0; channel < 3; ++channel) {
            error += std::abs(int(full[i + channel]) - int(reduced[i + channel]));
            brightness += full[i + channel];
        }
        const double values = static_cast<double>(Target::width) * Target::height * 3;
        require(brightness / values > 8, "Render scale water comparison has no visible scene");
        require(error / values < 12, "Lower resolution shifted the water scene capture or changed its lighting");
        require(renderer.sceneRebuildCount() == rebuilds, "Render scale changes rebuilt world geometry");
    }
    require(frame(1) == full, "Returning to native resolution changed the original water image");
    require(glGetError() == GL_NO_ERROR, "Scaled water generated an OpenGL error");
    std::cout << "Render scale passed: native UI/depth, 0/2/4 MSAA, resize, opaque snapshots and water\n";
}

} // namespace voxel
