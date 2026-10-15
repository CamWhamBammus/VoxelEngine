#pragma once

#include "render/shader.h"

#include <string_view>

namespace voxel {

// Requires a current OpenGL context throughout its lifetime.
class Hud {
public:
    Hud();
    ~Hud();
    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;

    void draw(std::string_view text, int framebufferWidth, int framebufferHeight,
              float contentScale = 1.0f);

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
};

} // namespace voxel
