#pragma once

#include "mesh.h"

namespace voxel {

// Shallow cosmetic surfaces on exposed blocks and every stair material:
// fitted paving, masonry, boards, roof tiles, stone fractures, and ore veins.
// Includes half-height treads, risers, and stair sides. Uses world-space
// vertices and the anchor's material ID/color. Outlines follow the same stone
// Voronoi regions and volumetric mineral field as materials.h. Ore deposits
// cross top/side corners and chunk boundaries without separate face seeds.
// The relief attribute projects each vertex onto its original painted face
// (render with decorative = 2).
// Geometry embeds 0.004 into its anchor and extends at most 0.07 into empty
// space. It never crosses its supporting surface's tangential edges.
// Faces beside stairs are clipped to the neighboring empty half-cube, so the
// same material continues into the opening without hidden/intersecting chips.
// At most one stone fracture strip (0.03 high) or three ore ridges, whose low
// tip and one lateral edge sink into the host. At block boundaries the relief
// tapers flush while the painted mineral continues around the corner.
// Each clipped outline has <=12 corners. Stone adds a midpoint and cap center,
// giving at most 39 triangles; the three ore ridges use at most 108 together.
// Crystal and amber have up to three broad facets (0.06 high), clipped from
// the same 3D crystal lattice that colors every face of their mineral body.
// About 1/5 of stone walls/ceilings and 1/12 of upward faces get a strip; an
// ore face without a painted vein has no relief. Stair faces always get a cut.
// Mossy stone and calcite use the same sparse fractures as ordinary stone.
// Manufactured surfaces use the same clipped faces, with recessed skirts and
// beveled caps up to 0.045 high. Rebuild after changes to this chunk or its
// face neighbors.
// Invalid chunk indices throw std::out_of_range, matching World::chunk().
MeshData buildBlockRelief(const World& world, std::size_t chunkIndex);

} // namespace voxel
