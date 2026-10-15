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
        .remove = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS,
        .place = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS,
        .pick = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS,
    };
}

void Input::capture(bool enabled)
{
    captured_ = enabled;
    scroll_ = 0.0;
    glfwSetInputMode(window_, GLFW_CURSOR, enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(window_, &cursorX_, &cursorY_);
}

void Input::update(Camera& camera, float deltaSeconds)
{
    actions_ = {};
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

    if (pressed(buttons.releaseMouse, previous.releaseMouse)) capture(false);
    if (pressed(buttons.toggleMouse, previous.toggleMouse)) capture(!captured_);
    if (pressed(buttons.wireframe, previous.wireframe)) wireframe_ = !wireframe_;
    if (pressed(buttons.resetCamera, previous.resetCamera)) camera.reset();
    if (pressed(buttons.help, previous.help)) help_ = !help_;
    actions_.save = pressed(buttons.save, previous.save);
    actions_.load = pressed(buttons.load, previous.load);
    actions_.undo = editActions.undo;
    actions_.redo = editActions.redo;

    // File/history shortcuts work with the cursor released. Movement, mouse
    // editing, and material selection below require mouse capture.
    if (!captured_) return;
    actions_.remove = pressed(buttons.remove, previous.remove);
    actions_.place = pressed(buttons.place, previous.place);
    actions_.pick = pressed(buttons.pick, previous.pick);
    for (std::size_t slot = 0; slot < PlaceableBlocks.size(); ++slot) {
        if (keyDown(GLFW_KEY_1 + static_cast<int>(slot))) actions_.selectedBlock = PlaceableBlocks[slot];
    }
    updateCamera(camera, deltaSeconds, modifier && (editKeys.z || editKeys.y));
}

void Input::updateCamera(Camera& camera, float deltaSeconds, bool editingShortcut)
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
    if (editingShortcut) return;
    const glm::vec3 direction{
        static_cast<float>(keyDown(GLFW_KEY_D) - keyDown(GLFW_KEY_A)),
        static_cast<float>(keyDown(GLFW_KEY_SPACE) - keyDown(GLFW_KEY_LEFT_CONTROL)),
        static_cast<float>(keyDown(GLFW_KEY_W) - keyDown(GLFW_KEY_S)),
    };
    const float speed = keyDown(GLFW_KEY_LEFT_SHIFT) ? 30.0f : 10.0f;
    camera.move(direction, speed * std::clamp(deltaSeconds, 0.0f, 0.1f));
}

} // namespace voxel
