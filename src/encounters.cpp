#include "encounters.h"
#include "block_shapes.h"
#include "editing.h"
#include "spawner_chains.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <glm/geometric.hpp>

namespace voxel {
namespace {

constexpr float Pi = 3.14159265359f;
constexpr float SpawnInterval = .75f;
constexpr float PlayerHitCooldown = .65f;
constexpr float HitRecoilDuration = .26f;
constexpr float HitKnockbackDistance = .62f;
constexpr std::size_t NoAsset = std::numeric_limits<std::size_t>::max();

bool finite(glm::vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float obstruction(const World& world, const AssetCollision* assets, glm::vec3 origin,
                  glm::vec3 direction, float distance, std::size_t ignore = NoAsset)
{
    float nearest = distance;
    const auto block = raycast(origin, direction, distance, [&](glm::ivec3 cell) {
        const auto material = world.get(cell);
        return isSolid(material) ? material : Block::Air;
    });
    if (block) nearest = block->distance;
    if (!assets) return nearest;
    // Activation rays end inside the spawner's own model. Skip only that
    // placement, continuing after its surfaces to still catch other scenery.
    float traveled = 0;
    for (int surface = 0; surface < 12 && traveled < nearest; ++surface) {
        const auto hit = assets->raycast(origin + direction * traveled, direction, nearest - traveled);
        if (!hit) break;
        if (hit->assetIndex != ignore) return std::min(nearest, traveled + hit->distance);
        traveled += hit->distance + .015f;
    }
    return nearest;
}

bool visible(const World& world, const AssetCollision* assets, glm::vec3 from,
             glm::vec3 to, std::size_t ignore = NoAsset)
{
    const auto offset = to - from;
    const float distance = glm::length(offset);
    return distance < .001f || obstruction(world, assets, from, offset / distance, distance, ignore)
        >= distance - .035f;
}

// A segment/AABB test is used for sword targeting and projectile sweeps. It
// catches fast crossings without requiring a projectile to land inside a body.
float bodyHit(glm::vec3 from, glm::vec3 direction, glm::vec3 minimum,
              glm::vec3 maximum, float reach)
{
    float near = 0, far = reach;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < .00001f) {
            if (from[axis] < minimum[axis] || from[axis] > maximum[axis]) return -1;
        } else {
            float first = (minimum[axis] - from[axis]) / direction[axis];
            float last = (maximum[axis] - from[axis]) / direction[axis];
            if (first > last) std::swap(first, last);
            near = std::max(near, first);
            far = std::min(far, last);
            if (near > far) return -1;
        }
    }
    return near;
}

int maximumHealth(MonsterKind kind)
{
    return kind == MonsterKind::Skeleton ? 40 : kind == MonsterKind::Wizard ? 50 : 35;
}

float height(MonsterKind kind)
{
    return kind == MonsterKind::Slime ? 1.1f : kind == MonsterKind::Wizard ? 2.5f : 2.1f;
}

void animation(MonsterInstance& model, MonsterAnimation next)
{
    if (model.animation == next) return;
    model.transitionFrom = blendedMonsterPose(model);
    model.animation = next;
    model.seconds = 0;
    model.transition = 0;
}

glm::vec3 horizontal(glm::vec3 vector)
{
    vector.y = 0;
    const float length = glm::length(vector);
    return length > .001f ? vector / length : glm::vec3(0, 0, 1);
}

glm::vec3 hitDirection(glm::vec3 vector)
{
    vector.y = 0;
    const float length = glm::length(vector);
    return std::isfinite(length) && length > .001f ? vector / length : glm::vec3(0);
}

bool slimeFits(const World& world, const AssetCollision* assets, glm::vec3 feet)
{
    // Player supplies gravity, step handling and swept movement; the slime's
    // broad silhouette also needs clearance outside that narrower body.
    const auto minimum = feet + glm::vec3(-.69f, .002f, -.69f);
    const auto maximum = feet + glm::vec3(.69f, 1.08f, .69f);
    const auto first = glm::ivec3(glm::floor(minimum + glm::vec3(.0001f)));
    const auto last = glm::ivec3(glm::ceil(maximum - glm::vec3(.0001f))) - glm::ivec3(1);
    if (!World::contains(first) || !World::contains(last)) return false;
    for (int y = first.y; y <= last.y; ++y) for (int z = first.z; z <= last.z; ++z)
        for (int x = first.x; x <= last.x; ++x) {
            const auto shape = blockShape(world.get({x, y, z}));
            for (int box = 0; box < shape.count; ++box) {
                const auto low = glm::vec3(x, y, z) + shape.boxes[box].minimum;
                const auto high = glm::vec3(x, y, z) + shape.boxes[box].maximum;
                if (glm::all(glm::lessThan(minimum, high - glm::vec3(.0001f)))
                    && glm::all(glm::greaterThan(maximum, low + glm::vec3(.0001f)))) return false;
            }
        }
    return !assets || !assets->overlaps(minimum, maximum);
}

} // namespace

void EncounterSession::begin(std::span<const HubAsset> assets,
                             std::shared_ptr<const AssetCollision> collision,
                             std::span<const std::size_t> cleared)
{
    clear();
    collision_ = std::move(collision);
    for (std::size_t index = 0; index < assets.size(); ++index) {
        const auto& asset = assets[index];
        if (asset.kind != HubAssetKind::TrialSpawner || !finite(asset.position)) continue;
        std::array<int, 3> counts{};
        for (std::size_t i = 0; i < counts.size(); ++i)
            counts[i] = std::clamp(int(asset.spawner.counts[i]), 0, SpawnerMobLimit);
        Spawner spawner{index, asset.position, counts, counts};
        spawner.cleared = std::find(cleared.begin(), cleared.end(), index) != cleared.end();
        spawners_.push_back(spawner);
    }
    // Serialized IDs are independent of placement order. Invalid external
    // data falls back to standalone encounters rather than trapping an exit.
    if (validateSpawnerChains(assets).empty()) {
        for (std::size_t index = 0; index < spawners_.size(); ++index) {
            const auto nextAsset = spawnerById(assets, assets[spawners_[index].assetIndex].spawner.next);
            if (!nextAsset) continue;
            const auto next = std::find_if(spawners_.begin(), spawners_.end(), [&](const Spawner& candidate) {
                return candidate.assetIndex == *nextAsset;
            });
            if (next == spawners_.end()) continue;
            const auto nextIndex = static_cast<std::size_t>(next - spawners_.begin());
            spawners_[index].next = nextIndex;
            next->predecessor = index;
        }
    }
    prepareProximityStarts();
}

void EncounterSession::clear()
{
    collision_.reset();
    spawners_.clear(); mobs_.clear(); monsters_.clear(); bars_.clear();
    projectiles_.clear(); hits_.clear(); completions_.clear(); activations_.clear();
    swordCooldown_ = hurtCooldown_ = 0;
    playerHitDirection_ = glm::vec3(0);
}

void EncounterSession::prepareProximityStarts()
{
    for (auto& spawner : spawners_) spawner.proximityReady = false;
    for (std::size_t root = 0; root < spawners_.size(); ++root) {
        if (spawners_[root].predecessor) continue;
        auto index = std::optional(root);
        for (std::size_t visited = 0; index && visited < spawners_.size(); ++visited) {
            auto& spawner = spawners_[*index];
            if (!spawner.cleared) {
                spawner.proximityReady = !spawner.complete;
                break;
            }
            index = spawner.next;
        }
    }
}

void EncounterSession::activate(Spawner& spawner, bool chained)
{
    spawner.active = true;
    spawner.proximityReady = false;
    spawner.handoffSeconds = -1;
    activations_.push_back({spawner.position, 0, chained});
    ++activationSerial_;
}

bool EncounterSession::active() const
{
    return std::any_of(spawners_.begin(), spawners_.end(), [](const Spawner& spawner) {
        return (spawner.active || spawner.handoffSeconds >= 0) && !spawner.complete && !spawner.cleared;
    });
}

std::size_t EncounterSession::remaining() const
{
    return std::count_if(spawners_.begin(), spawners_.end(), [](const Spawner& spawner) {
        return !spawner.cleared;
    });
}

void EncounterSession::markCleared(std::size_t assetIndex)
{
    for (auto& spawner : spawners_) {
        if (spawner.assetIndex != assetIndex || !spawner.complete || spawner.cleared) continue;
        spawner.cleared = true;
        std::erase(completions_, assetIndex);
        // Only the caller's successful save releases the next encounter. A
        // pending completion may wait indefinitely without advancing a chain.
        auto next = spawner.next;
        for (std::size_t visited = 0; next && visited < spawners_.size(); ++visited) {
            auto& successor = spawners_[*next];
            if (!successor.cleared) {
                if (!successor.complete && !successor.active) {
                    successor.proximityReady = false;
                    successor.handoffSeconds = ChainDelay;
                }
                break;
            }
            next = successor.next;
        }
        return;
    }
}

void EncounterSession::resetUnfinished()
{
    mobs_.clear(); monsters_.clear(); bars_.clear(); projectiles_.clear(); hits_.clear(); activations_.clear();
    swordCooldown_ = hurtCooldown_ = 0;
    playerHitDirection_ = glm::vec3(0);
    for (auto& spawner : spawners_) {
        spawner.handoffSeconds = -1;
        if (spawner.cleared || spawner.complete) continue;
        spawner.toSpawn = spawner.counts;
        spawner.spawnDelay = .45f;
        spawner.nextKind = spawner.spawnAttempt = 0;
        spawner.active = false;
    }
    prepareProximityStarts();
}

bool EncounterSession::spawnMob(const World& world, const Player& player, std::size_t spawnerIndex)
{
    auto& spawner = spawners_[spawnerIndex];
    int kind = spawner.nextKind;
    for (int attempt = 0; attempt < 3 && !spawner.toSpawn[kind]; ++attempt) kind = (kind + 1) % 3;
    if (!spawner.toSpawn[kind]) return false;
    Player body;
    body.setAssetCollision(collision_);
    // Try supported floor positions on three rings. Never create enemies in
    // the player, on each other, in water, or on the far side of an arena wall.
    for (int attempt = 0; attempt < 48; ++attempt) {
        const int candidate = (attempt + spawner.spawnAttempt) % 48;
        const float angle = float(candidate % 16) * Pi / 8;
        const float radius = 2.2f + float(candidate / 16);
        glm::vec3 point = spawner.position + glm::vec3(std::sin(angle) * radius, 2.5f,
                                                       std::cos(angle) * radius);
        point.x = std::floor(point.x) + .5f; point.z = std::floor(point.z) + .5f;
        if (!World::contains(glm::ivec3(glm::floor(point)))) continue;
        const float floorDistance = obstruction(world, collision_.get(), point, {0, -1, 0}, 5.0f);
        if (floorDistance >= 4.99f) continue;
        point.y -= floorDistance;
        if (std::abs(point.y - spawner.position.y) > 2.1f
            || glm::length(point - player.feetPosition()) < 2.4f) continue;
        if (std::any_of(mobs_.begin(), mobs_.end(), [&](const Mob& mob) {
            return glm::length(mob.body.feetPosition() - point) < 1.5f;
        })) continue;
        if (!body.setFeetPosition(world, point + glm::vec3(0, .003f, 0))) continue;
        body.move(world, {0, -.015f, 0});
        if (!body.grounded() || body.inWater()) continue;
        if (kind == static_cast<int>(MonsterKind::Slime)
            && !slimeFits(world, collision_.get(), body.feetPosition())) continue;
        if (!visible(world, collision_.get(), body.eyePosition(),
                     spawner.position + glm::vec3(0, 1.0f, 0), spawner.assetIndex)) continue;
        const auto monsterKind = static_cast<MonsterKind>(kind);
        MonsterInstance model; model.kind = monsterKind; model.position = body.feetPosition();
        model.yaw = glm::degrees(std::atan2(player.feetPosition().x - model.position.x,
                                          player.feetPosition().z - model.position.z));
        const int hp = maximumHealth(monsterKind);
        mobs_.push_back({body, model, spawnerIndex, hp, hp});
        --spawner.toSpawn[kind];
        spawner.nextKind = (kind + 1) % 3;
        spawner.spawnAttempt = (candidate + 5) % 48;
        return true;
    }
    return false;
}

void EncounterSession::swingSword(const World& world, const Player& player, glm::vec3 direction)
{
    swordCooldown_ = SwordCooldown;
    if (!finite(direction) || glm::length(direction) < .001f) return;
    direction = glm::normalize(direction);
    float nearest = obstruction(world, collision_.get(), player.eyePosition(), direction, SwordReach);
    Mob* target = nullptr;
    for (auto& mob : mobs_) {
        if (mob.health <= 0) continue;
        const glm::vec3 feet = mob.body.feetPosition();
        const float half = mob.model.kind == MonsterKind::Slime ? .72f : .47f;
        const float hit = bodyHit(player.eyePosition(), direction, feet + glm::vec3(-half, 0, -half),
                                  feet + glm::vec3(half, height(mob.model.kind), half), nearest);
        if (hit >= 0 && hit < nearest) { nearest = hit; target = &mob; }
    }
    if (!target) return;
    target->health = std::max(0, target->health - SwordDamage);
    target->healthVisible = 4.0f;
    target->recoilRemaining = HitRecoilDuration;
    target->model.hitReaction = 1;
    target->model.hitDirection = horizontal(target->body.feetPosition() - player.feetPosition());
    // Interrupt the windup as well as chasing: otherwise the AI immediately
    // cancels the shove, or lands a hit while visibly recoiling.
    target->attackTime = -1;
    target->attackFired = false;
    target->cooldown = std::max(target->cooldown, .48f);
    target->hopDelay = std::max(target->hopDelay, .35f);
    animation(target->model, MonsterAnimation::Idle);
    if (hits_.size() >= 32) hits_.erase(hits_.begin());
    hits_.push_back({player.eyePosition() + direction * nearest, target->model.hitDirection,
                     target->model.kind, 0, target->health == 0});
}

int EncounterSession::updateMob(Mob& mob, const World& world, const Player& player, float seconds)
{
    mob.age += seconds;
    mob.cooldown = std::max(0.0f, mob.cooldown - seconds);
    mob.hopDelay = std::max(0.0f, mob.hopDelay - seconds);
    mob.healthVisible = std::max(0.0f, mob.healthVisible - seconds);
    mob.model.transition = std::min(1.0f, mob.model.transition + seconds / .16f);
    const bool recoiling = mob.recoilRemaining > 0;
    const float recoilBefore = mob.recoilRemaining / HitRecoilDuration;
    mob.recoilRemaining = std::max(0.0f, mob.recoilRemaining - seconds);
    const float recoilAfter = mob.recoilRemaining / HitRecoilDuration;
    mob.model.hitReaction = recoilAfter;
    // Integrate a decaying velocity so the same shove takes the same distance
    // at low and high frame rates, without moving through colliders in one jump.
    const float knockback = HitKnockbackDistance
        * (recoilBefore * recoilBefore - recoilAfter * recoilAfter);
    const auto feet = mob.body.feetPosition();
    const auto offset = player.feetPosition() - feet;
    const float distance = glm::length(offset);
    glm::vec3 direction = horizontal(offset);
    const auto target = player.feetPosition() + glm::vec3(0, .9f, 0);
    const bool sight = visible(world, collision_.get(), feet + glm::vec3(0, 1.1f, 0), target);
    const bool wizard = mob.model.kind == MonsterKind::Wizard;
    const bool slime = mob.model.kind == MonsterKind::Slime;
    const float attackRange = wizard ? 8.0f : slime ? 3.1f : 2.0f;
    if (!recoiling && mob.attackTime < 0 && mob.cooldown == 0 && distance < attackRange && sight) {
        mob.attackTime = 0;
        mob.attackFired = false;
        mob.attackDirection = direction;
        animation(mob.model, MonsterAnimation::Attack);
    }

    bool jump = false;
    float speed = 0;
    int damage = 0;
    if (recoiling) {
        animation(mob.model, MonsterAnimation::Idle);
        mob.model.seconds += seconds;
    } else if (mob.attackTime >= 0) {
        const float previous = mob.attackTime;
        mob.attackTime += seconds;
        const float duration = wizard ? 2.6f : slime ? 1.65f : 1.5f;
        const float impact = wizard ? 1.3f : slime ? .60f : .62f;
        mob.model.seconds = mob.attackTime / duration * monsterCycleDuration(mob.model.kind, MonsterAnimation::Attack);
        // A slime commits to its pounce direction, giving the player a chance
        // to sidestep. Skeletons also stop turning once their slash starts.
        if (!wizard) direction = mob.attackDirection;
        if (slime && mob.attackTime > .21f && mob.attackTime < .83f) {
            speed = 2.7f;
            jump = previous <= .21f;
        }
        if (!mob.attackFired && mob.attackTime >= impact) {
            mob.attackFired = true;
            if (wizard) {
                const auto origin = feet + glm::vec3(0, 1.55f, 0) + direction * .55f;
                if (visible(world, collision_.get(), feet + glm::vec3(0, 1.55f, 0), origin)
                    && glm::length(target - origin) > .01f) {
                    projectiles_.push_back({origin, origin, glm::normalize(target - origin) * 5.0f, 4.0f});
                }
            } else {
                const float reach = slime ? 2.2f : 2.35f;
                if (distance < reach && sight && glm::dot(horizontal(offset), mob.attackDirection) > .35f)
                    damage = slime ? 12 : 16;
            }
        }
        if (mob.attackTime >= duration) {
            mob.attackTime = -1;
            mob.cooldown = wizard ? .8f : slime ? .7f : .65f;
            animation(mob.model, MonsterAnimation::Idle);
        }
    } else {
        if (wizard) {
            if (distance > 6.0f) speed = 1.3f;
            else if (distance < 2.6f) { speed = 1.0f; direction = -direction; }
        } else if (distance > (slime ? 1.7f : 1.45f)) speed = slime ? 1.6f : 2.0f;
        if (slime && speed > 0 && mob.body.grounded() && mob.hopDelay == 0) {
            jump = true; mob.hopDelay = 1.2f;
        }
        animation(mob.model, speed > 0 ? MonsterAnimation::Walk : MonsterAnimation::Idle);
        mob.model.seconds += seconds;
    }

    // Do not overlap the player just to reach an attack distance. Gravity
    // still runs while a monster waits or winds up an attack.
    if (distance < .9f && glm::dot(direction, horizontal(offset)) > 0) speed = 0;
    Player moved = mob.body;
    moved.update(world, {0, speed / 4.5f}, direction, jump, false, seconds);
    // Gravity still runs while stunned, and the regular swept movement plus
    // the broad-slime check below also resolve knockback against scenery.
    if (knockback > 0) moved.move(world, mob.model.hitDirection * knockback);
    // Slide around simple obstructions instead of endlessly walking directly
    // into them. Each alternate move uses the real terrain/model sweeps too.
    if (speed > 0 && mob.attackTime < 0 && !jump && mob.body.grounded()
        && glm::length(glm::vec2(moved.feetPosition().x - feet.x, moved.feetPosition().z - feet.z))
            < speed * seconds * .3f) {
        const glm::vec3 side{-direction.z, 0, direction.x};
        float best = -std::numeric_limits<float>::infinity();
        for (float turn : {1.0f, -1.0f}) {
            Player alternate = mob.body;
            alternate.update(world, {0, speed / 4.5f}, horizontal(direction * .3f + side * turn),
                             false, false, seconds);
            const auto delta = alternate.feetPosition() - feet;
            const float score = glm::length(delta) + glm::dot(delta, direction) * .1f;
            if (score > best) { best = score; moved = std::move(alternate); }
        }
    }
    if (slime && !slimeFits(world, collision_.get(), moved.feetPosition())) {
        Player vertical = mob.body;
        vertical.update(world, {}, direction, jump, false, seconds);
        const auto displacement = moved.feetPosition() - vertical.feetPosition();
        moved = vertical;
        // Resolve the wide body's first contact. Frame movement and knockback
        // are shorter than its diameter, so an endpoint overlap cannot skip a
        // thin obstacle. Swept Player movement remains authoritative underneath.
        float low = 0, high = 1;
        for (int attempt = 0; attempt < 7; ++attempt) {
            const float fraction = (low + high) * .5f;
            Player candidate = vertical;
            candidate.move(world, {displacement.x * fraction, 0, displacement.z * fraction});
            if (slimeFits(world, collision_.get(), candidate.feetPosition())) {
                low = fraction; moved = std::move(candidate);
            } else high = fraction;
        }
    }
    // Keep a little space between mobs so a wave does not turn into one
    // unreadable stack. Vertical physics can continue in place when crowded.
    const bool crowded = std::any_of(mobs_.begin(), mobs_.end(), [&](const Mob& other) {
        return &other != &mob && other.health > 0
            && glm::length(other.body.feetPosition() - moved.feetPosition()) < .8f
            && glm::length(other.body.feetPosition() - feet)
                > glm::length(other.body.feetPosition() - moved.feetPosition());
    });
    if (crowded) mob.body.update(world, {}, direction, jump, false, seconds);
    else mob.body = std::move(moved);
    mob.model.position = mob.body.feetPosition();
    mob.model.yaw = glm::degrees(std::atan2(direction.x, direction.z));
    if (slime) {
        // The preview's pounce moves its root in place. In a real encounter
        // physics owns that movement; retain its squash and stretch without
        // adding a second jump or pushing the visible body through scenery.
        const auto pose = blendedMonsterPose(mob.model);
        if (!pose.empty()) {
            const auto root = pose.front().translation;
            const glm::vec3 right{direction.z, 0, -direction.x};
            mob.model.position -= right * root.x + direction * root.z + glm::vec3(0, root.y, 0);
        }
    }
    // An enemy that falls out of the arena cannot strand the encounter forever.
    if (mob.body.feetPosition().y < -8) mob.health = 0;
    return damage;
}

void EncounterSession::checkCompletions()
{
    for (std::size_t index = 0; index < spawners_.size(); ++index) {
        auto& spawner = spawners_[index];
        if (!spawner.active || spawner.complete || spawner.cleared) continue;
        if (std::accumulate(spawner.toSpawn.begin(), spawner.toSpawn.end(), 0) != 0) continue;
        if (std::any_of(mobs_.begin(), mobs_.end(), [&](const Mob& mob) { return mob.spawner == index; })) continue;
        spawner.complete = true;
        completions_.push_back(spawner.assetIndex);
    }
}

void EncounterSession::refreshViews()
{
    monsters_.clear(); bars_.clear();
    for (const auto& mob : mobs_) {
        monsters_.push_back(mob.model);
        bars_.push_back({mob.body.feetPosition() + glm::vec3(0, height(mob.model.kind) + .3f, 0),
                         mob.health, mob.maximum, mob.healthVisible});
    }
}

int EncounterSession::update(const World& world, const Player& player, glm::vec3 aimDirection,
                             bool swordAttack, float deltaSeconds)
{
    playerHitDirection_ = glm::vec3(0);
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0) return 0;
    const float seconds = std::min(deltaSeconds, .1f);
    swordCooldown_ = std::max(0.0f, swordCooldown_ - seconds);
    hurtCooldown_ = std::max(0.0f, hurtCooldown_ - seconds);
    for (auto& hit : hits_) hit.age += seconds;
    std::erase_if(hits_, [](const EncounterHit& hit) { return hit.age >= EncounterHit::Lifetime; });
    for (auto& activation : activations_) activation.age += seconds;
    std::erase_if(activations_, [](const EncounterActivation& activation) {
        return activation.age >= EncounterActivation::Lifetime;
    });
    for (std::size_t index = 0; index < spawners_.size(); ++index) {
        auto& spawner = spawners_[index];
        if (spawner.cleared || spawner.complete) continue;
        if (!spawner.active && spawner.handoffSeconds >= 0) {
            spawner.handoffSeconds = std::max(0.0f, spawner.handoffSeconds - seconds);
            if (spawner.handoffSeconds <= .00001f) {
                activate(spawner, true);
                // Keep the regular spawn preparation after the two-second
                // handoff; this frame's time was already spent waiting.
                continue;
            }
        }
        if (!spawner.active && spawner.proximityReady
            && glm::length(player.feetPosition() - spawner.position) <= ActivationRadius
            && visible(world, collision_.get(), player.eyePosition(),
                       spawner.position + glm::vec3(0, 1.0f, 0), spawner.assetIndex)) activate(spawner, false);
        if (!spawner.active) continue;
        spawner.spawnDelay = std::max(0.0f, spawner.spawnDelay - seconds);
        if (spawner.spawnDelay == 0 && mobs_.size() < MaxAlive) {
            spawnMob(world, player, index);
            spawner.spawnDelay = SpawnInterval;
        }
    }
    if (swordAttack && swordCooldown_ == 0) swingSword(world, player, aimDirection);
    std::erase_if(mobs_, [](const Mob& mob) { return mob.health <= 0; });
    int damage = 0;
    glm::vec3 damageDirection(0);
    for (auto& mob : mobs_) {
        const auto away = hitDirection(player.feetPosition() - mob.body.feetPosition());
        const int hit = updateMob(mob, world, player, seconds);
        if (hit > damage) { damage = hit; damageDirection = away; }
    }
    std::erase_if(mobs_, [](const Mob& mob) { return mob.health <= 0; });
    for (auto& projectile : projectiles_) {
        projectile.previousPosition = projectile.position;
        const float travel = glm::length(projectile.velocity) * seconds;
        const auto direction = glm::normalize(projectile.velocity);
        const float wall = obstruction(world, collision_.get(), projectile.position, direction, travel);
        const auto feet = player.feetPosition();
        const float hit = bodyHit(projectile.position, direction, feet + glm::vec3(-.44f, -.1f, -.44f),
                                  feet + glm::vec3(.44f, Player::Height + .1f, .44f), travel);
        if (hit >= 0 && hit < wall) {
            // Direction follows the bolt that actually landed, even if its
            // caster has moved. The strongest same-frame hit owns the feedback.
            if (damage < 18) { damage = 18; damageDirection = hitDirection(projectile.velocity); }
            projectile.remainingSeconds = 0;
        }
        else if (wall < travel) projectile.remainingSeconds = 0;
        projectile.position += direction * wall;
        projectile.remainingSeconds -= seconds;
    }
    std::erase_if(projectiles_, [](const EncounterProjectile& projectile) { return projectile.remainingSeconds <= 0; });
    checkCompletions();
    refreshViews();
    if (damage && hurtCooldown_ == 0) {
        hurtCooldown_ = PlayerHitCooldown;
        playerHitDirection_ = damageDirection;
        return damage;
    }
    return 0;
}

} // namespace voxel
