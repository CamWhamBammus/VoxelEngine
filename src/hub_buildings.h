#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace voxel {

class World;

enum class HubBuildingStyle { Cottage, Hall, Workshop, Storehouse, Inn, Tower, Greenhouse };

struct HubBuilding {
    // Minimum world X/Z corner of the rotated footprint; Y is the floor voxel.
    glm::ivec3 origin;
    int width = 7;
    int depth = 7;
    HubBuildingStyle style = HubBuildingStyle::Cottage;
    // Local front is +Z. Facing 0/1/2/3 turns it toward +Z/+X/-Z/-X.
    int facing = 0;
    int variant = 0;
};

// Odd local widths/depths from 7 through 25 are supported. Buildings extend at
// most one voxel beyond the footprint; their highest voxel is at most 32 above
// the floor. Ground-level entrances stay open; upper windows are facade details.
// Invalid dimensions/facing/style or bounds throw before modifying the world.
void buildHubBuilding(World& world, const HubBuilding& building);

// Rotated floor dimensions, and the center lower cell of the open front door.
glm::ivec2 hubBuildingFootprint(const HubBuilding& building);
glm::ivec3 hubBuildingDoor(const HubBuilding& building);
// Local Y of the main wall's top and the highest constructed voxel.
int hubBuildingWallHeight(const HubBuilding& building);
int hubBuildingHeight(const HubBuilding& building);

} // namespace voxel
