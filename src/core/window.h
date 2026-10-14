#pragma once

struct GLFWwindow;

namespace voxel {

// Owns GLFW and its single OpenGL context. Destroy GPU resources before this.
class Window {
public:
    explicit Window(bool visible = true);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* handle() const { return window_; }
    bool shouldClose() const;
    void framebufferSize(int& width, int& height) const;
    void present() const;

private:
    GLFWwindow* window_ = nullptr;
};

} // namespace voxel
