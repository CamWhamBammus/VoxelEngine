#pragma once

#include "hub_assets.h"
#include <span>
#include <vector>

namespace voxel {

// New garden layers follow real terrain and avoid the existing village kit.
std::vector<HubAsset> planLushAssets(const World& world, std::span<const HubAsset> existing);

// Tall new silhouettes replace overlapping small plants, while preserving the
// village architecture, paths, rocks, roots, and the remaining garden layer.
void blendLushAssets(const World& world, std::vector<HubAsset>& assets);

// Paint natural surfaces without adding/removing collision or changing routes.
void dressLushTerrain(World& world);

} // namespace voxel
