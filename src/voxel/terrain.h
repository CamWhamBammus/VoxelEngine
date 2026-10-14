#pragma once

#include "voxel/chunk.h"

#include <cstdint>

namespace voxel {

// Highest solid voxel's world Y coordinate, in the inclusive range [4, 13].
int terrainHeight(int worldX, int worldZ, std::uint32_t seed);

// Replaces every voxel, using world coordinates so adjacent chunks agree.
void generateTerrain(Chunk& chunk, glm::ivec3 origin, std::uint32_t seed);

} // namespace voxel
