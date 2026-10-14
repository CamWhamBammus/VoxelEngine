#pragma once

#include "voxel/raycast.h"
#include "voxel/world.h"

namespace voxel {

bool removeBlock(World& world, const RayHit& hit);
bool placeBlock(World& world, const RayHit& hit, Block block, glm::vec3 cameraPosition);

} // namespace voxel
