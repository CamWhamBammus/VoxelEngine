#include "input.h"
#include "camera.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>

namespace voxel {
namespace {

bool pressed(bool down, bool previouslyDown)
{
    return down && !previouslyDown;
}

} // namespace

Input::Input(GLFWwindow* window) : window_(window)
{
    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, [](GLFWwindow* window, double, double y) {
        auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
        if (input && input->captured_ && glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
            input->scroll_ += y;
        }
    });
    glfwSetCharCallback(window_, [](GLFWwindow* window, unsigned int character) {
        auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
        if (input && character >= 32 && character < 127 && input->pendingText_.size() < 64)
            input->pendingText_.push_back(static_cast<char>(character));
    });
    capture(true);
}

Input::~Input()
{
    glfwSetScrollCallback(window_, nullptr);
    glfwSetCharCallback(window_, nullptr);
    glfwSetWindowUserPointer(window_, nullptr);
}

bool Input::keyDown(int key) const
{
    return glfwGetKey(window_, key) == GLFW_PRESS;
}

Input::Buttons Input::readButtons() const
{
    return {
        .releaseMouse = keyDown(GLFW_KEY_ESCAPE),
        .toggleMouse = keyDown(GLFW_KEY_TAB),
        .wireframe = keyDown(GLFW_KEY_F),
        .save = keyDown(GLFW_KEY_F5),
        .load = keyDown(GLFW_KEY_F9),
        .resetCamera = keyDown(GLFW_KEY_R),
        .help = keyDown(GLFW_KEY_H),
        .previousMaterial = keyDown(GLFW_KEY_LEFT_BRACKET),
        .nextMaterial = keyDown(GLFW_KEY_RIGHT_BRACKET),
        .jump = keyDown(GLFW_KEY_SPACE),
        .remove = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS,
        .place = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS,
        .pick = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS,
        .interact = keyDown(GLFW_KEY_E),
        .upgrade = keyDown(GLFW_KEY_ENTER) || keyDown(GLFW_KEY_KP_ENTER),
        .tools = {keyDown(GLFW_KEY_1), keyDown(GLFW_KEY_2), keyDown(GLFW_KEY_3)},
        .courses = {keyDown(GLFW_KEY_1), keyDown(GLFW_KEY_2), keyDown(GLFW_KEY_3),
                    keyDown(GLFW_KEY_4), keyDown(GLFW_KEY_5), keyDown(GLFW_KEY_6),
                    keyDown(GLFW_KEY_7), keyDown(GLFW_KEY_8), keyDown(GLFW_KEY_9), keyDown(GLFW_KEY_0)},
        .crossingMenu = keyDown(GLFW_KEY_F6),
        .menuPrevious = keyDown(GLFW_KEY_UP),
        .menuNext = keyDown(GLFW_KEY_DOWN),
        .previousCrossing = keyDown(GLFW_KEY_P),
        .nextCrossing = keyDown(GLFW_KEY_N),
        .returnToHub = keyDown(GLFW_KEY_B),
        .devToggle = keyDown(GLFW_KEY_F7),
        .devTest = keyDown(GLFW_KEY_F8),
        .devAccept = keyDown(GLFW_KEY_F10),
        .levelCreator = keyDown(GLFW_KEY_F11),
        .levelMenu = keyDown(GLFW_KEY_F4),
        .rotate = keyDown(GLFW_KEY_T),
        .variation = keyDown(GLFW_KEY_Y),
        .backspace = keyDown(GLFW_KEY_BACKSPACE),
        .selectAll = keyDown(GLFW_KEY_A),
        .menuLeft = keyDown(GLFW_KEY_LEFT),
        .menuRight = keyDown(GLFW_KEY_RIGHT),
        .devTools = keyDown(GLFW_KEY_G),
        .devCancel = keyDown(GLFW_KEY_X),
        .devCornerA = keyDown(GLFW_KEY_J),
        .devCornerB = keyDown(GLFW_KEY_K),
        .pasteLevel = keyDown(GLFW_KEY_L),
        .pasteRaise = keyDown(GLFW_KEY_PAGE_UP),
        .pasteLower = keyDown(GLFW_KEY_PAGE_DOWN),
    };
}

void Input::capture(bool enabled)
{
    captured_ = enabled;
    if (enabled) suppressDevMouse_ = true;
    scroll_ = 0.0;
    hotbarScroll_.reset();
    glfwSetInputMode(window_, GLFW_CURSOR, enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(window_, &cursorX_, &cursorY_);
}

void Input::update(Camera& camera, float deltaSeconds, bool editorMode, bool modal, bool crossingPicker,
                   bool devMode, bool devFlying, bool devTyping)
{
    actions_ = {};
    if (modal != wasModal_ || editorMode != wasEditorMode_ || devMode != wasDevMode_
        || devFlying != wasDevFlying_) {
        scroll_ = 0;
        hotbarScroll_.reset();
    }
    wasModal_ = modal;
    wasEditorMode_ = editorMode;
    if (!editorMode) wireframe_ = false;
    const Buttons buttons = readButtons();
    const Buttons previous = previousButtons_;
    previousButtons_ = buttons;

    const bool modifier = keyDown(GLFW_KEY_LEFT_CONTROL) || keyDown(GLFW_KEY_RIGHT_CONTROL)
        || keyDown(GLFW_KEY_LEFT_SUPER) || keyDown(GLFW_KEY_RIGHT_SUPER);
    const bool shift = keyDown(GLFW_KEY_LEFT_SHIFT) || keyDown(GLFW_KEY_RIGHT_SHIFT);
    const EditShortcutKeys editKeys{keyDown(GLFW_KEY_Z), keyDown(GLFW_KEY_Y), modifier, shift,
                                    keyDown(GLFW_KEY_C), keyDown(GLFW_KEY_V)};
    const auto editActions = editShortcuts_.update(editKeys);
    const bool focused = glfwGetWindowAttrib(window_, GLFW_FOCUSED);
    const auto pasteActions = pasteShortcuts_.update(
        {buttons.previousCrossing, buttons.pasteLevel, buttons.menuLeft, buttons.menuRight,
         buttons.menuPrevious, buttons.menuNext, buttons.pasteRaise, buttons.pasteLower},
        focused && captured_ && devMode && devFlying && !modal && !modifier);

    if (!devMode && keyDown(GLFW_KEY_Q)) glfwSetWindowShouldClose(window_, GLFW_TRUE);
    if (!focused) {
        if (captured_) capture(false);
        scroll_ = 0;
        hotbarScroll_.reset();
        pendingText_.clear();
        return;
    }
    actions_.devToggle = !editorMode && pressed(buttons.devToggle, previous.devToggle);
    actions_.levelCreator = !editorMode && pressed(buttons.levelCreator, previous.levelCreator);
    actions_.levelMenu = !editorMode && pressed(buttons.levelMenu, previous.levelMenu);
    if (devMode) {
        if (wasDevFlying_ != devFlying) suppressDevMouse_ = true;
        updateDev(camera, deltaSeconds, buttons, previous, modal, devFlying, devTyping, modifier, editActions, pasteActions);
        pendingText_.clear();
        wasDevMode_ = true;
        wasDevMenu_ = modal;
        wasDevFlying_ = devFlying;
        return;
    }
    if (wasDevMode_) capture(true);
    wasDevMode_ = wasDevMenu_ = wasDevFlying_ = false;
    pendingText_.clear();

    if (!editorMode && modal) {
        // Closing a menu returns directly to play. Focus loss still releases
        // the mouse, and Tab can recapture it without a stale pointer jump.
        actions_.closeMenu = pressed(buttons.releaseMouse, previous.releaseMouse)
            || pressed(buttons.toggleMouse, previous.toggleMouse);
        if (!captured_ && pressed(buttons.toggleMouse, previous.toggleMouse)) capture(true);
    } else {
        if (pressed(buttons.releaseMouse, previous.releaseMouse)) capture(false);
        if (pressed(buttons.toggleMouse, previous.toggleMouse)) capture(!captured_);
    }
    if (editorMode && pressed(buttons.wireframe, previous.wireframe)) wireframe_ = !wireframe_;
    const bool resetCamera = pressed(buttons.resetCamera, previous.resetCamera);
    if (resetCamera) {
        if (editorMode) camera.reset();
        scroll_ = 0.0;
        hotbarScroll_.reset();
        glfwGetCursorPos(window_, &cursorX_, &cursorY_);
    }
    if (pressed(buttons.help, previous.help)) help_ = !help_;
    if (editorMode) {
        actions_.save = pressed(buttons.save, previous.save);
        actions_.load = pressed(buttons.load, previous.load);
        actions_.undo = editActions.undo;
        actions_.redo = editActions.redo;
    }

    // Editor file/history shortcuts work with the cursor released. Movement, mouse
    // editing, and material selection below require mouse capture.
    if (!captured_) return;
    actions_.active = true;
    actions_.movement = {
        static_cast<float>(keyDown(GLFW_KEY_D) - keyDown(GLFW_KEY_A)),
        static_cast<float>(keyDown(GLFW_KEY_W) - keyDown(GLFW_KEY_S)),
    };
    actions_.jump = pressed(buttons.jump, previous.jump);
    actions_.sprint = keyDown(GLFW_KEY_LEFT_SHIFT);
    actions_.respawn = resetCamera;
    if (editorMode) {
        actions_.remove = pressed(buttons.remove, previous.remove);
        actions_.place = pressed(buttons.place, previous.place);
        actions_.removePressed = actions_.remove;
        actions_.placePressed = actions_.place;
        actions_.pick = pressed(buttons.pick, previous.pick);
        actions_.materialStep = static_cast<int>(pressed(buttons.nextMaterial, previous.nextMaterial))
            - static_cast<int>(pressed(buttons.previousMaterial, previous.previousMaterial));
        for (int key = GLFW_KEY_1; key <= GLFW_KEY_9; ++key) {
            if (keyDown(key)) actions_.selectedBlock = PlaceableBlocks[static_cast<std::size_t>(key - GLFW_KEY_1)];
        }
    } else {
        actions_.interact = pressed(buttons.interact, previous.interact);
        actions_.upgrade = pressed(buttons.upgrade, previous.upgrade);
        actions_.crossingMenu = pressed(buttons.crossingMenu, previous.crossingMenu);
        actions_.crossingStep = static_cast<int>(pressed(buttons.nextCrossing, previous.nextCrossing))
            - static_cast<int>(pressed(buttons.previousCrossing, previous.previousCrossing));
        actions_.returnToHub = pressed(buttons.returnToHub, previous.returnToHub);
        actions_.restartCrossing = resetCamera && shift;
        if (crossingPicker) {
            for (int course = 0; course < 10; ++course)
                if (pressed(buttons.courses[course], previous.courses[course])) actions_.selectedCrossing = course;
            actions_.crossingMenuStep = static_cast<int>(pressed(buttons.menuNext, previous.menuNext))
                - static_cast<int>(pressed(buttons.menuPrevious, previous.menuPrevious));
        }
        for (int slot = 0; slot < 3; ++slot)
            if (pressed(buttons.tools[slot], previous.tools[slot])) actions_.selectedTool = slot;
        actions_.toolStep = static_cast<int>(pressed(buttons.nextMaterial, previous.nextMaterial))
            - static_cast<int>(pressed(buttons.previousMaterial, previous.previousMaterial));
        const int scrollSteps = modal ? 0 : hotbarScroll_.update(scroll_);
        if (actions_.toolStep == 0) actions_.toolStep = scrollSteps;
        if (modal) { actions_.toolStep = 0; hotbarScroll_.reset(); }
        scroll_ = 0.0;
    }
    if (!editorMode && modal) {
        glfwGetCursorPos(window_, &cursorX_, &cursorY_);
        return;
    }
    // Reset wins over queued mouse motion, zoom, and held movement keys this frame.
    if (!resetCamera) updateCamera(camera, deltaSeconds, modifier && (editKeys.z || editKeys.y), editorMode);
}

void Input::updateDev(Camera& camera, float deltaSeconds, const Buttons& buttons, const Buttons& previous,
                      bool modal, bool flying, bool typing, bool modifier, EditShortcutActions shortcuts,
                      PasteShortcutActions pasteShortcuts)
{
    // Menus own an ordinary pointer. Returning to the room recaptures without
    // applying the pointer's menu movement to the camera.
    if (modal && captured_) capture(false);
    if (!modal && (!wasDevMode_ || wasDevMenu_)) capture(true);
    actions_.active = modal; // A focused menu accepts clicks and keys with the pointer released.
    actions_.devPalette = !typing && pressed(buttons.interact, previous.interact);
    actions_.devTest = pressed(buttons.devTest, previous.devTest);
    actions_.devAccept = pressed(buttons.devAccept, previous.devAccept);
    actions_.devLibrary = pressed(buttons.load, previous.load);
    actions_.save = pressed(buttons.save, previous.save);
    actions_.upgrade = pressed(buttons.upgrade, previous.upgrade);
    if (!typing) actions_.returnToHub = pressed(buttons.returnToHub, previous.returnToHub);
    if (modal) {
        suppressDevMouse_ = true;
        actions_.devTools = !typing && !modifier && pressed(buttons.devTools, previous.devTools);
        actions_.undo = !typing && shortcuts.undo;
        actions_.redo = !typing && shortcuts.redo;
        actions_.closeMenu = pressed(buttons.releaseMouse, previous.releaseMouse);
        actions_.devMenuStep = {
            static_cast<int>(pressed(buttons.menuRight, previous.menuRight)) - static_cast<int>(pressed(buttons.menuLeft, previous.menuLeft)),
            static_cast<int>(pressed(buttons.menuNext, previous.menuNext)) - static_cast<int>(pressed(buttons.menuPrevious, previous.menuPrevious))};
        actions_.devTabStep = static_cast<int>(pressed(buttons.nextMaterial, previous.nextMaterial))
            - static_cast<int>(pressed(buttons.previousMaterial, previous.previousMaterial));
        if (pressed(buttons.toggleMouse, previous.toggleMouse)) actions_.devTabStep = 1;
        if (typing) {
            if (!modifier) actions_.devText = pendingText_;
            actions_.devSelectAll = modifier && pressed(buttons.selectAll, previous.selectAll);
            if (buttons.backspace) {
                backspaceRepeat_ -= std::clamp(deltaSeconds, 0.0f, 0.1f);
                if (!previous.backspace || backspaceRepeat_ <= 0) {
                    actions_.devBackspace = true;
                    backspaceRepeat_ = previous.backspace ? 0.05f : 0.35f;
                }
            } else backspaceRepeat_ = 0;
        }
        actions_.devPointerPressed = pressed(buttons.remove, previous.remove);
        double x, y;
        int width, height;
        glfwGetCursorPos(window_, &x, &y);
        glfwGetWindowSize(window_, &width, &height);
        actions_.devPointer = {width > 0 ? float(x / width) : 0, height > 0 ? float(y / height) : 0};
        scroll_ = 0;
        hotbarScroll_.reset();
        removeRepeat_ = placeRepeat_ = 0;
    } else {
        if (pressed(buttons.releaseMouse, previous.releaseMouse)) capture(false);
        if (pressed(buttons.toggleMouse, previous.toggleMouse)) capture(!captured_);
        if (pressed(buttons.help, previous.help)) help_ = !help_;
        actions_.undo = shortcuts.undo;
        actions_.redo = shortcuts.redo;
        if (!captured_) return;
        actions_.active = true;
        if (flying) {
            actions_.devTools = !modifier && pressed(buttons.devTools, previous.devTools);
            actions_.devCopy = shortcuts.copy;
            actions_.devPaste = shortcuts.paste;
            actions_.devCancel = !modifier && pressed(buttons.devCancel, previous.devCancel);
            actions_.devCornerA = !modifier && pressed(buttons.devCornerA, previous.devCornerA);
            actions_.devCornerB = !modifier && pressed(buttons.devCornerB, previous.devCornerB);
            actions_.devPastePin = pasteShortcuts.pin;
            actions_.devPasteLevel = pasteShortcuts.level;
            actions_.devPasteNudge = pasteShortcuts.nudge;
        }
        actions_.movement = {float(keyDown(GLFW_KEY_D) - keyDown(GLFW_KEY_A)), float(keyDown(GLFW_KEY_W) - keyDown(GLFW_KEY_S))};
        actions_.jump = pressed(buttons.jump, previous.jump);
        actions_.sprint = keyDown(GLFW_KEY_LEFT_SHIFT);
        actions_.respawn = pressed(buttons.resetCamera, previous.resetCamera);
        actions_.restartCrossing = actions_.respawn && actions_.sprint;
        actions_.devRotate = !modifier && pressed(buttons.rotate, previous.rotate);
        actions_.devVariation = !modifier && pressed(buttons.variation, previous.variation);
        const auto repeating = [&](bool down, bool wasDown, float& countdown) {
            if (!down) { countdown = 0; return false; }
            countdown -= std::clamp(deltaSeconds, 0.0f, 0.1f);
            if (!wasDown || countdown <= 0) { countdown = wasDown ? 0.16f : 0.35f; return true; }
            return false;
        };
        const bool clipboardShortcut = modifier && (keyDown(GLFW_KEY_C) || keyDown(GLFW_KEY_V));
        if (clipboardShortcut) suppressDevMouse_ = true;
        if (suppressDevMouse_ || clipboardShortcut) {
            if (!clipboardShortcut && !buttons.remove && !buttons.place && !buttons.pick) suppressDevMouse_ = false;
            removeRepeat_ = placeRepeat_ = 0;
        } else {
            actions_.pick = pressed(buttons.pick, previous.pick);
            actions_.removePressed = pressed(buttons.remove, previous.remove);
            actions_.placePressed = pressed(buttons.place, previous.place);
            actions_.remove = repeating(buttons.remove, previous.remove, removeRepeat_);
            actions_.place = repeating(buttons.place, previous.place, placeRepeat_);
        }
        actions_.materialStep = hotbarScroll_.update(scroll_);
        scroll_ = 0;
        // Dev hotbar consumes scrolling; camera FOV stays fixed while building.
        updateCamera(camera, deltaSeconds, clipboardShortcut || (modifier && (keyDown(GLFW_KEY_Z) || keyDown(GLFW_KEY_Y))), flying);
    }
    if (!typing)
        for (int slot = 0; slot < 9; ++slot)
            if (pressed(buttons.courses[slot], previous.courses[slot])) actions_.devSlot = slot;
}

void Input::updateCamera(Camera& camera, float deltaSeconds, bool editingShortcut, bool editorMode)
{
    camera.setFieldOfView(camera.fieldOfView() - static_cast<float>(scroll_) * 2.0f);
    scroll_ = 0.0;
    double x, y;
    glfwGetCursorPos(window_, &x, &y);
    camera.rotate(static_cast<float>(x - cursorX_) * 0.10f,
                  static_cast<float>(cursorY_ - y) * 0.10f);
    cursorX_ = x;
    cursorY_ = y;

    // Ctrl shortcuts should edit history or the clipboard without moving down.
    if (!editorMode || editingShortcut) return;
    const glm::vec3 direction{
        static_cast<float>(keyDown(GLFW_KEY_D) - keyDown(GLFW_KEY_A)),
        static_cast<float>(keyDown(GLFW_KEY_SPACE) - keyDown(GLFW_KEY_LEFT_CONTROL)),
        static_cast<float>(keyDown(GLFW_KEY_W) - keyDown(GLFW_KEY_S)),
    };
    float speed = 10.0f;
    if (keyDown(GLFW_KEY_LEFT_SHIFT)) speed = 30.0f;
    // Precision movement takes priority if Alt and Shift are both held.
    if (keyDown(GLFW_KEY_LEFT_ALT)) speed = 2.0f;
    camera.move(direction, speed * std::clamp(deltaSeconds, 0.0f, 0.1f));
}

} // namespace voxel
