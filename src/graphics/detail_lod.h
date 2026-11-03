#pragma once

#include "mesh.h"
#include <array>

namespace voxel {

// Only shallow block relief and automatic cave dressing use these ranges.
// Terrain, stair shapes, authored assets, lights, and collision stay intact.
enum class DetailLodKind { Relief, Cave };
inline constexpr std::size_t DetailLodBandCount = 3;

struct DetailLodRange {
    std::size_t firstIndex = 0;
    std::size_t indexCount = 0;
};
struct DetailLodMesh {
    MeshData mesh;
    std::array<DetailLodRange, DetailLodBandCount> ranges;
};
struct DetailLodFade { float start, end; };

// Reorders indices into disjoint distance bands without copying vertices or
// changing any triangle. All relief on a host voxel gets the same band, so a
// bevel's cap and skirts disappear together. Cave pieces use their occupied
// cell, keeping each tuft, mineral pair, or ceiling taper together. The order
// within a band is stable and does not depend on the camera or world seed.
// Invalid indices, incomplete triangles, and nonfinite positions throw.
DetailLodMesh partitionDetailLod(MeshData mesh, DetailLodKind kind);

DetailLodFade detailLodFade(DetailLodKind kind, std::size_t band);
float detailLodCoverage(float distance, DetailLodFade fade);

// Conservative draw rejection uses the complete mesh/chunk bounds, including
// its cosmetic margin. A band is omitted only when every fragment has faded.
// There is no camera-side history or threshold oscillation between frames.
bool detailLodVisible(glm::vec3 camera, glm::vec3 minimum, glm::vec3 maximum,
                      DetailLodFade fade);

// Set both uniforms to zero for ordinary terrain and authored models. The
// rejection happens before expensive lighting, and uses world coordinates so
// its pattern neither swims with the camera nor changes with time. CPU range
// culling uses the same fade.end, removing already invisible triangles from
// draw submission rather than relying on fragment discard alone.
inline constexpr const char* detailLodShader = R"glsl(
uniform float detailFadeStart;
uniform float detailFadeEnd;
bool discardDistantDetail(vec3 position, vec3 eye) {
    if (detailFadeEnd <= 0.0) return false;
    float distanceToEye = length(position - eye);
    if (distanceToEye <= detailFadeStart) return false;
    if (distanceToEye >= detailFadeEnd) return true;
    float coverage = 1.0 - smoothstep(detailFadeStart, detailFadeEnd, distanceToEye);
    uvec3 cell = uvec3(ivec3(floor(position * 24.0)));
    uint hash = cell.x * 0x9e3779b9u ^ cell.y * 0x85ebca6bu ^ cell.z * 0xc2b2ae35u;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return float(hash & 0x00ffffffu) / 16777216.0 >= coverage;
}
)glsl";

} // namespace voxel
