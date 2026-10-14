#include "voxel/raycast.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace voxel {

std::optional<RayHit> raycast(glm::vec3 origin, glm::vec3 direction, float maxDistance,
                              const std::function<Block(glm::ivec3)>& sample)
{
    if (!sample || !std::isfinite(maxDistance) || maxDistance < 0.0f) {
        return std::nullopt;
    }

    glm::ivec3 position(0);
    std::array<double, 3> unit{};
    double lengthSquared = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(origin[axis]) || !std::isfinite(direction[axis])) {
            return std::nullopt;
        }
        const double coordinate = std::floor(static_cast<double>(origin[axis]));
        if (coordinate < std::numeric_limits<int>::min()
            || coordinate > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        position[axis] = static_cast<int>(coordinate);
        unit[axis] = direction[axis];
        lengthSquared += unit[axis] * unit[axis];
    }
    if (lengthSquared == 0.0) {
        return std::nullopt;
    }
    if (isSolid(sample(position))) {
        return RayHit{position, glm::ivec3(0), 0.0f};
    }

    const double length = std::sqrt(lengthSquared);
    std::array<int, 3> step{};
    std::array<double, 3> next{};
    std::array<double, 3> delta{};
    for (int axis = 0; axis < 3; ++axis) {
        unit[axis] /= length;
        if (unit[axis] == 0.0) {
            next[axis] = delta[axis] = std::numeric_limits<double>::infinity();
            continue;
        }
        step[axis] = unit[axis] > 0.0 ? 1 : -1;
        const double boundary = static_cast<double>(position[axis]) + (step[axis] > 0 ? 1.0 : 0.0);
        next[axis] = (boundary - static_cast<double>(origin[axis])) / unit[axis];
        delta[axis] = 1.0 / std::abs(unit[axis]);
    }

    while (true) {
        const double distance = std::min({next[0], next[1], next[2]});
        if (distance > static_cast<double>(maxDistance)) {
            return std::nullopt;
        }
        glm::ivec3 normal(0);
        bool hasNormal = false;
        for (int axis = 0; axis < 3; ++axis) {
            if (next[axis] != distance) {
                continue;
            }
            if ((step[axis] > 0 && position[axis] == std::numeric_limits<int>::max())
                || (step[axis] < 0 && position[axis] == std::numeric_limits<int>::min())) {
                return std::nullopt;
            }
            position[axis] += step[axis];
            next[axis] += delta[axis];
            if (!hasNormal) {
                normal[axis] = -step[axis];
                hasNormal = true;
            }
        }
        if (isSolid(sample(position))) {
            return RayHit{position, normal, static_cast<float>(distance)};
        }
    }
}

} // namespace voxel
