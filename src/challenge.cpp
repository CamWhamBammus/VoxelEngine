#include "challenge.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace voxel {
namespace {

bool finite(glm::vec3 position)
{
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

bool withinReach(glm::vec3 eye, glm::vec3 marker)
{
    const double x = static_cast<double>(eye.x) - marker.x;
    const double y = static_cast<double>(eye.y) - marker.y;
    const double z = static_cast<double>(eye.z) - marker.z;
    return x * x + y * y + z * z <= Challenge::InteractionReach * Challenge::InteractionReach;
}

glm::vec3 crystalPosition(const World& world, int x, int z)
{
    // Markers float just above the actual surface, including edited columns.
    for (int y = World::MaxExclusive.y - 1; y >= World::Min.y; --y) {
        if (isSolid(world.get({x, y, z}))) {
            return {x + 0.5f, y + 1.8f, z + 0.5f};
        }
    }
    throw std::invalid_argument("Beacon Trail needs terrain beneath every crystal");
}

} // namespace

void Challenge::reset(const World& world, glm::vec3 camp)
{
    if (!finite(camp)) {
        throw std::invalid_argument("Beacon Trail camp position must be finite");
    }
    // Fixed columns spread the trail across the map; the seed determines heights.
    const std::array<Crystal, 3> crystals{{
        {crystalPosition(world, -20, -18), false},
        {crystalPosition(world, 30, -12), false},
        {crystalPosition(world, -10, 32), false},
    }};
    crystals_ = crystals;
    camp_ = camp;
    state_ = ChallengeState::Playing;
    timeRemaining_ = TimeLimit;
}

Interaction Challenge::interact(glm::vec3 eye)
{
    if (state_ != ChallengeState::Playing) {
        return Interaction::Finished;
    }
    if (!finite(eye)) {
        return Interaction::None;
    }
    // A nearby crystal takes priority even if camp happens to overlap it.
    for (auto& crystal : crystals_) {
        if (!crystal.collected && withinReach(eye, crystal.position)) {
            crystal.collected = true;
            return Interaction::Collected;
        }
    }
    if (withinReach(eye, camp_)) {
        if (collectedCount() == static_cast<int>(crystals_.size())) {
            state_ = ChallengeState::Won;
            return Interaction::Won;
        }
        return Interaction::NeedCrystals;
    }
    return Interaction::None;
}

int Challenge::collectedCount() const
{
    int count = 0;
    for (const auto& crystal : crystals_) {
        if (crystal.collected) ++count;
    }
    return count;
}

void Challenge::update(float seconds, bool paused)
{
    if (paused || state_ != ChallengeState::Playing || !std::isfinite(seconds) || seconds <= 0.0f) {
        return;
    }
    timeRemaining_ = std::max(0.0f, timeRemaining_ - seconds);
    if (timeRemaining_ == 0.0f) {
        state_ = ChallengeState::Lost;
    }
}

void Challenge::penalizeFall()
{
    update(FallPenalty);
}

} // namespace voxel
