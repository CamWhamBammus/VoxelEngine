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

// The arrival workshop's fixtures share one layout for collision, models,
// interaction, and route checks. Grounded origins sit on the finished floor.
struct HubBlacksmithLayout {
    glm::vec3 anvil, forge, workbench, toolRack, sign;
};
const HubBuilding& sceneBlacksmithBuilding();
HubBlacksmithLayout sceneBlacksmithLayout();
glm::vec3 sceneBlacksmithStation();
glm::vec3 sceneBlacksmithApproach();

struct HubArmoryLayout {
    glm::vec3 workbench, sign;
    std::array<glm::vec3, 3> stands;
};
const HubBuilding& sceneArmoryBuilding();
HubArmoryLayout sceneArmoryLayout();
glm::vec3 sceneArmoryStation();
glm::vec3 sceneArmoryApproach();

struct HubBridge { glm::ivec2 from, to; };
struct HubWaterfall {
    glm::ivec2 lip; // Top of the first fall; cascades flow along world X.
    int direction; // +1 east, -1 west.
    int top;
};
std::span<const HubBridge> sceneBridgePlan();
std::span<const HubWaterfall> sceneWaterfallPlan();
// Floor-voxel waypoints for the seven-wide north stair and the mine's east turn.
// Flights rise at most one block per tread; the level sections are landings.
std::span<const glm::ivec3> sceneExpeditionApproach();

// Replace the world with the enclosed tree-and-water hub. No building services
// or expedition transitions run here; this scene supplies their physical spaces.
void buildUndergroundScene(World& world);

} // namespace voxel
