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
    verticalVelocity_ = 0.0f;
    jumpHeld_ = false;
    return true;
}

void Player::move(const World& world, glm::vec3 displacement)
{
    if (!finite(displacement)) return;
    bool canStep = displacement.y <= 0.0f && supported(world, feet_);
    for (const int axis : {0, 2}) {
        const float before = feet_[axis];
        moveAxis(world, axis, displacement[axis]);
        const float remaining = displacement[axis] - (feet_[axis] - before);
        if (!canStep || std::abs(remaining) <= ContactTolerance || !supported(world, feet_)) continue;

        // Try a low stair only after ordinary movement meets an obstacle.
        // All three legs are swept, including headroom above the contact point.
        Player raised = *this;
        raised.moveAxis(world, 1, 1.0f);
        if (raised.feet_.y - feet_.y < 1.0f - ContactTolerance) continue;
        raised.moveAxis(world, axis, remaining);
        const float progress = (raised.feet_[axis] - feet_[axis]) * (remaining > 0.0f ? 1.0f : -1.0f);
        if (progress <= ContactTolerance) continue;
        raised.moveAxis(world, 1, -1.0f);
        const float rise = raised.feet_.y - feet_.y;
        if (rise <= ContactTolerance || rise > 1.0f + ContactTolerance || !supported(world, raised.feet_)) continue;
        feet_ = raised.feet_;
        canStep = false; // A diagonal move must not climb two steps at once.
    }
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

void Player::update(const World& world, glm::vec2 movement, glm::vec3 forward,
                    bool jump, bool sprint, float deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f
        || !std::isfinite(movement.x) || !std::isfinite(movement.y) || !finite(forward)) return;

    const double forwardLength = std::hypot(double(forward.x), double(forward.z));
    const glm::vec3 facing = forwardLength > 0.0
        ? glm::vec3(forward.x / forwardLength, 0, forward.z / forwardLength)
        : glm::vec3(0, 0, -1);
    const glm::vec3 right{-facing.z, 0, facing.x};
    const double inputLength = std::hypot(double(movement.x), double(movement.y));
    if (inputLength > 1.0) movement = glm::vec2(movement.x / inputLength, movement.y / inputLength);
    const float speed = sprint ? 7.0f : 4.5f;
    const glm::vec3 horizontalVelocity = (right * movement.x + facing * movement.y) * speed;

    // Refresh support before jumping, including when the floor was just mined.
    grounded_ = supported(world, feet_);
    if (jump && !jumpHeld_ && grounded_) verticalVelocity_ = 8.0f;
    jumpHeld_ = jump;

    // Small physics steps keep jumps and wall sliding consistent during a slow frame.
    constexpr float gravity = 24.0f;
    float remaining = std::min(deltaSeconds, 0.1f);
    while (remaining > 0.0f) {
        const float step = std::min(remaining, 1.0f / 120.0f);
        const float verticalDistance = verticalVelocity_ * step - 0.5f * gravity * step * step;
        const float previousY = feet_.y;
        move(world, horizontalVelocity * step + glm::vec3(0, verticalDistance, 0));
        verticalVelocity_ -= gravity * step;
        if (std::abs((feet_.y - previousY) - verticalDistance) > ContactTolerance
            || (grounded_ && verticalVelocity_ < 0.0f)) {
            verticalVelocity_ = 0.0f;
        }
        remaining -= step;
    }
}

bool Player::spawn(const World& world, glm::ivec2 preferred)
{
    preferred.x = std::clamp(preferred.x, World::Min.x, World::MaxExclusive.x - 1);
    preferred.y = std::clamp(preferred.y, World::Min.z, World::MaxExclusive.z - 1);
    bool found = false;
    glm::vec3 destination{};
    int nearestSquared = std::numeric_limits<int>::max();
    for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
        for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
            const int dx = x - preferred.x;
            const int dz = z - preferred.y;
            const int distanceSquared = dx * dx + dz * dz;
            if (distanceSquared >= nearestSquared) continue;

            for (int y = World::MaxExclusive.y - 1; y >= World::Min.y; --y) {
                if (!isSolid(world.get({x, y, z}))) continue;
                const glm::vec3 candidate{x + 0.5f, y + 1.0f, z + 0.5f};
                // Spawn above the highest solid, with room for the entire body.
                if (!collides(world, candidate)) {
                    destination = candidate;
                    nearestSquared = distanceSquared;
                    found = true;
                }
                break;
            }
        }
    }
    return found && setFeetPosition(world, destination);
}

} // namespace voxel
