#pragma once

#include "voxel/raycast.h"
#include "voxel/world.h"

namespace voxel {
class EditHistory;

bool removeBlock(World& world, const RayHit& hit, EditHistory* history = nullptr);
bool placeBlock(World& world, const RayHit& hit, Block block, glm::vec3 cameraPosition,
                EditHistory* history = nullptr);

} // namespace voxel
