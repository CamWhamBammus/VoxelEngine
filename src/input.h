#pragma once

#include "world.h"

#include <glm/vec2.hpp>
#include <array>
#include <optional>
#include <string>

struct GLFWwindow;

namespace voxel {
class Camera;

// One-frame requests passed to Game, reset at the start of every input update.
struct InputActions {
    bool active = false; // Focused with the mouse captured; otherwise the game pauses.
    glm::vec2 movement{0.0f}; // Right/left and forward/back, independent of camera pitch.
    bool jump = false;
    bool sprint = false;
    bool respawn = false;
    std::optional<int> selectedTool; // Play hotbar, numbered 1–3 on the keyboard.
    int toolStep = 0;
    bool interact = false;
    bool upgrade = false;
    bool closeMenu = false;
    bool crossingMenu = false;
    std::optional<int> selectedCrossing; // 0–9 while the test picker is open.
    int crossingMenuStep = 0;
    int crossingStep = 0;
    bool returnToHub = false;
    bool restartCrossing = false;
    bool levelCreator = false, levelMenu = false; // F11 authoring / F4 expeditions.
    bool devToggle = false, devPalette = false, devTest = false, devAccept = false, devLibrary = false;
    bool devRotate = false, devVariation = false, devBackspace = false, devSelectAll = false;
    std::optional<int> devSlot, devClick;
    int devTabStep = 0;
    glm::ivec2 devMenuStep{0};
    std::string devText;
    bool devPointerPressed = false;
    glm::vec2 devPointer{0}; // Window-relative position, normalized to [0,1].
    // Editing requests are emitted only in --editor mode.
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

    void update(Camera& camera, float deltaSeconds, bool editorMode = true, bool modal = false,
                bool crossingPicker = false, bool devMode = false, bool devFlying = false,
                bool devTyping = false);
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
        bool remove = false;
        bool place = false;
        bool pick = false;
        bool interact = false;
        bool upgrade = false;
        std::array<bool, 3> tools{};
        std::array<bool, 10> courses{};
        bool crossingMenu = false;
        bool menuPrevious = false;
        bool menuNext = false;
        bool previousCrossing = false;
        bool nextCrossing = false;
        bool returnToHub = false;
        bool devToggle = false, devTest = false, devAccept = false;
        bool levelCreator = false, levelMenu = false;
        bool rotate = false, variation = false, backspace = false, selectAll = false;
        bool menuLeft = false, menuRight = false;
    };

    bool keyDown(int key) const;
    Buttons readButtons() const;
    void capture(bool enabled);
    void updateCamera(Camera& camera, float deltaSeconds, bool editingShortcut, bool editorMode);
    void updateDev(Camera& camera, float deltaSeconds, const Buttons& buttons, const Buttons& previous,
                   bool modal, bool flying, bool typing, bool modifier, EditShortcutActions shortcuts);

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
    std::string pendingText_;
    bool wasDevMode_ = false, wasDevMenu_ = false;
    float removeRepeat_ = 0.0f, placeRepeat_ = 0.0f;
    float backspaceRepeat_ = 0.0f;
    bool suppressDevMouse_ = true, wasDevFlying_ = false;
};

} // namespace voxel
