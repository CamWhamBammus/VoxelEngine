#pragma once

#include <array>

struct GLFWwindow;

namespace voxel {
class Camera;

class Input {
public:
    explicit Input(GLFWwindow* window);
    void update(Camera& camera, float deltaSeconds);
    bool captured() const { return captured_; }
    bool wireframe() const { return wireframe_; }

private:
    void capture(bool enabled);
    GLFWwindow* window_;
    bool captured_ = false;
    bool wireframe_ = false;
    std::array<bool, 3> previous_{};
    double cursorX_ = 0.0;
    double cursorY_ = 0.0;
};

} // namespace voxel
