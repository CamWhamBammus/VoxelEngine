#pragma once

#include "mesh.h"
#include <glm/mat4x4.hpp>

namespace voxel {

// Origin is the carry grip. The cage hangs below it, with its broad front
// facing +Z. Metal, amber panes and the leather glove are closed solid shapes.
MeshData buildHeldLantern();

// A small idle sway, in view space at the tools' 65-degree vertical FOV.
glm::mat4 heldLanternPose(float timeSeconds, float aspectRatio);

// View-space light source follows the centre of the glowing cage.
glm::vec3 heldLanternLightPosition(float timeSeconds, float aspectRatio);

} // namespace voxel
