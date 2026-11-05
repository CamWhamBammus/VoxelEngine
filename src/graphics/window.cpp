#include "graphics/window.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace voxel {

Window::Window(bool visible)
{
    glfwSetErrorCallback([](int code, const char* message) {
        std::cerr << "GLFW error " << code << ": " << message << '\n';
    });
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
    window_ = glfwCreateWindow(1280, 720, "Voxel Engine", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create an OpenGL 4.1 window");
    }
    glfwMakeContextCurrent(window_);
    glewExperimental = GL_TRUE;
    const GLenum result = glewInit();
    if (result != GLEW_OK) {
        const std::string message = reinterpret_cast<const char*>(glewGetErrorString(result));
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLEW: " + message);
    }
    // GLEW may probe a legacy extension query in a core profile.
    while (glGetError() != GL_NO_ERROR) {}
    vsync_ = visible;
    glfwSwapInterval(vsync_ ? 1 : 0);
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow*, int width, int height) {
        glViewport(0, 0, width, height);
    });
    int width, height;
    framebufferSize(width, height);
    glViewport(0, 0, width, height);
}

Window::~Window()
{
    glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_); }
void Window::framebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(window_, &width, &height);
}
void Window::present() const { glfwSwapBuffers(window_); }

bool Window::fullscreen() const { return glfwGetWindowMonitor(window_) != nullptr; }

void Window::setVsync(bool enabled)
{
    if (vsync_ == enabled) return;
    glfwSwapInterval(enabled ? 1 : 0);
    vsync_ = enabled;
}

bool Window::setFullscreen(bool enabled)
{
    if (fullscreen() == enabled) return true;
    if (!enabled) {
        glfwSetWindowMonitor(window_, nullptr, windowedX_, windowedY_,
                             windowedWidth_, windowedHeight_, GLFW_DONT_CARE);
        if (fullscreen()) return false;
        if (windowedMaximized_) glfwMaximizeWindow(window_);
        return true;
    }

    int x, y, width, height;
    glfwGetWindowPos(window_, &x, &y);
    glfwGetWindowSize(window_, &width, &height);
    int monitorCount = 0;
    auto monitors = glfwGetMonitors(&monitorCount);
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    int largestOverlap = 0;
    // Follow the monitor containing most of the window, instead of always
    // moving a game opened on an external screen to the laptop display.
    for (int i = 0; i < monitorCount; ++i) {
        const auto mode = glfwGetVideoMode(monitors[i]);
        if (!mode) continue;
        int monitorX, monitorY;
        glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);
        const int overlapWidth = std::max(0, std::min(x + width, monitorX + mode->width) - std::max(x, monitorX));
        const int overlapHeight = std::max(0, std::min(y + height, monitorY + mode->height) - std::max(y, monitorY));
        const int overlap = overlapWidth * overlapHeight;
        if (overlap > largestOverlap) { monitor = monitors[i]; largestOverlap = overlap; }
    }
    if (!monitor) return false;
    const auto mode = glfwGetVideoMode(monitor);
    if (!mode) return false;
    windowedX_ = x; windowedY_ = y;
    windowedWidth_ = std::max(width, 1); windowedHeight_ = std::max(height, 1);
    windowedMaximized_ = glfwGetWindowAttrib(window_, GLFW_MAXIMIZED) == GLFW_TRUE;
    glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    return fullscreen();
}

} // namespace voxel
