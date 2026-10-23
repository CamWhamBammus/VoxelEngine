#pragma once

#include "mesh.h"
#include <glm/gtc/quaternion.hpp>
#include <string_view>

namespace voxel {

enum class MonsterKind { Skeleton, Wizard, Slime };
enum class MonsterAnimation { Idle, Walk, Attack };

// Models face +Z. Each part is drawn around its own joint; parents precede children.
struct MonsterPart {
    std::string_view name;
    int parent = -1;
    glm::vec3 pivot{0};
    MeshData mesh;
};
struct MonsterRig { std::vector<MonsterPart> parts; };
struct MonsterJoint {
    glm::vec3 translation{0};
    glm::vec3 rotation{0}; // Degrees, local XYZ Euler angles.
    glm::vec3 scale{1};
};
using MonsterPose = std::vector<MonsterJoint>;

MonsterRig buildSkeletonRig();
MonsterRig buildWizardRig();
MonsterRig buildSlimeRig();
MonsterPose poseSkeleton(MonsterAnimation animation, float seconds);
MonsterPose poseWizard(MonsterAnimation animation, float seconds);
MonsterPose poseSlime(MonsterAnimation animation, float seconds);

const MonsterRig& monsterRig(MonsterKind kind);
MonsterPose monsterPose(MonsterKind kind, MonsterAnimation animation, float seconds);
float monsterCycleDuration(MonsterKind kind, MonsterAnimation animation);
std::string_view monsterName(MonsterKind kind);

struct MonsterInstance {
    MonsterKind kind = MonsterKind::Skeleton;
    glm::vec3 position{0};
    float yaw = 0;
    MonsterAnimation animation = MonsterAnimation::Idle;
    float seconds = 0;
    MonsterPose transitionFrom;
    float transition = 1;
    float hitReaction = 0; // 1 at impact, returning to 0 as the recoil settles.
    glm::vec3 hitDirection{0, 0, 1}; // World direction away from the attacker.
};
MonsterPose blendedMonsterPose(const MonsterInstance& monster);
std::vector<glm::mat4> monsterMatrices(const MonsterInstance& monster);

} // namespace voxel
