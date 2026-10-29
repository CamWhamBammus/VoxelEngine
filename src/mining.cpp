#include "mining.h"
#include "equipment.h"

#include <algorithm>
#include <cmath>

namespace voxel {

void MiningSession::cancelTarget()
{
    target_.reset();
    worked_ = progress_ = 0;
    blocked_ = false;
}

void MiningSession::reset()
{
    cancelTarget();
    swingRemaining_ = 0;
}

std::optional<MiningTarget> MiningSession::update(std::optional<MiningTarget> target,
                                                 bool held, int tier, float seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0) return {};
    seconds = std::min(seconds, .1f);
    swingRemaining_ = std::max(0.0f, swingRemaining_ - seconds);
    if (held && swingRemaining_ == 0) swingRemaining_ = SwingDuration;
    const auto ore = target ? oreInfo(target->block) : nullptr;
    if (!ore || !World::contains(target->position)) target.reset();
    if (target_ != target || !held) worked_ = progress_ = 0;
    target_ = target;
    blocked_ = ore && target_ && tier < ore->requiredTier;
    if (!held || !target_ || blocked_) return {};

    // Upgrades unlock tougher deposits and shorten familiar ones. Damage is
    // continuous while held; releasing or changing the target discards work.
    const float speed = 1.0f + .07f * (std::clamp(tier, 1, GearTierCount) - 1);
    worked_ += seconds * speed;
    progress_ = std::min(1.0f, worked_ / ore->mineSeconds);
    if (progress_ < 1.0f) return {};
    const auto finished = target_;
    cancelTarget();
    return finished;
}

} // namespace voxel
