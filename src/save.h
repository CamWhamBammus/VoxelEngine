#pragma once

#include "world.h"

#include <filesystem>
#include <iosfwd>

namespace voxel {

// Versioned, little-endian snapshot. Throws std::runtime_error for invalid
// snapshots or failed streams. Decoding reads only the fixed format size plus
// one byte to detect trailing data, and returns an entirely new world.
void encodeWorld(const World& world, std::ostream& output);
World decodeWorld(std::istream& input);

// Writes a unique temporary file beside the destination, then atomically
// replaces it on macOS/Linux. Throws on failure without deleting the old save.
void saveWorld(const World& world, const std::filesystem::path& path);
World loadWorld(const std::filesystem::path& path);

} // namespace voxel
