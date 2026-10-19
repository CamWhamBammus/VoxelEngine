#pragma once

#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace voxel {
class World;
class AssetCollision;

// Position is the center of the player's feet; the body stays upright.
class Player {
public:
    static constexpr float Width = 0.6f;
    static constexpr float Height = 1.8f;
    static constexpr float EyeHeight = 1.62f;

    const glm::vec3& feetPosition() const { return feet_; }
    glm::vec3 eyePosition() const { return feet_ + glm::vec3(0, EyeHeight, 0); }
    bool grounded() const { return grounded_; }
    bool inWater() const { return inWater_; }
    bool underwater() const { return underwater_; }
    bool overlapsBlock(glm::ivec3 position) const;

    // Share the current scene collider; editor flight does not use Player.
    void setAssetCollision(std::shared_ptr<const AssetCollision> collision);

    // Teleport only when the entire body fits. Y may be outside the voxel world.
    bool setFeetPosition(const World& world, glm::vec3 position);
    // Sweep each axis to slide along walls. Grounded, non-rising movement can
    // step onto one-voxel stairs when the full body has clearance and support.
    void move(const World& world, glm::vec3 displacement);
    // Movement x is right and y is forward. Jump is held to swim up; descend
    // swims down. On land, a fresh jump press is still required for each jump.
    void update(const World& world, glm::vec2 movement, glm::vec3 forward,
                bool jump, bool sprint, float deltaSeconds, bool descend = false);
    // Stand on the nearest solid column. An empty world leaves the player unchanged.
    bool spawn(const World& world, glm::ivec2 preferred = glm::ivec2(0));

private:
    void moveAxis(const World& world, int axis, float distance);
    void refreshWater(const World& world);
    bool climbShore(const World& world, glm::vec3 remainingMovement);

    std::shared_ptr<const AssetCollision> assetCollision_;
    glm::vec3 feet_{0.5f, 16.0f, 0.5f};
    bool grounded_ = false;
    float verticalVelocity_ = 0.0f;
    bool jumpHeld_ = false;
    bool inWater_ = false;
    bool underwater_ = false;
    float waterSurface_ = 0.0f;
};

} // namespace voxel
