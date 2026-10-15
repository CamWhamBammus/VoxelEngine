#include "graphics/window.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

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
    glfwSwapInterval(visible ? 1 : 0);
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

} // namespace voxel
