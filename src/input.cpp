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
    capture(true);
}

Input::~Input()
{
    glfwSetScrollCallback(window_, nullptr);
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
    };
}

void Input::capture(bool enabled)
{
    captured_ = enabled;
    scroll_ = 0.0;
    glfwSetInputMode(window_, GLFW_CURSOR, enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(window_, &cursorX_, &cursorY_);
}

void Input::update(Camera& camera, float deltaSeconds, bool editorMode, bool modal)
{
    actions_ = {};
    if (!editorMode) wireframe_ = false;
    const Buttons buttons = readButtons();
    const Buttons previous = previousButtons_;
    previousButtons_ = buttons;

    const bool modifier = keyDown(GLFW_KEY_LEFT_CONTROL) || keyDown(GLFW_KEY_RIGHT_CONTROL)
        || keyDown(GLFW_KEY_LEFT_SUPER) || keyDown(GLFW_KEY_RIGHT_SUPER);
    const bool shift = keyDown(GLFW_KEY_LEFT_SHIFT) || keyDown(GLFW_KEY_RIGHT_SHIFT);
    const EditShortcutKeys editKeys{keyDown(GLFW_KEY_Z), keyDown(GLFW_KEY_Y), modifier, shift};
    const auto editActions = editShortcuts_.update(editKeys);

    if (keyDown(GLFW_KEY_Q)) glfwSetWindowShouldClose(window_, GLFW_TRUE);
    if (!glfwGetWindowAttrib(window_, GLFW_FOCUSED)) {
        if (captured_) capture(false);
        return;
    }

    if (!editorMode && modal) {
        // Closing the forge returns directly to play. Focus loss still releases
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
        actions_.pick = pressed(buttons.pick, previous.pick);
        actions_.materialStep = static_cast<int>(pressed(buttons.nextMaterial, previous.nextMaterial))
            - static_cast<int>(pressed(buttons.previousMaterial, previous.previousMaterial));
        for (int key = GLFW_KEY_1; key <= GLFW_KEY_9; ++key) {
            if (keyDown(key)) actions_.selectedBlock = PlaceableBlocks[static_cast<std::size_t>(key - GLFW_KEY_1)];
        }
    } else {
        actions_.interact = pressed(buttons.interact, previous.interact);
        actions_.upgrade = pressed(buttons.upgrade, previous.upgrade);
        for (int slot = 0; slot < 3; ++slot)
            if (pressed(buttons.tools[slot], previous.tools[slot])) actions_.selectedTool = slot;
        actions_.toolStep = static_cast<int>(pressed(buttons.nextMaterial, previous.nextMaterial))
            - static_cast<int>(pressed(buttons.previousMaterial, previous.previousMaterial));
        if (actions_.toolStep == 0) actions_.toolStep = scroll_ > 0.0 ? -1 : scroll_ < 0.0 ? 1 : 0;
        scroll_ = 0.0;
    }
    if (!editorMode && modal) {
        glfwGetCursorPos(window_, &cursorX_, &cursorY_);
        return;
    }
    // Reset wins over queued mouse motion, zoom, and held movement keys this frame.
    if (!resetCamera) updateCamera(camera, deltaSeconds, modifier && (editKeys.z || editKeys.y), editorMode);
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

    // Ctrl-Z/Ctrl-Y should edit history without also moving the camera down.
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
