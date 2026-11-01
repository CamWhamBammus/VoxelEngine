#include "game.h"
#include "heartwood.h"
#include "input.h"

#include <glm/geometric.hpp>

namespace voxel {

bool Game::nearSkillTree() const
{
    if (editorMode_ || devMode_ || monsterPreviewActive_ || levelController_.open()
        || rootCrossingActive() || health_.dead()) return false;
    const auto& hall = heartwoodLayout();
    if (glm::distance(player.feetPosition(), hall.approach) > 6.0f) return false;
    if (!isSolid(world.get(glm::ivec3(glm::floor(hall.seed)) - glm::ivec3(0,1,0)))) return false;
    const auto direction = hall.station - player.eyePosition();
    const float distance = glm::length(direction);
    if (distance > 7.0f) return false;
    if (distance < .001f) return true;
    const auto obstruction = raycast(player.eyePosition(), direction, distance,
        [this](glm::ivec3 point) { return world.get(point); });
    return !obstruction || obstruction->distance >= distance - .001f;
}

bool Game::updateSkills(const InputActions& actions)
{
    if (!skillsOpen_) {
        if (!actions.active || !actions.interact || menuOpen() || !nearSkillTree()) return false;
        skillsOpen_ = true;
        status.clear();
        return true;
    }
    if (!actions.active) return true;
    if (actions.closeMenu || actions.interact || !nearSkillTree()) {
        skillsOpen_ = false;
        return true;
    }
    if (actions.skillTab) skillJournal_.selectTab(*actions.skillTab);
    if (actions.skillTabStep) skillJournal_.cycleTab(actions.skillTabStep);
    if (actions.skillSelection) skillJournal_.selectSkill(*actions.skillSelection);
    skillJournal_.moveSelection(actions.skillMove);
    // Browsing pauses world input. Enter deliberately does not purchase or
    // activate anything: this screen is a preview of future abilities.
    return true;
}

} // namespace voxel
