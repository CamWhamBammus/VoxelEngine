#include "core/frustum.h"

#include <cmath>

namespace voxel {

Frustum::Frustum(const glm::mat4& viewProjection)
{
    std::array<glm::dvec4, 4> rows{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            const float value = viewProjection[column][row];
            if (!std::isfinite(value)) return;
            rows[row][column] = value;
        }
    }
    const double determinant = glm::determinant(glm::dmat4(viewProjection));
    if (!std::isfinite(determinant) || determinant == 0.0) return;

    // OpenGL clips against -w <= x,y,z <= w; GLM stores matrix columns.
    planes_ = {rows[3] + rows[0], rows[3] - rows[0],
               rows[3] + rows[1], rows[3] - rows[1],
               rows[3] + rows[2], rows[3] - rows[2]};
    for (auto& plane : planes_) {
        const double length = glm::length(glm::dvec3(plane));
        if (!std::isfinite(length) || length == 0.0) return;
        plane /= length;
    }
    valid_ = true;
}

bool Frustum::intersects(glm::vec3 minimum, glm::vec3 maximum) const
{
    if (!valid_) return true;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis])
            || minimum[axis] > maximum[axis]) return true;
    }
    constexpr double boundaryTolerance = 1e-5;
    for (const auto& plane : planes_) {
        double distance = plane.w;
        for (int axis = 0; axis < 3; ++axis) {
            distance += plane[axis] * (plane[axis] >= 0.0 ? maximum[axis] : minimum[axis]);
        }
        if (distance < -boundaryTolerance) return false;
    }
    return true;
}

} // namespace voxel
