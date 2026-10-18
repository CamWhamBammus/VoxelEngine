#pragma once

#include <span>
#include <string_view>
#include <vector>
#include <glm/vec3.hpp>

namespace voxel {
class World;

inline constexpr int RootCrossingCount = 10;

struct RootCrossingInfo {
    std::string_view name, description, difficulty;
};

struct RootCrossingCourse {
    glm::vec3 spawn, finish, checkpoint;
    float resetY;
    float spawnYaw = -90.0f;
    // Feet positions along the intended route, including its safe landings.
    std::vector<glm::vec3> route;
};

std::span<const RootCrossingInfo> rootCrossingCatalog();
// Replaces the scene, preserving its seed. Invalid indices never mutate it.
RootCrossingCourse buildRootCrossing(World& world, int index);

} // namespace voxel
