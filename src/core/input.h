#pragma once

#include <array>
#include "voxel/block.h"

struct GLFWwindow;

namespace voxel {
class Camera;

class Input {
public:
    explicit Input(GLFWwindow* window);
    void update(Camera& camera, float deltaSeconds);
    bool captured() const { return captured_; }
    bool wireframe() const { return wireframe_; }
    bool removeRequested() const { return remove_; }
    bool placeRequested() const { return place_; }
    Block selectedBlock() const { return selected_; }
    bool saveRequested() const { return save_; }
    bool loadRequested() const { return load_; }

private:
    void capture(bool enabled);
    GLFWwindow* window_;
    bool captured_ = false;
    bool wireframe_ = false;
    bool remove_ = false;
    bool place_ = false;
    bool save_ = false;
    bool load_ = false;
    Block selected_ = Block::Grass;
    std::array<bool, 5> previous_{};
    std::array<bool, 2> previousMouse_{};
    double cursorX_ = 0.0;
    double cursorY_ = 0.0;
};

} // namespace voxel
