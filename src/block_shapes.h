#pragma once

#include "world.h"

namespace voxel {

struct BlockBox {
    glm::vec3 minimum;
    glm::vec3 maximum;
};

struct BlockShape {
    std::array<BlockBox, 2> boxes{};
    int count = 0;
};

// Shared by collision, selection, and meshing. Coordinates are local to the
// voxel, so rotating a stair never changes its one-cell building footprint.
inline BlockShape blockShape(Block block) noexcept
{
    if (!isSolid(block)) return {};
    BlockShape shape;
    shape.boxes[0] = {{0, 0, 0}, {1, 1, 1}};
    shape.count = 1;
    if (!isStair(block)) return shape;
    shape.boxes[0].maximum.y = .5f;
    shape.boxes[1] = {{0, .5f, 0}, {1, 1, 1}};
    shape.count = 2;
    auto& upper = shape.boxes[1];
    switch (stairFacing(block)) {
    case 0: upper.maximum.z = .5f; break;
    case 1: upper.maximum.x = .5f; break;
    case 2: upper.minimum.z = .5f; break;
    case 3: upper.minimum.x = .5f; break;
    }
    return shape;
}

} // namespace voxel
