#pragma once

#include <span>
#include <string_view>
#include <glm/vec3.hpp>

namespace voxel {
class World;

// Floor heights name the solid voxel; a standing player's feet are one above.
inline constexpr int HeartwoodFloor = 8;
inline constexpr int HeartwoodGalleryFloor = 21;
inline constexpr int HeartwoodUpperFloor = 34;
inline constexpr glm::ivec3 HeartwoodMinimum{-20, 8, -20};
inline constexpr glm::ivec3 HeartwoodMaximum{36, 76, 36};

enum class HeartwoodRoomKind { Library, ExpeditionLoft, Conservatory };

struct HeartwoodRoom {
    HeartwoodRoomKind kind;
    std::string_view name;
    glm::ivec3 minimum, maximum; // Clear interior, including its standing floor.
    glm::vec3 entrance;
};

struct HeartwoodLayout {
    glm::vec3 seed, station, approach;
    glm::vec3 cameraEye, cameraTarget;
};

const HeartwoodLayout& heartwoodLayout();
std::span<const HeartwoodRoom> heartwoodRooms();

// Hollow living-wood hall, its galleries, stairs, and future service rooms.
// Decorations use these same coordinates rather than a second floor plan.
void buildHeartwood(World& world);
} // namespace voxel
