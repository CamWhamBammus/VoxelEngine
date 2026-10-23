#include "encounters.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <glm/geometric.hpp>

namespace {
using namespace voxel;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

HubAsset spawner(std::array<int, 3> counts, glm::vec3 position = {.5f, 9, .5f})
{
    HubAsset asset{HubAssetKind::TrialSpawner, position};
    asset.spawner.counts = counts;
    return asset;
}

void position(Player& player, const World& world, glm::vec3 feet)
{
    require(player.setFeetPosition(world, feet), "player fixture position must fit");
}

int tick(EncounterSession& session, const World& world, const Player& player, int frames = 1)
{
    int damage = 0;
    for (int i = 0; i < frames; ++i) damage += session.update(world, player, {0, 0, -1}, false, .1f);
    return damage;
}

void floor(World& world)
{
    for (int z = -24; z <= 24; ++z)
        for (int x = -24; x <= 24; ++x)
            for (int y = 0; y <= 40; ++y) world.set({x, y, z}, y == 8 ? Block::Stone : Block::Air);
}

void wall(World& world, int z, Block material)
{
    for (int x = -24; x <= 24; ++x)
        for (int y = 9; y <= 15; ++y) world.set({x, y, z}, material);
}

void start(EncounterSession& session, World& world, Player& player, const HubAsset& asset)
{
    position(player, world, {.5f, 9, 7.5f});
    session.begin(std::span(&asset, 1), {});
    tick(session, world, player, 5);
    require(session.monsters().size() == 1, "first enemy appears gradually after proximity activation");
}

glm::vec3 aim(const Player& player, const MonsterInstance& monster)
{
    return glm::normalize(monster.position + glm::vec3(0, .8f, 0) - player.eyePosition());
}

void strike(EncounterSession& session, World& world, Player& player)
{
    require(!session.monsters().empty(), "strike needs a target");
    const auto monster = session.monsters().front();
    position(player, world, monster.position + glm::vec3(0, 0, 2.4f));
    session.update(world, player, aim(player, monster), true, .01f);
}

void spawnAndSwordChecks(World& world)
{
    const auto asset = spawner({1, 0, 0});
    EncounterSession session;
    Player player;
    position(player, world, {.5f, 9, 12.5f});
    session.begin(std::span(&asset, 1), {});
    tick(session, world, player, 30);
    require(!session.active() && session.monsters().empty() && session.remaining() == 1,
            "distant spawners stay dormant but still count as uncleared");
    position(player, world, {.5f, 9, 7.5f});
    require(tick(session, world, player) == 0 && session.active() && session.monsters().empty(),
            "proximity starts a short visible preparation interval, not an instant hit");
    require(tick(session, world, player, 4) == 0 && session.monsters().size() == 1,
            "an enemy starts with a grace period before attacking");
    require(session.healthBars().front().current == 40 && session.healthBars().front().visibleSeconds == 0,
            "enemy health bars stay hidden before the first sword hit");
    strike(session, world, player);
    require(session.healthBars().front().current == 15 && session.healthBars().front().visibleSeconds > 3.8f
            && session.swordSwing() == 1, "one aimed sword hit deals25 damage and reveals enemy health");
    require(session.hits().size() == 1 && !session.hits().front().lethal,
            "a real damaging contact creates one nonlethal impact");
    for (int i = 0; i < 3; ++i) {
        const auto monster = session.monsters().front();
        session.update(world, player, aim(player, monster), true, .1f);
    }
    require(session.healthBars().front().current == 15 && session.pendingCompletions().empty()
            && session.hits().size() == 1 && session.hits().front().age > .29f,
            "holding attack cannot bypass the sword cooldown");
    tick(session, world, player, 2);
    strike(session, world, player);
    require(session.monsters().empty() && session.pendingCompletions().size() == 1
            && session.pendingCompletions().front() == 0 && session.remaining() == 1,
            "last kill reports the authored spawner index and waits for a durable completion");
    require(session.hits().size() == 1 && session.hits().front().lethal && session.hits().front().age == 0,
            "the lethal contact survives removal of the enemy");
    session.resetUnfinished();
    require(session.hits().empty(), "respawning clears outstanding impacts");
    tick(session, world, player, 30);
    require(session.monsters().empty() && session.pendingCompletions().size() == 1,
            "a pending completion survives respawn so failed saves cannot duplicate enemies or rewards");
    session.markCleared(0);
    require(session.remaining() == 0 && session.pendingCompletions().empty() && !session.active(),
            "acknowledging a saved completion releases the section exit");
    session.resetUnfinished();
    tick(session, world, player, 30);
    require(session.monsters().empty(), "completed spawners never restart after respawn");
    const std::array<std::size_t, 1> cleared{0};
    session.begin(std::span(&asset, 1), {}, cleared);
    tick(session, world, player, 30);
    require(session.remaining() == 0 && session.monsters().empty(), "stored clears are applied on section entry");

    start(session, world, player, asset);
    strike(session, world, player);
    session.resetUnfinished();
    require(session.monsters().empty() && session.projectiles().empty() && session.hits().empty() && !session.active()
            && session.swordSwing() == 0 && session.remaining() == 1,
            "unfinished encounters reset all transient combat state on death");
    position(player, world, {.5f, 9, 7.5f});
    tick(session, world, player, 5);
    require(session.monsters().size() == 1 && session.healthBars().front().current == 40,
            "an unfinished retry restores authored counts and full enemy health");
}

void spawningChecks(World& world)
{
    EncounterSession session;
    Player player;
    position(player, world, {.5f, 9, 7.5f});
    const auto mixed = spawner({2, 1, 1});
    session.begin(std::span(&mixed, 1), {});
    tick(session, world, player, 40);
    require(session.monsters().size() == 4, "a finite mixed encounter spawns the selected total");
    std::array<int, 3> counts{};
    for (const auto& monster : session.monsters()) ++counts[static_cast<int>(monster.kind)];
    require(counts == mixed.spawner.counts, "all three monster counts are honored independently");
    tick(session, world, player, 60);
    require(session.monsters().size() == 4, "spawners do not repeat their wave once all requested mobs exist");

    const std::array crowded{spawner({20, 20, 20}), spawner({20, 20, 20}, {5.5f, 9, .5f})};
    session.begin(crowded, {});
    tick(session, world, player, 150);
    require(session.monsters().size() == EncounterSession::MaxAlive,
            "the live mob cap is shared across all nearby spawners");
    for (const auto& monster : session.monsters()) require(monster.position.y >= 8.99f,
            "spawning requires solid floor support");

    const auto floating = spawner({1, 0, 0}, {.5f, 35, .5f});
    position(player, world, {.5f, 35, 6.5f});
    session.begin(std::span(&floating, 1), {});
    tick(session, world, player, 30);
    require(session.active() && session.monsters().empty(),
            "a spawner with no supported spawn spots waits instead of creating floating enemies");
}

void obstructionChecks(World& world)
{
    EncounterSession session;
    Player player;
    const auto asset = spawner({1, 0, 0});
    position(player, world, {.5f, 9, 7.5f});
    wall(world, 4, Block::Stone);
    session.begin(std::span(&asset, 1), {});
    tick(session, world, player, 30);
    require(!session.active(), "proximity through a solid wall does not activate a different room's spawner");
    wall(world, 4, Block::Air);
    tick(session, world, player, 5);
    require(session.monsters().size() == 1, "opening line of sight lets the nearby encounter start");
    const int barrier = int(std::floor(session.monsters().front().position.z)) + 1;
    wall(world, barrier, Block::Stone);
    position(player, world, {.5f, 9, float(barrier) + 1.5f});
    for (int frame = 0; frame < 50; ++frame) {
        session.update(world, player, aim(player, session.monsters().front()), true, .1f);
        require(session.healthBars().front().current == 40, "sword strikes cannot damage enemies through terrain");
        require(session.hits().empty(), "a wall-blocked sword produces no enemy damage effects");
        require(session.monsters().front().position.z <= float(barrier) - .29f,
                "chasing monsters respect terrain collision");
    }
    wall(world, barrier, Block::Air);

    const std::array placements{asset,
        HubAsset{HubAssetKind::TimberPillar, {.5f, 9, 4.5f}},
        HubAsset{HubAssetKind::TimberPillar, {.5f, 10, 4.5f}}};
    auto collision = std::make_shared<AssetCollision>(); collision->rebuild(placements);
    position(player, world, {.5f, 9, 7.5f});
    session.begin(placements, collision);
    tick(session, world, player, 20);
    require(!session.active(), "decorative model collision blocks activation rays too");
    position(player, world, {3.5f, 9, 6.5f});
    tick(session, world, player, 5);
    require(!session.monsters().empty(), "the spawner's own visible model does not block its activation");

    const auto slime = spawner({0, 0, 1});
    start(session, world, player, slime);
    const int slimeWall = int(std::floor(session.monsters().front().position.z)) + 2;
    wall(world, slimeWall, Block::Stone);
    for (int frame = 0; frame < 100; ++frame) {
        tick(session, world, player);
        // The health anchor tracks the physics body, while the model's origin
        // compensates for the preview animation's local pounce translation.
        require(session.healthBars().front().position.z < float(slimeWall) - .68f,
                "the slime's broad sides stay outside walls while it chases and pounces");
    }
    wall(world, slimeWall, Block::Air);
}

void hitFeedbackChecks(World& world)
{
    EncounterSession session;
    Player player;
    const auto skeleton = spawner({1, 0, 0});
    start(session, world, player, skeleton);
    auto monster = session.monsters().front();
    position(player, world, monster.position + glm::vec3(0, 0, 2.4f));
    session.update(world, player, {0, 1, 0}, true, .01f);
    require(session.hits().empty() && session.monsters().front().hitReaction == 0,
            "a missed sword swing has no impact or enemy recoil");
    tick(session, world, player, 5);
    monster = session.monsters().front();
    position(player, world, monster.position + glm::vec3(0, 0, 2.4f));
    const auto direction = aim(player, monster);
    const auto contact = player.eyePosition() + direction
        * ((monster.position.z + .47f - player.eyePosition().z) / direction.z);
    const auto before = monster.position;
    require(session.update(world, player, direction, true, .01f) == 0,
            "a sword hit interrupts attacks before they damage the player");
    require(session.hits().size() == 1 && session.hits().front().kind == MonsterKind::Skeleton
            && glm::length(session.hits().front().position - contact) < .002f
            && glm::length(session.hits().front().direction - glm::vec3(0, 0, -1)) < .001f,
            "damage effects use the ray's visible contact and the shove's horizontal direction");
    float previousDistance = 0;
    for (const float seconds : {.08f, .08f, .09f}) {
        require(session.update(world, player, direction, false, seconds) == 0,
                "a recoiling enemy cannot attack");
        const auto current = session.monsters().front();
        const float distance = before.z - current.position.z;
        require(distance > previousDistance && current.animation != MonsterAnimation::Attack,
                "knockback visibly advances across several frames while chasing is paused");
        require(std::abs(current.position.y - before.y) < .001f,
                "grounded recoil keeps the enemy on its floor");
        previousDistance = distance;
    }
    require(std::abs(previousDistance - .62f) < .003f
            && session.monsters().front().hitReaction < .001f,
            "decaying recoil travels a short predictable distance and settles to zero");
    const float stoppedZ = session.monsters().front().position.z;
    tick(session, world, player);
    require(session.monsters().front().position.z > stoppedZ + .1f,
            "normal chasing recovers after the short hit reaction");
    tick(session, world, player);
    require(session.hits().empty(), "contact effects expire even while the encounter continues");

    // The same integration at a different frame rate gives the same shove.
    start(session, world, player, skeleton);
    monster = session.monsters().front();
    position(player, world, monster.position + glm::vec3(0, 0, 2.4f));
    session.update(world, player, aim(player, monster), true, .1f);
    session.update(world, player, {}, false, .1f);
    session.update(world, player, {}, false, .06f);
    require(std::abs(monster.position.z - session.monsters().front().position.z - .62f) < .003f,
            "the shove does not depend on the frame rate");
    session.begin(std::span(&skeleton, 1), {});
    require(session.hits().empty(), "entering another section clears old hit effects");

    start(session, world, player, skeleton);
    monster = session.monsters().front();
    position(player, world, monster.position + glm::vec3(0, 0, 1.6f));
    for (int frame = 0; frame < 30
            && session.monsters().front().animation != MonsterAnimation::Attack; ++frame)
        tick(session, world, player);
    require(session.monsters().front().animation == MonsterAnimation::Attack,
            "interruption fixture has a winding-up skeleton");
    session.update(world, player, aim(player, session.monsters().front()), true, .01f);
    require(session.monsters().front().animation != MonsterAnimation::Attack
            && session.monsters().front().hitReaction > .9f
            && tick(session, world, player, 4) == 0,
            "a successful strike cancels the current enemy attack instead of merely hiding its animation");
    session.clear();
    require(session.hits().empty(), "leaving combat clears hit effects");

    // A shove off a ledge must not suspend gravity for the hit animation.
    start(session, world, player, skeleton);
    strike(session, world, player);
    const auto falling = session.monsters().front().position;
    for (int z = int(std::floor(falling.z)) - 1; z <= int(std::floor(falling.z)) + 1; ++z)
        world.set({int(std::floor(falling.x)), 8, z}, Block::Air);
    tick(session, world, player, 2);
    require(session.monsters().front().position.y < falling.y - .1f,
            "gravity continues while an enemy recoils over a gap");
    for (int z = int(std::floor(falling.z)) - 1; z <= int(std::floor(falling.z)) + 1; ++z)
        world.set({int(std::floor(falling.x)), 8, z}, Block::Stone);
}

void knockbackCollisionChecks(World& world)
{
    EncounterSession session;
    Player player;
    const auto skeleton = spawner({1, 0, 0});
    start(session, world, player, skeleton);
    const auto before = session.monsters().front().position;
    const int barrier = int(std::floor(before.z)) - 1;
    wall(world, barrier, Block::Stone);
    strike(session, world, player);
    tick(session, world, player, 2);
    require(session.monsters().front().position.z < before.z
            && session.monsters().front().position.z >= float(barrier + 1) + Player::Width / 2 - .001f,
            "a sustained shove stops at the terrain wall behind the enemy");
    wall(world, barrier, Block::Air);

    for (const auto counts : {std::array{1, 0, 0}, std::array{0, 0, 1}}) {
        const auto asset = spawner(counts);
        auto collision = std::make_shared<AssetCollision>();
        position(player, world, {.5f, 9, 7.5f});
        session.begin(std::span(&asset, 1), collision);
        tick(session, world, player, 5);
        require(session.monsters().size() == 1, "fence fixture must spawn an enemy");
        const bool slime = counts[2] != 0;
        const auto feet = session.healthBars().front().position - glm::vec3(0, slime ? 1.4f : 2.4f, 0);
        const float half = slime ? .69f : Player::Width / 2;
        const std::array fence{HubAsset{HubAssetKind::TimberFence,
            feet - glm::vec3(0, 0, half + .35f)}};
        collision->rebuild(fence);
        position(player, world, feet + glm::vec3(0, 0, 2.4f));
        session.update(world, player, glm::normalize(feet + glm::vec3(0, .8f, 0) - player.eyePosition()), true, .01f);
        for (int frame = 0; frame < 3; ++frame) {
            const auto current = session.healthBars().front().position - glm::vec3(0, slime ? 1.4f : 2.4f, 0);
            require(!collision->overlaps(current + glm::vec3(-half, .002f, -half),
                                         current + glm::vec3(half, slime ? 1.08f : Player::Height, half)),
                    "every knockback frame keeps the full mob body outside decorative fence rails");
            if (frame < 2) tick(session, world, player);
        }
        const auto after = session.healthBars().front().position;
        require(feet.z - after.z > .02f && feet.z - after.z < .4f,
                "a thin fence stops the shove, including the slime's broad sides");
    }
}

void attackChecks(World& world)
{
    EncounterSession session;
    Player player;
    const auto skeleton = spawner({1, 0, 0});
    start(session, world, player, skeleton);
    auto monster = session.monsters().front();
    position(player, world, monster.position + glm::vec3(0, 0, 1.6f));
    require(tick(session, world, player, 6) == 0, "freshly spawned melee enemies cannot deal immediate contact damage");
    bool telegraphed = false, damaged = false;
    int sinceHit = 100;
    for (int frame = 0; frame < 100; ++frame) {
        auto away = player.feetPosition() - session.monsters().front().position;
        away.y = 0; away = glm::normalize(away);
        const int damage = tick(session, world, player);
        telegraphed |= session.monsters().front().animation == MonsterAnimation::Attack;
        if (damage) {
            require(telegraphed && damage == 16 && sinceHit >= 6,
                    "skeleton slashes are telegraphed, timed and respect the player hit cooldown");
            require(glm::dot(session.playerHitDirection(), away) > .999f
                    && session.playerHitDirection().y == 0,
                    "a successful melee hit reports the direction away from its attacker");
            damaged = true; sinceHit = 0;
        } else require(session.playerHitDirection() == glm::vec3(0),
                       "misses and cooldown frames cannot replay the previous player hit direction");
        ++sinceHit;
    }
    require(damaged, "a nearby skeleton eventually completes its melee attack");

    const auto wizard = spawner({0, 1, 0});
    start(session, world, player, wizard);
    bool boltHit = false;
    glm::vec3 boltDirection(0);
    for (int frame = 0; frame < 120 && !boltHit; ++frame) {
        if (!session.projectiles().empty()) {
            boltDirection = session.projectiles().front().velocity;
            boltDirection.y = 0; boltDirection = glm::normalize(boltDirection);
        }
        const int damage = tick(session, world, player);
        if (damage) {
            require(damage == 18 && glm::dot(session.playerHitDirection(), boltDirection) > .999f,
                    "a wizard hit reports the traveling bolt's horizontal direction");
            boltHit = true;
        } else require(session.playerHitDirection() == glm::vec3(0),
                       "a bolt still in flight does not create player damage feedback");
    }
    require(boltHit, "a stationary player eventually receives a visible wizard bolt");
    session.resetUnfinished();
    require(session.playerHitDirection() == glm::vec3(0), "retrying combat clears the accepted hit direction");
    start(session, world, player, wizard);
    for (int frame = 0; frame < 80 && session.projectiles().empty(); ++frame) tick(session, world, player);
    require(!session.projectiles().empty(), "wizards visibly cast projectiles after their windup");
    const auto projectile = session.projectiles().front();
    require(std::abs(glm::length(projectile.velocity) - 5.0f) < .01f,
            "wizard bolts travel slowly enough to dodge");
    position(player, world, player.feetPosition() + glm::vec3(6, 0, 0));
    require(tick(session, world, player, 15) == 0, "moving out of a projectile's path dodges its fixed trajectory");

    start(session, world, player, wizard);
    for (int frame = 0; frame < 80 && session.projectiles().empty(); ++frame) tick(session, world, player);
    const int barrier = int(std::floor(session.projectiles().front().position.z)) + 1;
    wall(world, barrier, Block::Stone);
    require(tick(session, world, player, 10) == 0 && session.projectiles().empty(),
            "projectiles sweep into terrain instead of passing through it to hit the player");
    wall(world, barrier, Block::Air);

    auto collision = std::make_shared<AssetCollision>();
    position(player, world, {.5f, 9, 7.5f});
    session.begin(std::span(&wizard, 1), collision);
    for (int frame = 0; frame < 80 && session.projectiles().empty(); ++frame) tick(session, world, player);
    require(!session.projectiles().empty(), "model obstruction fixture has a traveling bolt");
    const auto bolt = session.projectiles().front();
    const auto obstacle = bolt.position + glm::normalize(bolt.velocity) * 1.3f;
    const std::array pillars{
        HubAsset{HubAssetKind::TimberPillar, {obstacle.x, 9, obstacle.z}},
        HubAsset{HubAssetKind::TimberPillar, {obstacle.x, 10, obstacle.z}}};
    collision->rebuild(pillars);
    require(tick(session, world, player, 10) == 0 && session.projectiles().empty(),
            "projectile sweeps also stop on solid decorative assets");

    const auto slime = spawner({0, 0, 1});
    start(session, world, player, slime);
    bool hopping = false;
    damaged = false;
    for (int frame = 0; frame < 120; ++frame) {
        const int damage = tick(session, world, player);
        hopping |= session.monsters().front().position.y > 9.2f;
        damaged |= damage == 12;
    }
    require(hopping && damaged, "slimes physically hop and complete damaging pounces");

    const auto before = session.monsters().front().position;
    for (float time : {-1.0f, 0.0f, std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity()})
        require(session.update(world, player, {0, 0, -1}, true, time) == 0
                && session.monsters().front().position == before
                && session.playerHitDirection() == glm::vec3(0),
                "invalid time steps cannot move, swing, or damage anything");
    session.clear();
    require(session.playerHitDirection() == glm::vec3(0), "leaving combat clears the hit direction");
}

} // namespace

int main()
{
    try {
        voxel::World world;
        floor(world);
        spawnAndSwordChecks(world);
        spawningChecks(world);
        obstructionChecks(world);
        attackChecks(world);
        hitFeedbackChecks(world);
        knockbackCollisionChecks(world);
        std::cout << "Encounter tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
