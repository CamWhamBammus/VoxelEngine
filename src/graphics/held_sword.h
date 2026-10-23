#pragma once

#include "mesh.h"
#include <glm/mat4x4.hpp>

namespace voxel {

// Authored in metres around the grip: blade points up +Y, broad face toward +Z.
MeshData buildHeldSword();

// View-space pose for a 65-degree vertical field of view. Remaining swing is
// normalized: 1 begins the windup, 0 is the resting pose.
glm::mat4 heldSwordPose(float swingRemaining, float aspectRatio);

} // namespace voxel
