#pragma once

#include <array>
#include <glm/vec3.hpp>

namespace voxel {
class World;

// Feet position and view across the village square. The cavern floor is Y=3.
inline constexpr glm::vec3 VillageSpawn{-1.5f, 4.0f, 13.5f};
inline constexpr float VillageYaw = -90.0f;
inline constexpr float VillagePitch = -8.0f;
inline constexpr std::array<glm::vec3, 3> OreChambers{{
    {-21.5f, 4.0f, -14.5f},
    {28.5f, 4.0f, -12.5f},
    {27.5f, 4.0f, 28.5f},
}};

// Replace the complete world with a seeded, enclosed village and mine sample.
void buildUndergroundScene(World& world);

} // namespace voxel
