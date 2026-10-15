#pragma once

#include <glm/glm.hpp>

#include <array>

namespace voxel {

class Frustum {
public:
    explicit Frustum(const glm::mat4& viewProjection);

    // Bounds touching the clip volume are visible. Invalid matrices or bounds
    // conservatively remain visible, so culling cannot hide their geometry.
    bool intersects(glm::vec3 minimum, glm::vec3 maximum) const;

private:
    std::array<glm::dvec4, 6> planes_{};
    bool valid_ = false;
};

} // namespace voxel
