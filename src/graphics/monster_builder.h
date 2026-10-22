#pragma once

#include "monsters.h"
#include "graphics/asset_builder.h"
#include <algorithm>
#include <cmath>

namespace voxel {

// Small helpers for authored parts and eased animation poses. Geometry stays
// local to a joint, so a sword follows its hand and a jaw follows its skull.
template<class Draw>
void monsterPart(MonsterRig& rig, std::string_view name, int parent, glm::vec3 pivot, Draw draw)
{
    MonsterPart part{name, parent, pivot, {}};
    const HubAsset origin{};
    AssetBuilder builder(part.mesh, origin);
    draw(builder);
    rig.parts.push_back(std::move(part));
}
inline float cycle(float seconds, float duration)
{
    return seconds / duration - std::floor(seconds / duration);
}
inline float ease(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float ramp(float t, float start, float end)
{
    return ease((t - start) / (end - start));
}
inline float pulse(float t, float start, float peak, float end)
{
    return t < peak ? ramp(t, start, peak) : 1.0f - ramp(t, peak, end);
}

} // namespace voxel
