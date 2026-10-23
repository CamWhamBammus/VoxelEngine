#pragma once

#include "world.h"

#include <glm/vec2.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>

struct GLFWwindow;

namespace voxel {
class Camera;

// One-frame requests passed to Game, reset at the start of every input update.
struct InputActions {
    bool active = false; // Focused with the mouse captured; otherwise the game pauses.
    glm::vec2 movement{0.0f}; // Right/left and forward/back, independent of camera pitch.
    bool jump = false; // Held: jump on land, rise continuously in water.
    bool descend = false; // Held Ctrl: dive in water; ignored on land.
    bool sprint = false;
    bool respawn = false;
    std::optional<int> selectedTool; // Play hotbar, numbered 1–4 on the keyboard.
    int toolStep = 0;
    bool attack = false; // One swing per physical left click during play.
    bool interact = false;
    bool upgrade = false;
    bool closeMenu = false;
    bool inventory = false, inventorySort = false, lootAll = false;
    std::optional<int> inventoryClick, lootClick;
    bool inventoryPointerPressed = false;
    glm::vec2 inventoryPointer{0}; // Normalized window coordinates, like the creative menus.
    bool monsterPreview = false; // F3 animation workbench, from the hub.
    bool crossingMenu = false;
    std::optional<int> selectedCrossing; // 0–9 while the test picker is open.
    int crossingMenuStep = 0;
    int crossingStep = 0;
    bool returnToHub = false;
    bool restartCrossing = false;
    bool levelCreator = false, levelMenu = false; // F11 authoring / F4 expeditions.
    bool devToggle = false, devPalette = false, devTest = false, devAccept = false, devLibrary = false;
    bool devRotate = false, devVariation = false, devBackspace = false, devSelectAll = false;
    bool devTools = false; // G opens/closes the building tools menu.
    bool devCopy = false, devPaste = false; // Cmd/Ctrl+C/V, once per physical key press.
    bool devCancel = false, devCornerA = false, devCornerB = false; // X cancels; J/K select corners.
    bool devPastePin = false, devPasteLevel = false; // P pins a preview; L chooses its floor level.
    glm::ivec3 devPasteNudge{0}; // Arrow keys move sideways/forward; Page Up/Down move vertically.
    std::optional<int> devSlot, devClick;
    int devTabStep = 0;
    glm::ivec2 devMenuStep{0};
    std::string devText;
    bool devPointerPressed = false;
    glm::vec2 devPointer{0}; // Window-relative position, normalized to [0,1].
    // Editing requests are emitted by --editor and the section creator.
    bool save = false;
    bool load = false;
    bool undo = false;
    bool redo = false;
    bool remove = false;
    bool place = false;
    bool removePressed = false, placePressed = false; // Physical clicks, without creative brush repeat.
    bool pick = false;
    std::optional<Block> selectedBlock;
    int materialStep = 0; // Signed hotbar steps; positive scroll moves to the next slot.
};

// Trackpads emit fractional wheel ticks. Preserve those between frames, but drop
// excess whole ticks from a fast fling instead of letting the hotbar keep spinning.
class HotbarScroll {
public:
    static constexpr int MaxSteps = 4;
    int update(double offset)
    {
        if (!std::isfinite(offset)) return 0;
        const double total = remainder_ + offset;
        if (!std::isfinite(total)) { reset(); return 0; }
        constexpr double TickTolerance = 1e-9;
        const double whole = std::trunc(total + std::copysign(TickTolerance, total));
        remainder_ = total - whole;
        if (std::abs(remainder_) < TickTolerance) remainder_ = 0;
        return static_cast<int>(std::clamp(whole, -double(MaxSteps), double(MaxSteps)));
    }
    void reset() { remainder_ = 0; }

private:
    double remainder_ = 0;
};

struct EditShortcutKeys {
    bool z = false;
    bool y = false;
    bool modifier = false;
    bool shift = false;
    bool c = false;
    bool v = false;
};

struct EditShortcutActions {
    bool undo = false;
    bool redo = false;
    bool copy = false;
    bool paste = false;
};

class EditShortcuts {
public:
    EditShortcutActions update(EditShortcutKeys keys)
    {
        // Only a physical letter-key press creates an action. Releasing Shift
        // after redo must never turn the held Z key into a fresh undo action.
        const bool zPressed = keys.z && !previous_.z;
        const bool yPressed = keys.y && !previous_.y;
        const bool cPressed = keys.c && !previous_.c;
        const bool vPressed = keys.v && !previous_.v;
        previous_ = keys;
        return {keys.modifier && zPressed && !keys.shift,
                keys.modifier && (yPressed || (zPressed && keys.shift)),
                keys.modifier && cPressed, keys.modifier && vPressed};
    }

private:
    EditShortcutKeys previous_;
};

struct PasteShortcutKeys {
    bool pin = false, level = false;
    bool left = false, right = false, forward = false, back = false;
    bool raise = false, lower = false;
};

struct PasteShortcutActions {
    bool pin = false, level = false;
    glm::ivec3 nudge{0};
};

class PasteShortcuts {
public:
    PasteShortcutActions update(PasteShortcutKeys keys, bool enabled)
    {
        const auto pressed = [](bool down, bool previous) { return down && !previous; };
        const PasteShortcutActions actions{
            pressed(keys.pin, previous_.pin), pressed(keys.level, previous_.level),
            {int(pressed(keys.right, previous_.right)) - int(pressed(keys.left, previous_.left)),
             int(pressed(keys.raise, previous_.raise)) - int(pressed(keys.lower, previous_.lower)),
             int(pressed(keys.back, previous_.back)) - int(pressed(keys.forward, previous_.forward))}};
        // Track disabled frames too: closing a menu or releasing a modifier must
        // not turn a held navigation key into a new paste movement.
        previous_ = keys;
        return enabled ? actions : PasteShortcutActions{};
    }

private:
    PasteShortcutKeys previous_;
};

class Input {
public:
    explicit Input(GLFWwindow* window);
    ~Input();
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    void update(Camera& camera, float deltaSeconds, bool editorMode = true, bool modal = false,
                bool crossingPicker = false, bool devMode = false, bool devFlying = false,
                bool devTyping = false, bool inventoryModal = false);
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
        bool inventory = false, inventorySort = false;
        std::array<bool, 4> tools{};
        std::array<bool, 10> courses{};
        bool monsterPreview = false;
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
        bool devTools = false, devCancel = false, devCornerA = false, devCornerB = false;
        bool pasteLevel = false, pasteRaise = false, pasteLower = false;
    };

    bool keyDown(int key) const;
    Buttons readButtons() const;
    void capture(bool enabled);
    void updateCamera(Camera& camera, float deltaSeconds, bool editingShortcut, bool editorMode);
    void updateDev(Camera& camera, float deltaSeconds, const Buttons& buttons, const Buttons& previous,
                   bool modal, bool flying, bool typing, bool modifier, EditShortcutActions shortcuts,
                   PasteShortcutActions pasteShortcuts);

    GLFWwindow* window_;
    InputActions actions_;
    Buttons previousButtons_;
    EditShortcuts editShortcuts_;
    PasteShortcuts pasteShortcuts_;
    bool captured_ = false;
    bool wireframe_ = false;
    bool help_ = false;
    double cursorX_ = 0.0;
    double cursorY_ = 0.0;
    double scroll_ = 0.0;
    HotbarScroll hotbarScroll_;
    std::string pendingText_;
    bool wasDevMode_ = false, wasDevMenu_ = false;
    bool wasModal_ = false, wasEditorMode_ = true;
    bool wasInventoryModal_ = false;
    float removeRepeat_ = 0.0f, placeRepeat_ = 0.0f;
    float backspaceRepeat_ = 0.0f;
    bool suppressDevMouse_ = true, wasDevFlying_ = false;
};

} // namespace voxel
