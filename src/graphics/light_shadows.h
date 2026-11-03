#pragma once

#include "graphics/light_grid.h"

namespace voxel {
class World;

// One byte per input source: 1 means every possible shadow ray stays in air,
// water or cutout foliage; 0 means use the usual voxel shadow trace. This is a
// conservative proof, not a visibility approximation. Recompute on world edits
// or source changes, alongside the source texture; it never depends on camera.
std::vector<std::uint8_t> unoccludedLightSources(const World& world,
                                              std::span<const PointLight> lights);

} // namespace voxel
