#pragma once

#include "mesh.h"

namespace voxel {

// Shallow cosmetic plates on exposed stone and ore faces, with world-space
// vertices and the anchor's material ID/color. Outlines follow the same stone
// Voronoi regions and ore flecks as materials.h. The relief attribute projects
// each vertex onto its original painted face (render with decorative = 2).
// Geometry embeds 0.004 into its anchor and extends at most 0.12 into one
// adjacent air cell. It never crosses the face's tangential block edges.
// At most two stone plates or three ore flecks, each a <=12-corner polygon:
// 3N-2 triangles gives limits of 68/102 per face. Stone faces are sparse; an
// ore face without a painted fleck has no relief. Rebuild after changes to
// this chunk or its face neighbors.
// Invalid chunk indices throw std::out_of_range, matching World::chunk().
MeshData buildBlockRelief(const World& world, std::size_t chunkIndex);

} // namespace voxel
