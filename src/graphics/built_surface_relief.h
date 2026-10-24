#pragma once

#include "block_shapes.h"
#include "mesh.h"

namespace voxel {

// Manufactured surfaces use continuous courses instead of occasional rock
// chips. The caller clips the face to the exposed part of its block first.
bool hasBuiltSurfaceRelief(Block material);
void appendBuiltSurfaceRelief(MeshData& mesh, glm::ivec3 cell,
    const BlockSurface& surface, Block material, std::uint32_t seed);

} // namespace voxel
