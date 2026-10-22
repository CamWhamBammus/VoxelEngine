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

struct BlockSurface {
    glm::vec3 minimum;
    glm::vec3 maximum;
    glm::ivec3 normal;
};

struct BlockSurfaces {
    // A cube has six rectangles. A stair adds a riser and splits its top and
    // two L-shaped sides, giving ten rectangles without any internal faces.
    std::array<BlockSurface, 10> faces{};
    int count = 0;
};

// Extract the boundary of the collision shape on the half-block grid. This
// keeps surface decorations attached to the same treads and risers players
// walk on, without exposing the overlap between the stair's two boxes.
inline BlockSurfaces blockSurfaces(Block block) noexcept
{
    const auto shape = blockShape(block);
    BlockSurfaces result;
    if (shape.count < 2) {
        if (shape.count == 0) return result;
        // Cubes dominate the cavern. Keep their common six-face path cheap.
        for (int axis = 0; axis < 3; ++axis) {
            for (const int direction : {1, -1}) {
                glm::vec3 low(0), high(1);
                glm::ivec3 normal(0);
                low[axis] = high[axis] = direction > 0 ? 1.0f : 0.0f;
                normal[axis] = direction;
                result.faces[result.count++] = {low, high, normal};
            }
        }
        return result;
    }
    const auto contains = [&](glm::vec3 point) {
        for (int i = 0; i < shape.count; ++i) {
            const auto& box = shape.boxes[i];
            if (point.x > box.minimum.x && point.x < box.maximum.x
                && point.y > box.minimum.y && point.y < box.maximum.y
                && point.z > box.minimum.z && point.z < box.maximum.z) return true;
        }
        return false;
    };
    for (int axis = 0; axis < 3; ++axis) {
        const int u = axis == 0 ? 2 : 0, v = axis == 1 ? 2 : 1;
        for (const int direction : {1, -1}) {
            for (int plane = 0; plane <= 2; ++plane) {
                bool mask[2][2]{};
                glm::ivec3 normal(0);
                normal[axis] = direction;
                for (int y = 0; y < 2; ++y) {
                    for (int x = 0; x < 2; ++x) {
                        glm::vec3 point(0);
                        point[axis] = plane * .5f;
                        point[u] = x * .5f + .25f;
                        point[v] = y * .5f + .25f;
                        const auto offset = glm::vec3(normal) * .01f;
                        mask[y][x] = contains(point - offset) && !contains(point + offset);
                    }
                }
                for (int y = 0; y < 2; ++y) {
                    for (int x = 0; x < 2; ++x) {
                        if (!mask[y][x]) continue;
                        const int width = x == 0 && mask[y][1] ? 2 : 1;
                        const int height = y == 0 && mask[1][x] && (width == 1 || mask[1][1]) ? 2 : 1;
                        glm::vec3 low(0), high(0);
                        low[axis] = high[axis] = plane * .5f;
                        low[u] = x * .5f; high[u] = (x + width) * .5f;
                        low[v] = y * .5f; high[v] = (y + height) * .5f;
                        result.faces[result.count++] = {low, high, normal};
                        for (int j = y; j < y + height; ++j) {
                            for (int i = x; i < x + width; ++i) mask[j][i] = false;
                        }
                    }
                }
            }
        }
    }
    return result;
}

} // namespace voxel
