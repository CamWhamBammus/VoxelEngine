#include "core/camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace voxel {
namespace {
const glm::vec3 worldUp{0.0f, 1.0f, 0.0f};

bool isFinite(const glm::vec3& vector)
{
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}
}

void Camera::setPosition(const glm::vec3& position)
{
    if (isFinite(position)) {
        position_ = position;
    }
}

void Camera::setFieldOfView(float degrees)
{
    if (std::isfinite(degrees)) {
        fieldOfViewDegrees_ = std::clamp(degrees, 25.0f, 100.0f);
    }
}

void Camera::reset()
{
    *this = Camera{};
}

glm::vec3 Camera::forward() const
{
    const float yaw = glm::radians(yawDegrees_);
    const float pitch = glm::radians(pitchDegrees_);
    return glm::normalize(glm::vec3{
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch),
    });
}

glm::mat4 Camera::viewMatrix() const
{
    return glm::lookAt(position_, position_ + forward(), worldUp);
}

glm::mat4 Camera::projectionMatrix(float aspectRatio) const
{
    if (!std::isfinite(aspectRatio) || aspectRatio <= 0.0f) {
        aspectRatio = 1.0f;
    }
    return glm::perspective(glm::radians(fieldOfViewDegrees_), aspectRatio, 0.1f, 500.0f);
}

void Camera::rotate(float yawDeltaDegrees, float pitchDeltaDegrees)
{
    if (!std::isfinite(yawDeltaDegrees) || !std::isfinite(pitchDeltaDegrees)) {
        return;
    }
    yawDegrees_ = std::remainder(yawDegrees_ + yawDeltaDegrees, 360.0f);
    pitchDegrees_ = std::clamp(pitchDegrees_ + pitchDeltaDegrees, -89.0f, 89.0f);
}

void Camera::move(glm::vec3 localDirection, float distance)
{
    if (!isFinite(localDirection) || !std::isfinite(distance)) {
        return;
    }
    const glm::vec3 facing = forward();
    const glm::vec3 right = glm::normalize(glm::cross(facing, worldUp));
    const glm::vec3 direction = right * localDirection.x
        + worldUp * localDirection.y + facing * localDirection.z;
    const float length = glm::length(direction);
    if (std::isfinite(length) && length > 0.0f) {
        setPosition(position_ + (direction / length) * distance);
    }
}

} // namespace voxel
