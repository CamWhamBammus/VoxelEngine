#pragma once

#include "voxel/block.h"

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

} // namespace voxel
