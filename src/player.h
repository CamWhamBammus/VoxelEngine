#pragma once

#include <glm/vec3.hpp>

namespace voxel {
class World;

// Position is the center of the player's feet; the body stays upright.
class Player {
public:
    static constexpr float Width = 0.6f;
    static constexpr float Height = 1.8f;
    static constexpr float EyeHeight = 1.62f;

    const glm::vec3& feetPosition() const { return feet_; }
    glm::vec3 eyePosition() const { return feet_ + glm::vec3(0, EyeHeight, 0); }
    bool grounded() const { return grounded_; }
    bool overlapsBlock(glm::ivec3 position) const;

    // Teleport only when the entire body fits. Y may be outside the voxel world.
    bool setFeetPosition(const World& world, glm::vec3 position);
    // Resolve each axis independently so the player slides along walls.
    void move(const World& world, glm::vec3 displacement);

private:
    void moveAxis(const World& world, int axis, float distance);

    glm::vec3 feet_{0.5f, 16.0f, 0.5f};
    bool grounded_ = false;
};

} // namespace voxel
