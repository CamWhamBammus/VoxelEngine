#include "player.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace voxel {
namespace {

constexpr double ContactTolerance = 0.00001;

struct Bounds {
    glm::dvec3 minimum;
    glm::dvec3 maximum;
};

Bounds bodyBounds(glm::vec3 feet)
{
    const double halfWidth = Player::Width / 2.0;
    return {glm::dvec3(feet) + glm::dvec3(-halfWidth, 0, -halfWidth),
            glm::dvec3(feet) + glm::dvec3(halfWidth, Player::Height, halfWidth)};
}

bool finite(glm::vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool overlapOnAxis(const Bounds& first, const Bounds& second, int axis)
{
    return first.minimum[axis] < second.maximum[axis] - ContactTolerance
        && first.maximum[axis] > second.minimum[axis] + ContactTolerance;
}

bool overlaps(const Bounds& first, const Bounds& second)
{
    return overlapOnAxis(first, second, 0) && overlapOnAxis(first, second, 1)
        && overlapOnAxis(first, second, 2);
}

template<typename Visit>
void visitSolidBlocks(const World& world, const Bounds& bounds, Visit visit)
{
    glm::ivec3 first, last;
    for (int axis = 0; axis < 3; ++axis) {
        if (bounds.maximum[axis] <= World::Min[axis]
            || bounds.minimum[axis] >= World::MaxExclusive[axis]) return;
        // Clip before converting to integers: even a very long sweep visits
        // only the finite world, never arbitrary coordinates outside it.
        first[axis] = static_cast<int>(std::floor(std::max(bounds.minimum[axis],
                                                         double(World::Min[axis]))));
        last[axis] = static_cast<int>(std::ceil(std::min(bounds.maximum[axis],
                                                       double(World::MaxExclusive[axis])))) - 1;
    }
    for (int y = first.y; y <= last.y; ++y) {
        for (int z = first.z; z <= last.z; ++z) {
            for (int x = first.x; x <= last.x; ++x) {
                if (isSolid(world.get({x, y, z}))) {
                    visit(Bounds{glm::dvec3(x, y, z), glm::dvec3(x + 1, y + 1, z + 1)});
                }
            }
        }
    }
}

bool collides(const World& world, glm::vec3 feet)
{
    const Bounds body = bodyBounds(feet);
    bool collision = false;
    visitSolidBlocks(world, body, [&](const Bounds& block) {
        collision |= overlaps(body, block);
    });
    return collision;
}

bool supported(const World& world, glm::vec3 feet)
{
    return collides(world, feet - glm::vec3(0, 0.001f, 0));
}

} // namespace

bool Player::overlapsBlock(glm::ivec3 position) const
{
    const glm::dvec3 minimum(position);
    return overlaps(bodyBounds(feet_), {minimum, minimum + glm::dvec3(1)});
}

bool Player::setFeetPosition(const World& world, glm::vec3 position)
{
    if (!finite(position)) return false;
    const Bounds body = bodyBounds(position);
    for (const int axis : {0, 2}) {
        if (body.minimum[axis] < World::Min[axis] - ContactTolerance
            || body.maximum[axis] > World::MaxExclusive[axis] + ContactTolerance) return false;
    }
    if (collides(world, position)) return false;
    feet_ = position;
    grounded_ = supported(world, feet_);
    return true;
}

void Player::move(const World& world, glm::vec3 displacement)
{
    if (!finite(displacement)) return;
    moveAxis(world, 0, displacement.x);
    moveAxis(world, 2, displacement.z);
    moveAxis(world, 1, displacement.y);
    grounded_ = supported(world, feet_);
}

void Player::moveAxis(const World& world, int axis, float distance)
{
    if (distance == 0.0f) return;
    const Bounds body = bodyBounds(feet_);
    double allowed = distance;
    if (axis != 1) {
        allowed = std::clamp(allowed, World::Min[axis] - body.minimum[axis],
                            World::MaxExclusive[axis] - body.maximum[axis]);
    }
    Bounds sweep = body;
    sweep.minimum[axis] += std::min(allowed, 0.0);
    sweep.maximum[axis] += std::max(allowed, 0.0);
    visitSolidBlocks(world, sweep, [&](const Bounds& block) {
        for (int other = 0; other < 3; ++other) {
            if (other != axis && !overlapOnAxis(body, block, other)) return;
        }
        if (overlaps(body, block)) {
            allowed = 0.0;
        } else if (distance > 0 && block.minimum[axis] >= body.maximum[axis] - ContactTolerance) {
            allowed = std::min(allowed, std::max(0.0, block.minimum[axis] - body.maximum[axis]));
        } else if (distance < 0 && block.maximum[axis] <= body.minimum[axis] + ContactTolerance) {
            allowed = std::max(allowed, std::min(0.0, block.maximum[axis] - body.minimum[axis]));
        }
    });
    const double next = feet_[axis] + allowed;
    if (std::abs(next) <= std::numeric_limits<float>::max()) feet_[axis] = static_cast<float>(next);
}

} // namespace voxel
