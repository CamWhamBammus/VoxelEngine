#include "health.h"
#include <algorithm>
#include <cmath>

namespace voxel {

float Health::respawnProgress() const
{
    return dead() ? std::clamp(deadSeconds_ / RespawnDelay,0.0f,1.0f) : 0;
}
bool Health::readyToRespawn() const { return dead() && deadSeconds_ >= RespawnDelay; }
bool Health::damage(int amount)
{
    if (amount <= 0 || dead()) return false;
    current_ = amount >= current_ ? 0 : current_ - amount;
    flash_ = .35f;
    if (dead()) deadSeconds_ = 0;
    return true;
}
void Health::update(float seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0) return;
    seconds = std::min(seconds,.1f);
    flash_ = std::max(0.0f,flash_-seconds);
    if (dead()) deadSeconds_ = std::min(RespawnDelay,deadSeconds_+seconds);
}
void Health::restore()
{
    current_ = Maximum;
    flash_ = deadSeconds_ = 0;
}
int fallDamage(float distance)
{
    if (!std::isfinite(distance) || distance <= 3.01f) return 0;
    return static_cast<int>(std::clamp(std::floor((distance-3.0f)*10.0f+.001f),0.0f,100.0f));
}

} // namespace voxel
