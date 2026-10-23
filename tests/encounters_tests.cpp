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
    for (int i = 0; i < 3; ++i) {
        const auto monster = session.monsters().front();
        session.update(world, player, aim(player, monster), true, .1f);
    }
    require(session.healthBars().front().current == 15 && session.pendingCompletions().empty(),
            "holding attack cannot bypass the sword cooldown");
    tick(session, world, player, 2);
    strike(session, world, player);
    require(session.monsters().empty() && session.pendingCompletions().size() == 1
            && session.pendingCompletions().front() == 0 && session.remaining() == 1,
            "last kill reports the authored spawner index and waits for a durable completion");
    session.resetUnfinished();
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
    require(session.monsters().empty() && session.projectiles().empty() && !session.active()
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
        const int damage = tick(session, world, player);
        telegraphed |= session.monsters().front().animation == MonsterAnimation::Attack;
        if (damage) {
            require(telegraphed && damage == 16 && sinceHit >= 6,
                    "skeleton slashes are telegraphed, timed and respect the player hit cooldown");
            damaged = true; sinceHit = 0;
        }
        ++sinceHit;
    }
    require(damaged, "a nearby skeleton eventually completes its melee attack");

    const auto wizard = spawner({0, 1, 0});
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
                && session.monsters().front().position == before,
                "invalid time steps cannot move, swing, or damage anything");
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
        std::cout << "Encounter tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
