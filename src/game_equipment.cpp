#include "game.h"
#include "scene.h"

#include <exception>
#include <glm/geometric.hpp>

namespace voxel {

bool Game::nearBlacksmith() const
{
    if (editorMode_ || devMode_ || monsterPreviewActive_ || levelController_.open() || rootCrossingActive()) return false;
    const auto station = sceneBlacksmithStation();
    const auto support = glm::ivec3(glm::floor(station)) - glm::ivec3(0, 1, 0);
    if (!isSolid(world.get(support)) || !isSolid(world.get(support - glm::ivec3(0, 1, 0)))) return false;
    const auto delta = station - player.eyePosition();
    const float distance = glm::length(delta);
    if (distance > 4.0f || glm::distance(player.feetPosition(), sceneBlacksmithApproach()) > 4.0f) return false;
    if (distance < .001f) return true;
    const auto obstruction = raycast(player.eyePosition(), delta, distance,
        [this](glm::ivec3 position) { return world.get(position); });
    return !obstruction || obstruction->distance >= distance - .001f;
}

void Game::upgradeEquipment()
{
    if (!progressError_.empty()) { status = progressError_; return; }
    auto upgraded = equipment;
    switch (upgradeGear(upgraded, upgradeSelection_)) {
    case UpgradeResult::MaxTier: status = std::string(gearSlotName(upgradeSelection_)) + " is fully upgraded"; return;
    case UpgradeResult::InsufficientResources: status = "Not enough ore for this upgrade"; return;
    case UpgradeResult::Upgraded: break;
    }
    try {
        // Commit the profile before charging the in-memory inventory. A failed
        // write leaves both the old equipment and existing save intact.
        if (!progressPath_.empty()) saveEquipment(upgraded, progressPath_);
        equipment = upgraded;
        status = gearName(equipment, upgradeSelection_) + " equipped";
    } catch (const std::exception& error) {
        status = std::string("Upgrade not saved: ") + error.what();
    }
}

bool Game::nearArmory() const
{
    if (editorMode_ || devMode_ || monsterPreviewActive_ || levelController_.open() || rootCrossingActive()) return false;
    const auto approach = sceneArmoryApproach();
    const auto station = sceneArmoryStation();
    if (!isSolid(world.get(glm::ivec3(glm::floor(approach)) - glm::ivec3(0,1,0)))) return false;
    const auto delta = station - player.eyePosition();
    const float distance = glm::length(delta);
    if (distance > 4.0f || glm::distance(player.feetPosition(), approach) > 4.0f) return false;
    if (distance < .001f) return true;
    const auto obstruction = raycast(player.eyePosition(), delta, distance,
        [this](glm::ivec3 position) { return world.get(position); });
    return !obstruction || obstruction->distance >= distance - .001f;
}

} // namespace voxel
