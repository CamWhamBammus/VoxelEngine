#pragma once

#include "mesh.h"

namespace voxel {

// Shallow cosmetic fractures on exposed stone and ore faces, including the
// half-height treads, risers, and sides of stone/limestone/slate stairs. Uses world-space
// vertices and the anchor's material ID/color. Outlines follow the same stone
// Voronoi regions and ore flecks as materials.h. The relief attribute projects
// each vertex onto its original painted face (render with decorative = 2).
// Geometry embeds 0.004 into its anchor and extends at most 0.05 into empty
// space. It never crosses its supporting surface's tangential edges.
// At most one stone fracture strip (0.03 high) or two ore ridges, whose low
// tip and one lateral edge sink into the host instead of lifting a full kite.
// Each clipped outline has <=12 corners. Stone adds a midpoint and cap center,
// giving at most 39 triangles; the two ore wedges use at most 68 together.
// About 1/5 of stone walls/ceilings and 1/12 of upward faces get a strip; an
// ore face without a painted vein has no relief. Rebuild after changes to
// this chunk or its face neighbors.
// Invalid chunk indices throw std::out_of_range, matching World::chunk().
MeshData buildBlockRelief(const World& world, std::size_t chunkIndex);

} // namespace voxel
