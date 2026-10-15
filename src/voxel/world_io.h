#pragma once

#include "voxel/world.h"

#include <filesystem>

namespace voxel {

// Writes a unique temporary file beside the destination, then atomically
// replaces it on macOS/Linux. Throws on failure without deleting the old save.
void saveWorld(const World& world, const std::filesystem::path& path);
World loadWorld(const std::filesystem::path& path);

} // namespace voxel
