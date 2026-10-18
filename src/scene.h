#pragma once

#include <array>
#include <span>
#include <glm/vec3.hpp>
#include "hub_buildings.h"

namespace voxel {
class World;

// Feet position on the arrival avenue, facing the tree island and north gate.
inline constexpr glm::vec3 VillageSpawn{2.5f, 19.0f, 77.5f};
inline constexpr float VillageYaw = -90.0f;
inline constexpr float VillagePitch = 4.0f;
inline constexpr glm::vec3 HeartTreeCenter{8.5f, 9.0f, 8.5f};
inline constexpr glm::vec3 ExpeditionDeparture{8.5f, 31.0f, -88.5f};
inline constexpr std::array<glm::vec3, 2> ExpeditionBeacons{{
    {-8.5f, 63.0f, -79.5f}, {25.5f, 63.0f, -79.5f}
}};
// The fixed architectural plan is shared by construction, tours, and route tests.
std::span<const HubBuilding> sceneBuildingPlan();

struct HubBridge { glm::ivec2 from, to; };
struct HubWaterfall {
    glm::ivec2 lip; // Top of the first fall; cascades flow along world X.
    int direction; // +1 east, -1 west.
    int top;
};
std::span<const HubBridge> sceneBridgePlan();
std::span<const HubWaterfall> sceneWaterfallPlan();

// Replace the world with the enclosed tree-and-water hub. No building services
// or expedition transitions run here; this scene supplies their physical spaces.
void buildUndergroundScene(World& world);

} // namespace voxel
