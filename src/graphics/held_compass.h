#pragma once

#include "mesh.h"
#include <glm/mat4x4.hpp>

namespace voxel {

struct HeldCompassState {
    float bearingRadians = 0;
    float timeSeconds = 0;
};

// The dial lies in XY and faces +Z. The needle pivots around the origin;
// its red tip points +Y before its independent bearing rotation is applied.
MeshData buildHeldCompass();
MeshData buildHeldCompassNeedle();
glm::mat4 heldCompassPose(float timeSeconds, float aspectRatio);
glm::mat4 heldCompassNeedlePose(float bearingRadians);

// Horizontal bearing relative to the camera, clockwise toward its right.
// A target directly above/below the player has no horizontal bearing.
float compassBearing(glm::vec3 position, glm::vec3 forward, glm::vec3 target);

} // namespace voxel
