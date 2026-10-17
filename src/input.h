#pragma once

#include "world.h"

#include <glm/vec2.hpp>
#include <optional>

struct GLFWwindow;

namespace voxel {
class Camera;

// One-frame requests passed to Game, reset at the start of every input update.
struct InputActions {
    bool active = false; // Focused with the mouse captured; otherwise the game pauses.
    glm::vec2 movement{0.0f}; // Right/left and forward/back, independent of camera pitch.
    bool jump = false;
    bool sprint = false;
    bool interact = false;
    bool respawn = false;
    bool restart = false;
    bool save = false;
    bool load = false;
    bool undo = false;
    bool redo = false;
    bool remove = false;
    bool place = false;
    bool pick = false;
    std::optional<Block> selectedBlock;
    int materialStep = 0; // Previous (-1) or next (+1), unless a number key selects directly.
};

struct EditShortcutKeys {
    bool z = false;
    bool y = false;
    bool modifier = false;
    bool shift = false;
};

struct EditShortcutActions {
    bool undo = false;
    bool redo = false;
};

class EditShortcuts {
public:
    EditShortcutActions update(EditShortcutKeys keys)
    {
        // Only a physical letter-key press creates an action. Releasing Shift
        // after redo must never turn the held Z key into a fresh undo action.
        const bool zPressed = keys.z && !previous_.z;
        const bool yPressed = keys.y && !previous_.y;
        previous_ = keys;
        return {keys.modifier && zPressed && !keys.shift,
                keys.modifier && (yPressed || (zPressed && keys.shift))};
    }

private:
    EditShortcutKeys previous_;
};

class Input {
public:
    explicit Input(GLFWwindow* window);
    ~Input();
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    void update(Camera& camera, float deltaSeconds, bool editorMode = true);
    const InputActions& actions() const { return actions_; }
    bool captured() const { return captured_; }
    bool wireframe() const { return wireframe_; }
    bool helpVisible() const { return help_; }

private:
    struct Buttons {
        bool releaseMouse = false;
        bool toggleMouse = false;
        bool wireframe = false;
        bool save = false;
        bool load = false;
        bool resetCamera = false;
        bool help = false;
        bool previousMaterial = false;
        bool nextMaterial = false;
        bool jump = false;
        bool interact = false;
        bool restart = false;
        bool remove = false;
        bool place = false;
        bool pick = false;
    };

    bool keyDown(int key) const;
    Buttons readButtons() const;
    void capture(bool enabled);
    void updateCamera(Camera& camera, float deltaSeconds, bool editingShortcut, bool editorMode);

    GLFWwindow* window_;
    InputActions actions_;
    Buttons previousButtons_;
    EditShortcuts editShortcuts_;
    bool captured_ = false;
    bool wireframe_ = false;
    bool help_ = false;
    double cursorX_ = 0.0;
    double cursorY_ = 0.0;
    double scroll_ = 0.0;
};

} // namespace voxel
