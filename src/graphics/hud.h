#pragma once

#include "graphics/opengl.h"

#include <string_view>
#include <optional>

namespace voxel {
class Game;

// Requires a current OpenGL context throughout its lifetime.
class Hud {
public:
    Hud();
    ~Hud();
    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;

    void draw(std::string_view text, int framebufferWidth, int framebufferHeight,
              float contentScale = 1.0f);
    void drawPlay(const Game& game, int framebufferWidth, int framebufferHeight,
                  float contentScale = 1.0f);
    void drawSection(const Game& game, int framebufferWidth, int framebufferHeight,
                     float contentScale = 1.0f);
    // Pointer coordinates are physical framebuffer pixels, measured from the
    // top-left. Drawing and hit testing share the same scaled layout.
    static std::optional<int> sectionClick(const Game& game, int framebufferWidth,
                                          int framebufferHeight, float contentScale,
                                          float x, float y);
    void drawLevels(const Game& game, int framebufferWidth, int framebufferHeight,
                    float contentScale = 1.0f);
    static std::optional<int> levelClick(const Game& game, int framebufferWidth,
                                        int framebufferHeight, float contentScale,
                                        float x, float y);

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
};

} // namespace voxel
