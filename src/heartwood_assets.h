#pragma once

#include "hub_assets.h"

namespace voxel {

// Hand-placed library furniture, supported lamps, and balcony details share
// the hall's architectural plan. Edited worlds only keep supported fixtures.
std::vector<HubAsset> planHeartwoodAssets(const World& world);

// The enlarged hall replaces the old island's scattered roots and shrubs.
// This runs after landscape dressing so no legacy prop crosses an interior route.
void blendHeartwoodAssets(const World& world, std::vector<HubAsset>& assets);

} // namespace voxel
