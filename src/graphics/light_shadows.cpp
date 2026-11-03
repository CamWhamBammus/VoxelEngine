#include "graphics/light_shadows.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace voxel {
namespace {

bool clearInfluence(const World& world, const PointLight& light)
{
    // The production DDA guarantees at most 32 cell crossings for the current
    // fifteen-metre sources. Larger or malformed sources keep its normal path.
    if (!std::isfinite(light.radius) || light.radius <= 0 || light.radius > 15) return false;
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(light.position[axis]) || !std::isfinite(light.color[axis]) || light.color[axis] < 0)
            return false;

    // A convex sphere contains every segment between a receiver (offset .03m
    // from its face) and the soft-light endpoints (less than .26m from source).
    // Include a little extra room for float rounding at voxel boundaries.
    const double radius = double(light.radius) + .35;
    glm::ivec3 first{}, last{};
    for (int axis = 0; axis < 3; ++axis) {
        const double low = double(light.position[axis]) - radius;
        const double high = double(light.position[axis]) + radius;
        // Shader occupancy outside the finite world is air. Clamp before any
        // integer conversion, including for large finite source coordinates.
        if (high < World::Min[axis] || low > World::MaxExclusive[axis]) return true;
        first[axis] = int(std::clamp(std::floor(low), double(World::Min[axis]),
                                     double(World::MaxExclusive[axis] - 1)));
        last[axis] = int(std::clamp(std::floor(high), double(World::Min[axis]),
                                    double(World::MaxExclusive[axis] - 1)));
        // A lower sphere face exactly on an integer boundary also touches the
        // preceding voxel. Keep it, rather than relying on traversal ties.
        first[axis] = std::max(World::Min[axis],first[axis] - 1);
    }
    const double radiusSquared = radius * radius;
    for (int z = first.z; z <= last.z; ++z)
        for (int y = first.y; y <= last.y; ++y)
            for (int x = first.x; x <= last.x; ++x) {
                const glm::ivec3 cell{x,y,z};
                const auto block = world.get(cell);
                if (!isOpaque(block) && !isStair(block)) continue;
                double distanceSquared = 0;
                for (int axis = 0; axis < 3; ++axis) {
                    const double nearest = std::clamp(double(light.position[axis]),
                                                      double(cell[axis]),double(cell[axis]) + 1);
                    const double distance = light.position[axis] - nearest;
                    distanceSquared += distance * distance;
                }
                // Stair boxes are deliberately conservative: even their empty
                // upper half keeps tracing, avoiding orientation assumptions.
                if (distanceSquared <= radiusSquared) return false;
            }
    return true;
}

} // namespace

std::vector<std::uint8_t> unoccludedLightSources(const World& world,
                                              std::span<const PointLight> lights)
{
    std::vector<std::uint8_t> result;
    result.reserve(lights.size());
    for (const auto& light : lights) result.push_back(clearInfluence(world,light) ? 1 : 0);
    return result;
}

} // namespace voxel
