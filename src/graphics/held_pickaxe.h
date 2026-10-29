#pragma once

#include "mesh.h"
#include <glm/mat4x4.hpp>

namespace voxel {

// Local origin is the grip. The handle points up +Y and the curved steel head
// spans X, with broad bevels facing +Z and -Z.
MeshData buildHeldPickaxe(int tier = 1);

// View-space pose for a 65-degree vertical FOV. Remaining swing is normalized:
// 1 starts the windup and 0 rests the tool at the player's right hand.
glm::mat4 heldPickaxePose(float swingRemaining, float aspectRatio);

} // namespace voxel
