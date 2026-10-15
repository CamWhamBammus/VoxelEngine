#include "voxel/editor.h"
#include "voxel/edit_history.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace voxel {

bool removeBlock(World& world, const RayHit& hit, EditHistory* history)
{
    return isSolid(world.get(hit.position)) && (history
        ? history->apply(world, hit.position, Block::Air) : world.set(hit.position, Block::Air));
}

bool placeBlock(World& world, const RayHit& hit, Block block, glm::vec3 cameraPosition, EditHistory* history)
{
    if (!isSolid(block) || !isSolid(world.get(hit.position))) {
        return false;
    }
    glm::ivec3 destination(0);
    int normalAxes = 0;
    bool overlapsCamera = true;
    for (int axis = 0; axis < 3; ++axis) {
        if (hit.normal[axis] < -1 || hit.normal[axis] > 1
            || !std::isfinite(cameraPosition[axis])) {
            return false;
        }
        normalAxes += hit.normal[axis] != 0 ? 1 : 0;
        const auto coordinate = static_cast<std::int64_t>(hit.position[axis]) + hit.normal[axis];
        if (coordinate < std::numeric_limits<int>::min()
            || coordinate > std::numeric_limits<int>::max()) {
            return false;
        }
        destination[axis] = static_cast<int>(coordinate);
        overlapsCamera &= std::floor(static_cast<double>(cameraPosition[axis])) == coordinate;
    }
    if (normalAxes != 1 || overlapsCamera || isSolid(world.get(destination))) {
        return false;
    }
    return history ? history->apply(world, destination, block) : world.set(destination, block);
}

} // namespace voxel
