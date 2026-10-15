#pragma once

#include "voxel/block.h"

#include <cstddef>
#include <deque>

namespace voxel {

class World;

class EditHistory {
public:
    explicit EditHistory(std::size_t capacity = 256);

    bool apply(World& world, glm::ivec3 position, Block block);
    bool undo(World& world);
    bool redo(World& world);
    void clear();

    std::size_t undoCount() const { return undo_.size(); }
    std::size_t redoCount() const { return redo_.size(); }

private:
    struct Edit {
        glm::ivec3 position;
        Block before;
        Block after;
    };

    std::size_t capacity_;
    std::deque<Edit> undo_;
    std::deque<Edit> redo_;
};

} // namespace voxel
