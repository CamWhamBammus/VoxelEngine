#pragma once

#include "items.h"
#include "world.h"

#include <span>

namespace voxel {

struct OreInfo {
    Block block;
    ItemId item;
    int requiredTier;
    float mineSeconds;
};

// Each resource has one source block. Existing crystal blocks keep their saved IDs.
std::span<const OreInfo> oreCatalog();
const OreInfo* oreInfo(Block block);

} // namespace voxel
