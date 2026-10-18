#pragma once

#include "world.h"

#include <filesystem>
#include <iosfwd>

namespace voxel {

// Versioned, little-endian snapshot. Throws std::runtime_error for invalid
// snapshots or failed streams. Version two stores the expanded dimensions;
// version-one 5x1x5 snapshots keep their original coordinates when imported.
// Decoding reads only the validated format size plus one trailing-data byte
// and returns an entirely new world. New legacy-import areas use seed terrain.
void encodeWorld(const World& world, std::ostream& output);
World decodeWorld(std::istream& input);

// Writes a unique temporary file beside the destination, then atomically
// replaces it on macOS/Linux. Throws on failure without deleting the old save.
void saveWorld(const World& world, const std::filesystem::path& path);
World loadWorld(const std::filesystem::path& path);

} // namespace voxel
