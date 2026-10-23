#pragma once

#include <glm/glm.hpp>

#include <array>

namespace voxel {

class Camera {
public:
    const glm::vec3& position() const { return position_; }
    void setPosition(const glm::vec3& position);
    float fieldOfView() const { return fieldOfViewDegrees_; }
    void setFieldOfView(float degrees);
    // Visual tilt only; aiming and movement still use the unrolled forward.
    void setRoll(float degrees);
    void reset();

    glm::vec3 forward() const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspectRatio) const;

    void rotate(float yawDeltaDegrees, float pitchDeltaDegrees);
    // Local x is right, y is world up, and z is forward.
    void move(glm::vec3 localDirection, float distance);

private:
    glm::vec3 position_{28.0f, 26.0f, 40.0f};
    float yawDegrees_{-125.0f};
    float pitchDegrees_{-25.0f};
    float fieldOfViewDegrees_{70.0f};
    float rollDegrees_ = 0;
};

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
