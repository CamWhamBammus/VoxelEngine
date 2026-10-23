#include "monsters.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace voxel {

const MonsterRig& monsterRig(MonsterKind kind)
{
    switch (kind) {
    case MonsterKind::Skeleton: { static const auto rig = buildSkeletonRig(); return rig; }
    case MonsterKind::Wizard: { static const auto rig = buildWizardRig(); return rig; }
    case MonsterKind::Slime: { static const auto rig = buildSlimeRig(); return rig; }
    }
    throw std::invalid_argument("Unknown monster kind");
}

MonsterPose monsterPose(MonsterKind kind, MonsterAnimation animation, float seconds)
{
    if (!std::isfinite(seconds)) seconds = 0;
    switch (kind) {
    case MonsterKind::Skeleton: return poseSkeleton(animation, seconds);
    case MonsterKind::Wizard: return poseWizard(animation, seconds);
    case MonsterKind::Slime: return poseSlime(animation, seconds);
    }
    throw std::invalid_argument("Unknown monster kind");
}

std::string_view monsterName(MonsterKind kind)
{
    switch (kind) {
    case MonsterKind::Skeleton: return "Scrapbone";
    case MonsterKind::Wizard: return "Hollow sage";
    case MonsterKind::Slime: return "Mossdrop";
    }
    return "Unknown";
}

float monsterCycleDuration(MonsterKind kind, MonsterAnimation animation)
{
    // Idle can blend at any time; actions finish a complete footstep or attack.
    if (animation == MonsterAnimation::Idle) return 0;
    switch (kind) {
    case MonsterKind::Skeleton: return animation == MonsterAnimation::Walk ? 1.1f : 2.4f;
    case MonsterKind::Wizard: return animation == MonsterAnimation::Walk ? 1.6f : 3.2f;
    case MonsterKind::Slime: return animation == MonsterAnimation::Walk ? 1.5f : 3.2f;
    }
    return 0;
}

MonsterPose blendedMonsterPose(const MonsterInstance& monster)
{
    auto pose = monsterPose(monster.kind, monster.animation, monster.seconds);
    if (monster.transitionFrom.size() != pose.size()) return pose;
    const float t = std::clamp(monster.transition, 0.0f, 1.0f);
    const float blend = t * t * (3.0f - 2.0f * t);
    for (std::size_t i = 0; i < pose.size(); ++i) {
        const auto& from = monster.transitionFrom[i];
        pose[i].translation = glm::mix(from.translation, pose[i].translation, blend);
        pose[i].scale = glm::mix(from.scale, pose[i].scale, blend);
        // Quaternion interpolation keeps shoulders and staffs from taking the
        // long way around when an animation is changed halfway through a swing.
        const auto rotation = glm::slerp(glm::quat(glm::radians(from.rotation)),
                                         glm::quat(glm::radians(pose[i].rotation)), blend);
        pose[i].rotation = glm::degrees(glm::eulerAngles(rotation));
    }
    return pose;
}

std::vector<glm::mat4> monsterMatrices(const MonsterInstance& monster)
{
    const auto& rig = monsterRig(monster.kind);
    const auto pose = blendedMonsterPose(monster);
    if (pose.size() != rig.parts.size()) throw std::logic_error("Monster pose does not match its rig");
    auto origin = glm::translate(glm::mat4(1), monster.position);
    const float reaction = std::isfinite(monster.hitReaction)
        ? std::clamp(monster.hitReaction, 0.0f, 1.0f) : 0.0f;
    if (reaction > 0) {
        // Lean in world space so a side or rear strike bends the body away
        // from that strike, independently of which way the creature faces.
        auto direction = glm::vec3(monster.hitDirection.x, 0, monster.hitDirection.z);
        const float length = glm::length(direction);
        direction = std::isfinite(length) && length > .001f ? direction/length : glm::vec3(0,0,1);
        const float strength = reaction * reaction * (3.0f - 2.0f * reaction);
        const float lean = monster.kind == MonsterKind::Slime ? 5.0f
            : monster.kind == MonsterKind::Wizard ? 6.0f : 8.0f;
        origin = glm::rotate(origin, glm::radians(lean * strength), glm::cross(glm::vec3(0,1,0),direction));
        if (monster.kind == MonsterKind::Slime)
            origin = glm::scale(origin, glm::vec3(1 + .13f*strength, 1 - .17f*strength, 1 + .13f*strength));
    }
    origin = glm::rotate(origin, glm::radians(monster.yaw), glm::vec3(0,1,0));
    std::vector<glm::mat4> matrices;
    matrices.reserve(rig.parts.size());
    for (std::size_t i = 0; i < rig.parts.size(); ++i) {
        const auto& part = rig.parts[i];
        const auto& joint = pose[i];
        if (part.parent >= static_cast<int>(i) || part.parent < -1)
            throw std::logic_error("Monster parents must precede their children");
        const auto parent = part.parent < 0 ? origin : matrices[part.parent];
        matrices.push_back(glm::translate(parent, part.pivot + joint.translation)
            * glm::mat4_cast(glm::quat(glm::radians(joint.rotation)))
            * glm::scale(glm::mat4(1), joint.scale));
    }
    return matrices;
}

} // namespace voxel
