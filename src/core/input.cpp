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
    const std::array<bool, 3> keys{down(GLFW_KEY_ESCAPE), down(GLFW_KEY_TAB), down(GLFW_KEY_F)};
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
    previous_ = keys;
    if (!captured_) return;

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
