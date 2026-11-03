#pragma once

#include "graphics/opengl.h"

#include <array>

namespace voxel {

// Draw linear world color between begin() and finish(). Alpha tags luminous
// surfaces for bloom. The final image and depth return to the caller's target;
// draw the selection outline and HUD afterwards so they remain crisp.
class PostProcess {
public:
    PostProcess();
    ~PostProcess();
    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    // Scale only the world target. finish() restores color and depth at the
    // requested output size so selection outlines and UI keep native detail.
    void setRenderScale(float scale);
    void setSamples(int samples);
    void begin(int width, int height);
    // Snapshot opaque color and depth before drawing water into the same scene.
    // Textures remain unchanged until the next capture, finish, or resize.
    void captureScene();
    GLuint sceneColor() const noexcept { return color_; }
    GLuint sceneDepth() const noexcept { return depth_; }
    int sceneWidth() const noexcept { return width_; }
    int sceneHeight() const noexcept { return height_; }
    void finish();

private:
    void resize(int width, int height);
    void restore();
    void release();

    Shader extract_, blur_, composite_;
    GLuint scene_ = 0, resolved_ = 0, color_ = 0, depth_ = 0;
    GLuint sceneColorBuffer_ = 0, sceneDepthBuffer_ = 0, vao_ = 0;
    std::array<GLuint, 2> bloomFrames_{}, bloomColors_{};
    int width_ = 0, height_ = 0, bloomWidth_ = 0, bloomHeight_ = 0;
    int outputWidth_ = 0, outputHeight_ = 0;
    float renderScale_ = 1.0f;
    int requestedSamples_ = 4, allocatedSamples_ = -1, actualSamples_ = 0;
    bool active_ = false;

    // Only state touched by this pass is saved. Texture units 0–2 are private
    // while the pass runs; their original bindings are restored at the end.
    GLint drawFrame_ = 0, readFrame_ = 0, program_ = 0, vertexArray_ = 0;
    GLint renderbuffer_ = 0, activeTexture_ = 0, depthFunction_ = 0;
    std::array<GLint, 3> textures_{};
    std::array<GLint, 4> viewport_{};
    std::array<GLint, 2> polygonMode_{};
    GLboolean depthTest_ = false, depthWrite_ = true, cull_ = false;
    GLboolean blend_ = false, scissor_ = false, multisample_ = true, srgb_ = false;
};

} // namespace voxel
