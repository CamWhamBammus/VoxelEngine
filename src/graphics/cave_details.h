#pragma once

#include "mesh.h"

namespace voxel {

// Cosmetic, noncolliding details anchored to this chunk's solid blocks. Rebuild
// when the owning chunk or its neighbors change; the original voxels stay intact.
// Uses existing Block material IDs and world-space vertices. Details extend less
// than one cell into a neighboring air cell, with fewer than 700 triangles/chunk.
// Crystal tops form upright pairs; wall minerals, ceiling tapers, and flowers are sparse.
// Invalid chunk indices throw std::out_of_range, matching World::chunk().
MeshData buildCaveDetails(const World& world, std::size_t chunkIndex);

} // namespace voxel
