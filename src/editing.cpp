#include "editing.h"
#include "block_shapes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace voxel {

namespace {

std::optional<RayHit> hitStair(glm::vec3 origin, const std::array<double, 3>& direction,
                              glm::ivec3 position, Block block, double entered, double exits,
                              glm::ivec3 entryNormal)
{
    std::optional<RayHit> closest;
    const auto shape = blockShape(block);
    for (int i = 0; i < shape.count; ++i) {
        const auto& box = shape.boxes[i];
        double near = -std::numeric_limits<double>::infinity();
        double far = std::numeric_limits<double>::infinity();
        glm::ivec3 normal(0);
        bool misses = false;
        for (int axis = 0; axis < 3; ++axis) {
            const double coordinate = static_cast<double>(origin[axis]) - position[axis];
            if (direction[axis] == 0.0) {
                // A ray along a box's open edge does not hit its solid volume.
                if (coordinate < box.minimum[axis] || coordinate >= box.maximum[axis]) misses = true;
                continue;
            }
            double first = (box.minimum[axis] - coordinate) / direction[axis];
            double last = (box.maximum[axis] - coordinate) / direction[axis];
            const int sign = direction[axis] > 0.0 ? -1 : 1;
            if (first > last) std::swap(first, last);
            if (first > near) {
                near = first;
                normal = glm::ivec3(0);
                normal[axis] = sign;
            }
            far = std::min(far, last);
        }
        const double distance = std::max(near, entered);
        if (misses || near >= far || far <= entered || distance > exits) continue;
        if (!closest || distance < closest->distance) {
            closest = RayHit{position, near < entered ? entryNormal : normal, static_cast<float>(distance)};
        }
    }
    return closest;
}

} // namespace

std::optional<RayHit> raycast(glm::vec3 origin, glm::vec3 direction, float maxDistance,
                              const std::function<Block(glm::ivec3)>& sample)
{
    if (!sample || !std::isfinite(maxDistance) || maxDistance < 0.0f) {
        return std::nullopt;
    }

    glm::ivec3 position(0);
    std::array<double, 3> unit{};
    double lengthSquared = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(origin[axis]) || !std::isfinite(direction[axis])) {
            return std::nullopt;
        }
        const double coordinate = std::floor(static_cast<double>(origin[axis]));
        if (coordinate < std::numeric_limits<int>::min()
            || coordinate > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        position[axis] = static_cast<int>(coordinate);
        unit[axis] = direction[axis];
        lengthSquared += unit[axis] * unit[axis];
    }
    if (lengthSquared == 0.0) {
        return std::nullopt;
    }
    const double length = std::sqrt(lengthSquared);
    std::array<int, 3> step{};
    std::array<double, 3> next{};
    std::array<double, 3> delta{};
    for (int axis = 0; axis < 3; ++axis) {
        unit[axis] /= length;
        if (unit[axis] == 0.0) {
            next[axis] = delta[axis] = std::numeric_limits<double>::infinity();
            continue;
        }
        step[axis] = unit[axis] > 0.0 ? 1 : -1;
        const double boundary = static_cast<double>(position[axis]) + (step[axis] > 0 ? 1.0 : 0.0);
        next[axis] = (boundary - static_cast<double>(origin[axis])) / unit[axis];
        delta[axis] = 1.0 / std::abs(unit[axis]);
    }

    // DDA visits the next voxel boundary instead of sampling tiny steps.
    // next[] gives each axis's next crossing distance; delta[] is its spacing.
    double entered = 0.0;
    glm::ivec3 entryNormal(0);
    while (true) {
        const double distance = std::min({next[0], next[1], next[2]});
        const auto block = sample(position);
        if (isStair(block)) {
            const auto hit = hitStair(origin, unit, position, block, entered,
                                     std::min(distance, static_cast<double>(maxDistance)), entryNormal);
            if (hit) return hit;
        } else if (isRenderable(block)) {
            return RayHit{position, entryNormal, static_cast<float>(entered)};
        }
        if (distance > static_cast<double>(maxDistance)) {
            return std::nullopt;
        }
        glm::ivec3 normal(0);
        bool hasNormal = false;
        for (int axis = 0; axis < 3; ++axis) {
            if (next[axis] != distance) {
                continue;
            }
            if ((step[axis] > 0 && position[axis] == std::numeric_limits<int>::max())
                || (step[axis] < 0 && position[axis] == std::numeric_limits<int>::min())) {
                return std::nullopt;
            }
            position[axis] += step[axis];
            next[axis] += delta[axis];
            if (!hasNormal) {
                normal[axis] = -step[axis];
                hasNormal = true;
            }
        }
        entered = distance;
        entryNormal = normal;
    }
}

EditHistory::EditHistory(std::size_t capacity) : capacity_(capacity)
{
}

bool EditHistory::apply(World& world, glm::ivec3 position, Block block)
{
    if (!isValidBlock(block)) {
        return false;
    }
    const Block before = world.get(position);
    if (!world.set(position, block)) {
        return false;
    }

    redo_.clear();
    if (capacity_ > 0) {
        undo_.push_back({position, before, block});
        if (undo_.size() > capacity_) {
            undo_.pop_front();
        }
    }
    return true;
}

bool EditHistory::undo(World& world)
{
    if (undo_.empty()) {
        return false;
    }
    const Edit edit = undo_.back();
    if (world.get(edit.position) != edit.after
        || !world.set(edit.position, edit.before)) {
        clear();
        return false;
    }
    redo_.push_back(edit);
    undo_.pop_back();
    return true;
}

bool EditHistory::redo(World& world)
{
    if (redo_.empty()) {
        return false;
    }
    const Edit edit = redo_.back();
    if (world.get(edit.position) != edit.before
        || !world.set(edit.position, edit.after)) {
        clear();
        return false;
    }
    undo_.push_back(edit);
    redo_.pop_back();
    return true;
}

void EditHistory::clear()
{
    undo_.clear();
    redo_.clear();
}

bool removeBlock(World& world, const RayHit& hit, EditHistory* history)
{
    if (!isRenderable(world.get(hit.position))) {
        return false;
    }
    if (history) {
        return history->apply(world, hit.position, Block::Air);
    }
    return world.set(hit.position, Block::Air);
}

PlacementCheck checkPlacement(const World& world, const RayHit& hit, Block block,
                               glm::vec3 cameraPosition)
{
    if (!isRenderable(block)) {
        return {{}, PlacementFailure::InvalidMaterial};
    }
    if (!isRenderable(world.get(hit.position))) {
        return {{}, PlacementFailure::MissingTarget};
    }
    int normalAxes = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (hit.normal[axis] < -1 || hit.normal[axis] > 1) {
            return {{}, PlacementFailure::InvalidFace};
        }
        normalAxes += hit.normal[axis] != 0 ? 1 : 0;
    }
    if (normalAxes != 1) {
        return {{}, PlacementFailure::InvalidFace};
    }

    glm::ivec3 destination(0);
    bool overlapsCamera = true;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(cameraPosition[axis])) {
            return {{}, PlacementFailure::InvalidCamera};
        }
        const auto coordinate = static_cast<std::int64_t>(hit.position[axis]) + hit.normal[axis];
        if (coordinate < std::numeric_limits<int>::min()
            || coordinate > std::numeric_limits<int>::max()) {
            return {{}, PlacementFailure::OutsideWorld};
        }
        destination[axis] = static_cast<int>(coordinate);
        overlapsCamera &= std::floor(static_cast<double>(cameraPosition[axis])) == coordinate;
    }
    if (!World::contains(destination)) {
        return {destination, PlacementFailure::OutsideWorld};
    }
    if (isRenderable(world.get(destination))) {
        return {destination, PlacementFailure::Occupied};
    }
    if (overlapsCamera && isSolid(block)) {
        return {destination, PlacementFailure::CameraOverlap};
    }
    return {destination, PlacementFailure::None};
}

bool placeBlock(World& world, const RayHit& hit, Block block, glm::vec3 cameraPosition, EditHistory* history)
{
    const auto placement = checkPlacement(world, hit, block, cameraPosition);
    if (placement.failure != PlacementFailure::None) {
        return false;
    }
    if (history) {
        return history->apply(world, placement.position, block);
    }
    return world.set(placement.position, block);
}

} // namespace voxel
