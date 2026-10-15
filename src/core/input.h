#pragma once

#include <array>
#include "voxel/block.h"

struct GLFWwindow;

namespace voxel {
class Camera;

class Input {
public:
    explicit Input(GLFWwindow* window);
    ~Input();
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    void update(Camera& camera, float deltaSeconds);
    bool captured() const { return captured_; }
    bool wireframe() const { return wireframe_; }
    bool removeRequested() const { return remove_; }
    bool placeRequested() const { return place_; }
    Block selectedBlock() const { return selected_; }
    bool saveRequested() const { return save_; }
    bool loadRequested() const { return load_; }
    bool undoRequested() const { return undo_; }
    bool redoRequested() const { return redo_; }
    bool pickRequested() const { return pick_; }
    void selectBlock(Block block) { if (isSolid(block)) selected_ = block; }

private:
    void capture(bool enabled);
    GLFWwindow* window_;
    bool captured_ = false;
    bool wireframe_ = false;
    bool remove_ = false;
    bool place_ = false;
    bool save_ = false;
    bool load_ = false;
    bool undo_ = false;
    bool redo_ = false;
    bool pick_ = false;
    Block selected_ = Block::Grass;
    std::array<bool, 8> previous_{};
    std::array<bool, 3> previousMouse_{};
    double cursorX_ = 0.0;
    double cursorY_ = 0.0;
    double scroll_ = 0.0;
};

} // namespace voxel
