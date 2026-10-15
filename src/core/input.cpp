#include "core/input.h"
#include "core/camera.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>

namespace voxel {

Input::Input(GLFWwindow* window) : window_(window) { capture(true); }

void Input::capture(bool enabled)
{
    captured_ = enabled;
    glfwSetInputMode(window_, GLFW_CURSOR, enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(window_, &cursorX_, &cursorY_);
}

void Input::update(Camera& camera, float deltaSeconds)
{
    const auto down = [this](int key) { return glfwGetKey(window_, key) == GLFW_PRESS; };
    const std::array<bool, 2> mouse{
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS,
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS,
    };
    const bool removePressed = mouse[0] && !previousMouse_[0];
    const bool placePressed = mouse[1] && !previousMouse_[1];
    previousMouse_ = mouse;
    remove_ = place_ = false;
    save_ = load_ = false;
    const std::array<bool, 5> keys{down(GLFW_KEY_ESCAPE), down(GLFW_KEY_TAB), down(GLFW_KEY_F),
                                  down(GLFW_KEY_F5), down(GLFW_KEY_F9)};
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
    previous_ = keys;
    if (!captured_) return;
    remove_ = removePressed;
    place_ = placePressed;
    for (std::size_t slot = 0; slot < PlaceableBlocks.size(); ++slot) {
        if (down(GLFW_KEY_1 + static_cast<int>(slot))) selected_ = PlaceableBlocks[slot];
    }

    double x, y;
    glfwGetCursorPos(window_, &x, &y);
    camera.rotate(static_cast<float>(x - cursorX_) * 0.10f,
                  static_cast<float>(cursorY_ - y) * 0.10f);
    cursorX_ = x;
    cursorY_ = y;
    const glm::vec3 direction{
        static_cast<float>(down(GLFW_KEY_D) - down(GLFW_KEY_A)),
        static_cast<float>(down(GLFW_KEY_SPACE) - down(GLFW_KEY_LEFT_CONTROL)),
        static_cast<float>(down(GLFW_KEY_W) - down(GLFW_KEY_S)),
    };
    const float speed = down(GLFW_KEY_LEFT_SHIFT) ? 30.0f : 10.0f;
    camera.move(direction, speed * std::clamp(deltaSeconds, 0.0f, 0.1f));
}

} // namespace voxel
