#include "graphics/post_process.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace voxel {
namespace {

constexpr const char* screenVertex = R"glsl(#version 410 core
out vec2 uv;
void main() {
    uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

constexpr const char* extractFragment = R"glsl(#version 410 core
in vec2 uv;
uniform sampler2D source;
out vec4 fragmentColor;
void main() {
    vec2 pixel = 1.0 / vec2(textureSize(source, 0));
    vec3 glow = vec3(0.0);
    for (int y = -1; y <= 1; y += 2) {
        for (int x = -1; x <= 1; x += 2) {
            vec4 sampleColor = texture(source, uv + vec2(x, y) * pixel);
            glow += sampleColor.rgb * sampleColor.a;
        }
    }
    fragmentColor = vec4(glow * 0.25, 1.0);
}
)glsl";

constexpr const char* blurFragment = R"glsl(#version 410 core
in vec2 uv;
uniform sampler2D source;
uniform int horizontal;
out vec4 fragmentColor;
void main() {
    vec2 stepSize = (horizontal != 0 ? vec2(1, 0) : vec2(0, 1)) / vec2(textureSize(source, 0));
    // Bilinear filtering combines adjacent Gaussian taps into five reads.
    vec3 glow = texture(source, uv).rgb * 0.227027;
    glow += texture(source, uv + stepSize * 1.384615).rgb * 0.316216;
    glow += texture(source, uv - stepSize * 1.384615).rgb * 0.316216;
    glow += texture(source, uv + stepSize * 3.230769).rgb * 0.070270;
    glow += texture(source, uv - stepSize * 3.230769).rgb * 0.070270;
    fragmentColor = vec4(glow, 1.0);
}
)glsl";

constexpr const char* compositeFragment = R"glsl(#version 410 core
in vec2 uv;
uniform sampler2D scene;
uniform sampler2D bloom;
uniform sampler2D sceneDepth;
out vec4 fragmentColor;
vec3 filmic(vec3 color) {
    return clamp((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}
void main() {
    vec3 hdr = max((texture(scene, uv).rgb + texture(bloom, uv).rgb * 0.10) * 0.90, vec3(0.0));
    // A gentle filmic shoulder retains bright detail instead of clipping HDR
    // highlights. Preserve hue at high radiance so amber glass stays amber.
    float peak = max(hdr.r, max(hdr.g, hdr.b));
    vec3 huePreserved = hdr * filmic(vec3(peak)).r / max(peak, 0.0001);
    vec3 color = mix(filmic(hdr), huePreserved, smoothstep(0.55, 1.8, peak));
    // Mild vibrance lifts muted colors without oversaturating gems.
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float chroma = max(color.r, max(color.g, color.b)) - min(color.r, min(color.g, color.b));
    color = mix(vec3(luma), color, 1.12 - 0.12 * chroma);
    fragmentColor = vec4(pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
    // Writing resolved depth here works with either fixed or floating-point
    // destination depth buffers, and preserves block-outline occlusion.
    gl_FragDepth = texture(sceneDepth, uv).r;
}
)glsl";

void completeFramebuffer()
{
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Could not create a complete HDR framebuffer");
    }
}

void allocationSucceeded()
{
    // An allocation failure can leave the old, still-complete attachments in
    // place. Never accept a resize based on framebuffer completeness alone.
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        throw std::runtime_error("HDR buffer allocation failed: OpenGL error " + std::to_string(error));
    }
}

void texture(GLuint name, int width, int height, bool depth = false)
{
    glBindTexture(GL_TEXTURE_2D, name);
    glTexImage2D(GL_TEXTURE_2D, 0, depth ? GL_DEPTH_COMPONENT24 : GL_RGBA16F,
                 width, height, 0, depth ? GL_DEPTH_COMPONENT : GL_RGBA, GL_FLOAT, nullptr);
    allocationSucceeded();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, depth ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, depth ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void enabled(GLenum capability, GLboolean value)
{
    if (value) glEnable(capability);
    else glDisable(capability);
}

} // namespace

PostProcess::PostProcess()
    : extract_(screenVertex, extractFragment), blur_(screenVertex, blurFragment),
      composite_(screenVertex, compositeFragment)
{
    glGenFramebuffers(1, &scene_);
    glGenFramebuffers(1, &resolved_);
    glGenFramebuffers(2, bloomFrames_.data());
    glGenTextures(1, &color_);
    glGenTextures(1, &depth_);
    glGenTextures(2, bloomColors_.data());
    glGenRenderbuffers(1, &multisampleColor_);
    glGenRenderbuffers(1, &multisampleDepth_);
    glGenVertexArrays(1, &vao_);
    if (!scene_ || !resolved_ || !color_ || !depth_ || !multisampleColor_ || !multisampleDepth_
        || !vao_ || !bloomFrames_[0] || !bloomFrames_[1] || !bloomColors_[0] || !bloomColors_[1]) {
        release();
        throw std::runtime_error("Failed to allocate post-processing resources");
    }
}

PostProcess::~PostProcess()
{
    if (active_) restore();
    release();
}

void PostProcess::release()
{
    glDeleteFramebuffers(1, &scene_);
    glDeleteFramebuffers(1, &resolved_);
    glDeleteFramebuffers(2, bloomFrames_.data());
    glDeleteTextures(1, &color_);
    glDeleteTextures(1, &depth_);
    glDeleteTextures(2, bloomColors_.data());
    glDeleteRenderbuffers(1, &multisampleColor_);
    glDeleteRenderbuffers(1, &multisampleDepth_);
    glDeleteVertexArrays(1, &vao_);
}

void PostProcess::resize(int width, int height)
{
    if (width == width_ && height == height_) return;
    GLint maxSize = 0, maxTextureSize = 0, maxSamples = 0;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxSize);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (width > std::min(maxSize, maxTextureSize) || height > std::min(maxSize, maxTextureSize)) {
        throw std::invalid_argument("HDR framebuffer dimensions exceed the GPU limit");
    }
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    const int samples = std::min(4, maxSamples);
    // If allocation fails partway through, force a complete retry next time.
    width_ = height_ = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, scene_);
    glBindRenderbuffer(GL_RENDERBUFFER, multisampleColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, width, height);
    allocationSucceeded();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleColor_);
    glBindRenderbuffer(GL_RENDERBUFFER, multisampleDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
    allocationSucceeded();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleDepth_);
    completeFramebuffer();

    glBindFramebuffer(GL_FRAMEBUFFER, resolved_);
    texture(color_, width, height);
    texture(depth_, width, height, true);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
    completeFramebuffer();

    bloomWidth_ = std::max(1, (width + 3) / 4);
    bloomHeight_ = std::max(1, (height + 3) / 4);
    for (int index = 0; index < 2; ++index) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFrames_[index]);
        texture(bloomColors_[index], bloomWidth_, bloomHeight_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomColors_[index], 0);
        completeFramebuffer();
    }
    width_ = width;
    height_ = height;
}

void PostProcess::begin(int width, int height)
{
    if (active_) throw std::logic_error("Post-processing frame already begun");
    if (width <= 0 || height <= 0) throw std::invalid_argument("HDR framebuffer dimensions must be positive");
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFrame_);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFrame_);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray_);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer_);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
    for (int unit = 0; unit < 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures_[unit]);
    }
    glGetIntegerv(GL_VIEWPORT, viewport_.data());
    glGetIntegerv(GL_POLYGON_MODE, polygonMode_.data());
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunction_);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite_);
    depthTest_ = glIsEnabled(GL_DEPTH_TEST);
    cull_ = glIsEnabled(GL_CULL_FACE);
    blend_ = glIsEnabled(GL_BLEND);
    scissor_ = glIsEnabled(GL_SCISSOR_TEST);
    multisample_ = glIsEnabled(GL_MULTISAMPLE);
    srgb_ = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    active_ = true;
    try {
        glActiveTexture(GL_TEXTURE0);
        resize(width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, scene_);
        glViewport(0, 0, width, height);
        glEnable(GL_MULTISAMPLE);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_TRUE);
    } catch (...) {
        restore();
        throw;
    }
}

void PostProcess::finish()
{
    if (!active_) throw std::logic_error("Post-processing frame has not begun");
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved_);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bloomFrames_[0]);
    glViewport(0, 0, bloomWidth_, bloomHeight_);
    extract_.use();
    extract_.setInt("source", 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    blur_.use();
    blur_.setInt("source", 0);
    for (int pass = 0; pass < 4; ++pass) {
        const int source = pass % 2;
        glBindTexture(GL_TEXTURE_2D, bloomColors_[source]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bloomFrames_[1 - source]);
        blur_.setInt("horizontal", source == 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFrame_));
    glViewport(0, 0, width_, height_);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, color_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomColors_[0]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, depth_);
    composite_.use();
    composite_.setInt("scene", 0);
    composite_.setInt("bloom", 1);
    composite_.setInt("sceneDepth", 2);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    restore();
}

void PostProcess::restore()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFrame_));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFrame_));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer_));
    for (int unit = 0; unit < 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textures_[unit]));
    }
    glActiveTexture(static_cast<GLenum>(activeTexture_));
    glBindVertexArray(static_cast<GLuint>(vertexArray_));
    glUseProgram(static_cast<GLuint>(program_));
    glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode_[0]);
    glDepthFunc(static_cast<GLenum>(depthFunction_));
    glDepthMask(depthWrite_);
    enabled(GL_DEPTH_TEST, depthTest_);
    enabled(GL_CULL_FACE, cull_);
    enabled(GL_BLEND, blend_);
    enabled(GL_SCISSOR_TEST, scissor_);
    enabled(GL_MULTISAMPLE, multisample_);
    enabled(GL_FRAMEBUFFER_SRGB, srgb_);
    active_ = false;
}

} // namespace voxel
