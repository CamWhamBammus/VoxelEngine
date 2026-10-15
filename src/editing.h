#pragma once

#include "world.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <optional>

namespace voxel {

struct RayHit {
    glm::ivec3 position;
    glm::ivec3 normal;
    float distance;
};

// Traverses voxels in world space; direction need not be normalized. Exact
// edge/corner crossings advance all tied axes, skipping cells touched only at
// an edge or point. The hit normal uses X, then Y, then Z to break these ties.
// A solid starting voxel returns distance zero and a zero normal.
std::optional<RayHit> raycast(glm::vec3 origin, glm::vec3 direction, float maxDistance,
                              const std::function<Block(glm::ivec3)>& sample);

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

bool removeBlock(World& world, const RayHit& hit, EditHistory* history = nullptr);
bool placeBlock(World& world, const RayHit& hit, Block block, glm::vec3 cameraPosition,
                EditHistory* history = nullptr);

} // namespace voxel
