#pragma once

#include "graphics/opengl.h"
#include "graphics/palette_previews.h"

#include <string_view>
#include <optional>

namespace voxel {
class Game;

inline constexpr int InventoryUiSlotBase = 0;
inline constexpr int InventoryUiLootBase = 100;
inline constexpr int InventoryUiSort = 200;
inline constexpr int InventoryUiTakeAll = 201;
inline constexpr int InventoryUiClose = 202;

// Requires a current OpenGL context throughout its lifetime.
class Hud {
public:
    Hud();
    ~Hud();
    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;

    void draw(std::string_view text, int framebufferWidth, int framebufferHeight,
              float contentScale = 1.0f);
    void drawMonsters(const Game& game, int framebufferWidth, int framebufferHeight,
                      float contentScale = 1.0f);
    void drawPlay(const Game& game, int framebufferWidth, int framebufferHeight,
                  float contentScale = 1.0f);
    // Draw before the tool belt and menus: hit-revealed mob bars and trial status.
    void drawCombat(const Game& game, int framebufferWidth, int framebufferHeight,
                    float contentScale = 1.0f);
    // Draw last so damage and the automatic respawn message remain visible.
    void drawHealth(const Game& game, int framebufferWidth, int framebufferHeight,
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
    void drawInventory(const Game& game, int framebufferWidth, int framebufferHeight,
                       float contentScale = 1.0f);
    static std::optional<int> inventoryClick(const Game& game, int framebufferWidth,
                                            int framebufferHeight, float contentScale,
                                            float x, float y);

private:
    Shader shader_;
    Shader previewShader_;
    PalettePreviews previews_;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
};

} // namespace voxel
