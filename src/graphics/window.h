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
    // Switching display modes keeps the last windowed position and size.
    // Returns false when no display is available or GLFW cannot switch modes.
    bool setFullscreen(bool enabled);
    bool fullscreen() const;
    void setVsync(bool enabled);
    bool vsync() const { return vsync_; }

private:
    GLFWwindow* window_ = nullptr;
    int windowedX_ = 0, windowedY_ = 0;
    int windowedWidth_ = 1280, windowedHeight_ = 720;
    bool windowedMaximized_ = false;
    bool vsync_ = true;
};

} // namespace voxel
