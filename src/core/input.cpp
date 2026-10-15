#include "core/input.h"
#include "core/camera.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>

namespace voxel {

Input::Input(GLFWwindow* window) : window_(window)
{
    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, [](GLFWwindow* window, double, double y) {
        auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
        if (input && input->captured_ && glfwGetWindowAttrib(window, GLFW_FOCUSED)) input->scroll_ += y;
    });
    capture(true);
}

Input::~Input()
{
    glfwSetScrollCallback(window_, nullptr);
    glfwSetWindowUserPointer(window_, nullptr);
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
    const auto down = [this](int key) { return glfwGetKey(window_, key) == GLFW_PRESS; };
    const std::array<bool, 3> mouse{
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS,
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS,
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS,
    };
    const bool removePressed = mouse[0] && !previousMouse_[0];
    const bool placePressed = mouse[1] && !previousMouse_[1];
    const bool pickPressed = mouse[2] && !previousMouse_[2];
    previousMouse_ = mouse;
    remove_ = place_ = false;
    save_ = load_ = undo_ = redo_ = pick_ = false;
    const bool shortcut = down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL)
        || down(GLFW_KEY_LEFT_SUPER) || down(GLFW_KEY_RIGHT_SUPER);
    const bool shift = down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT);
    const EditShortcutKeys editKeys{down(GLFW_KEY_Z), down(GLFW_KEY_Y), shortcut, shift};
    const auto editActions = editShortcuts_.update(editKeys);
    const std::array<bool, 7> keys{down(GLFW_KEY_ESCAPE), down(GLFW_KEY_TAB), down(GLFW_KEY_F),
        down(GLFW_KEY_F5), down(GLFW_KEY_F9), down(GLFW_KEY_R), down(GLFW_KEY_H)};
    if (down(GLFW_KEY_Q)) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
    if (!glfwGetWindowAttrib(window_, GLFW_FOCUSED)) {
        if (captured_) capture(false);
        previous_ = keys;
        return;
    }
    if (keys[0] && !previous_[0]) capture(false);
    if (keys[1] && !previous_[1]) capture(!captured_);
    if (keys[2] && !previous_[2]) wireframe_ = !wireframe_;
    save_ = keys[3] && !previous_[3];
    load_ = keys[4] && !previous_[4];
    undo_ = editActions.undo;
    redo_ = editActions.redo;
    if (keys[5] && !previous_[5]) camera.reset();
    if (keys[6] && !previous_[6]) help_ = !help_;
    previous_ = keys;
    if (!captured_) return;
    camera.setFieldOfView(camera.fieldOfView() - static_cast<float>(scroll_) * 2.0f);
    scroll_ = 0.0;
    remove_ = removePressed;
    place_ = placePressed;
    pick_ = pickPressed;
    for (std::size_t slot = 0; slot < PlaceableBlocks.size(); ++slot) {
        if (down(GLFW_KEY_1 + static_cast<int>(slot))) selected_ = PlaceableBlocks[slot];
    }

    double x, y;
    glfwGetCursorPos(window_, &x, &y);
    camera.rotate(static_cast<float>(x - cursorX_) * 0.10f,
                  static_cast<float>(cursorY_ - y) * 0.10f);
    cursorX_ = x;
    cursorY_ = y;
    if (shortcut && (editKeys.z || editKeys.y)) return;
    const glm::vec3 direction{
        static_cast<float>(down(GLFW_KEY_D) - down(GLFW_KEY_A)),
        static_cast<float>(down(GLFW_KEY_SPACE) - down(GLFW_KEY_LEFT_CONTROL)),
        static_cast<float>(down(GLFW_KEY_W) - down(GLFW_KEY_S)),
    };
    const float speed = down(GLFW_KEY_LEFT_SHIFT) ? 30.0f : 10.0f;
    camera.move(direction, speed * std::clamp(deltaSeconds, 0.0f, 0.1f));
}

} // namespace voxel
