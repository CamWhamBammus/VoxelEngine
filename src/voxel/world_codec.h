#pragma once

#include "voxel/world.h"

#include <iosfwd>

namespace voxel {

// Versioned, little-endian snapshot. Throws std::runtime_error for invalid
// snapshots or failed streams. Decoding reads only the fixed format size plus
// one byte to detect trailing data, and returns an entirely new world.
void encodeWorld(const World& world, std::ostream& output);
World decodeWorld(std::istream& input);

} // namespace voxel
