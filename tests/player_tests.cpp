#include "player.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <glm/geometric.hpp>

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(float first, float second, float tolerance = 0.0001f)
{
    return std::abs(first - second) < tolerance;
}
} // namespace

int main()
{
    using voxel::Block;
    using voxel::Player;
    using voxel::World;
    World world;
    for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
                world.set({x, y, z}, y == 0 ? Block::Stone : Block::Air);
            }
        }
    }

    Player player;
    check(player.setFeetPosition(world, {0.5f, 1.0f, 0.5f}), "body can stand on a floor");
    check(player.grounded(), "floor contact is grounded");
    check(near(player.eyePosition().y, 2.62f), "eye height is measured from the feet");
    check(player.overlapsBlock({0, 1, 0}) && player.overlapsBlock({0, 2, 0}),
        "placement checks include the entire body height");
    check(!player.overlapsBlock({0, 0, 0}) && !player.overlapsBlock({1, 1, 0})
        && !player.overlapsBlock({0, 3, 0}), "touching or separated blocks do not overlap the body");

    world.set({0, 1, 0}, Block::Water);
    world.set({0, 2, 0}, Block::Water);
    check(player.setFeetPosition(world, {0.5f, 1.0f, 0.5f}) && player.grounded(),
          "visible water does not collide with the player or replace ground support");
    world.set({0, 1, 0}, Block::Leaves);
    check(!player.setFeetPosition(world, {0.5f, 1.0f, 0.5f}), "leaf blocks still collide");
    world.set({0, 1, 0}, Block::Air);
    world.set({0, 2, 0}, Block::Air);

    for (int y = 1; y <= 4; ++y) {
        for (int z = 0; z <= 6; ++z) {
            world.set({4, y, z}, Block::Stone);
            world.set({-4, y, z}, Block::Stone);
        }
    }
    player.move(world, {20, 0, 0});
    check(near(player.feetPosition().x, 3.7f), "fast positive movement cannot tunnel through a wall");
    player.move(world, {10, 0, 4});
    check(near(player.feetPosition().x, 3.7f) && near(player.feetPosition().z, 4.5f),
        "diagonal movement slides along a touching wall");
    player.move(world, {-20, 0, 0});
    check(near(player.feetPosition().x, -2.7f), "negative world coordinates collide with wall faces");
    const auto beforeInvalidPosition = player.feetPosition();
    check(!player.setFeetPosition(world, {4.5f, 1.0f, 0.5f})
        && player.feetPosition() == beforeInvalidPosition, "teleport rejects a body inside solid blocks");

    world.set({0, 4, 0}, Block::Stone);
    check(player.setFeetPosition(world, {0.5f, 1, 0.5f}), "reset position under ceiling");
    player.move(world, {0, 20, 0});
    check(near(player.feetPosition().y, 2.2f) && !player.grounded(),
        "head collision stops upward movement below a ceiling");
    player.move(world, {0, -20, 0});
    check(near(player.feetPosition().y, 1.0f) && player.grounded(),
        "fast falls stop at the floor without tunneling");

    check(player.setFeetPosition(world, {20.5f, 1, 20.5f}), "move to open floor");
    for (int y = 1; y <= 2; ++y) {
        world.set({20, y, 24}, Block::Stone);
        world.set({20, y, 10}, Block::Stone);
    }
    player.move(world, {0, 0, 20});
    check(near(player.feetPosition().z, 23.7f), "positive z movement stops at the body edge");
    player.move(world, {0, 0, -20});
    check(near(player.feetPosition().z, 11.3f), "negative z movement stops at the body edge");
    player.move(world, {1000, 0, 1000});
    check(near(player.feetPosition().x, World::MaxExclusive.x - 0.3f) && near(player.feetPosition().z, World::MaxExclusive.z - 0.3f),
        "positive world edge keeps the full body inside");
    player.move(world, {-1000, 0, -1000});
    check(near(player.feetPosition().x, World::Min.x + 0.3f) && near(player.feetPosition().z, World::Min.z + 0.3f),
        "negative world edge keeps the full body inside");
    check(!player.setFeetPosition(world, {World::Min.x, 1, 0})
        && !player.setFeetPosition(world, {World::MaxExclusive.x, 1, 0}), "teleport rejects positions across world edges");

    const auto beforeInvalidMove = player.feetPosition();
    for (const float invalid : {std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()}) {
        player.move(world, {invalid, 0, 0});
        check(!player.setFeetPosition(world, {0, invalid, 0})
            && player.feetPosition() == beforeInvalidMove, "nonfinite movement and positions are ignored");
    }
    player.move(world, {std::numeric_limits<float>::max(), 0, 0});
    check(near(player.feetPosition().x, World::MaxExclusive.x - 0.3f), "extreme finite sweep remains bounded by the world");

    check(player.setFeetPosition(world, {20.5f, -2, 20.5f}), "feet may be below the world's editable height");
    player.move(world, {0, -20, 0});
    check(near(player.feetPosition().y, -22.0f) && !player.grounded(),
        "there is no invisible floor below the world");
    world.set({20, 0, 20}, Block::Air);
    check(player.setFeetPosition(world, {20.5f, 1, 20.5f}) && !player.grounded(),
        "removed support immediately leaves the body ungrounded");
    player.move(world, {0, -4, 0});
    check(near(player.feetPosition().y, -3.0f), "the body can fall through an opening");

    const glm::vec3 start{10.5f, 1, 30.5f};
    const glm::vec3 forward{0, 0, -1};
    player.setFeetPosition(world, start);
    for (int frame = 0; frame < 60; ++frame) {
        player.update(world, {0, 1}, forward, false, false, 1.0f / 60.0f);
    }
    check(near(player.feetPosition().z, start.z - 4.5f) && player.grounded(),
        "walking covers 4.5 blocks per second without sinking into the floor");
    player.setFeetPosition(world, start);
    for (int frame = 0; frame < 60; ++frame) {
        player.update(world, {0, 1}, {0, -100, -1}, false, true, 1.0f / 60.0f);
    }
    check(near(player.feetPosition().z, start.z - 7.0f, 0.001f)
        && near(player.feetPosition().y, start.y), "sprinting ignores the camera's pitch");
    player.setFeetPosition(world, start);
    for (int frame = 0; frame < 60; ++frame) {
        player.update(world, {1, 1}, forward, false, false, 1.0f / 60.0f);
    }
    const auto diagonal = player.feetPosition() - start;
    check(std::abs(std::hypot(diagonal.x, diagonal.z) - 4.5f) < 0.001f,
        "diagonal walking does not move faster");

    player.setFeetPosition(world, start);
    player.update(world, {}, forward, true, false, 1.0f / 60.0f);
    check(player.feetPosition().y > start.y && !player.grounded(), "jump lifts a grounded player");
    float highest = player.feetPosition().y;
    for (int frame = 0; frame < 120; ++frame) {
        player.update(world, {}, forward, true, false, 1.0f / 60.0f);
        highest = std::max(highest, player.feetPosition().y);
    }
    check(highest > start.y + 1.0f, "jump clears a one-block obstacle");
    check(near(player.feetPosition().y, start.y) && player.grounded(),
        "held jump lands without automatically jumping again");
    player.update(world, {}, forward, false, false, 1.0f / 60.0f);
    player.update(world, {}, forward, true, false, 1.0f / 60.0f);
    check(player.feetPosition().y > start.y, "jump works again after release and a new press");

    player.setFeetPosition(world, {start.x, 5, start.z});
    player.update(world, {}, forward, true, false, 0.1f);
    check(player.feetPosition().y < 5, "jump cannot create lift while airborne");
    for (int frame = 0; frame < 60; ++frame) {
        player.update(world, {}, forward, false, false, 1.0f / 60.0f);
    }
    check(near(player.feetPosition().y, start.y) && player.grounded(), "gravity lands on solid ground");

    world.set({10, 3, 30}, Block::Stone);
    player.setFeetPosition(world, start);
    player.update(world, {}, forward, true, false, 0.1f);
    check(player.feetPosition().y + Player::Height <= 3.0001f,
        "jumping into a ceiling keeps the full body below it");
    for (int frame = 0; frame < 60; ++frame) {
        player.update(world, {}, forward, false, false, 1.0f / 60.0f);
    }
    check(near(player.feetPosition().y, 1.0f), "head collision cancels upward velocity and permits landing");
    world.set({10, 3, 30}, Block::Air);

    player.setFeetPosition(world, start);
    world.set({10, 0, 30}, Block::Air);
    player.update(world, {}, forward, true, false, 0.1f);
    check(player.feetPosition().y < start.y && !player.grounded(),
        "mined support cannot still launch a grounded jump");
    world.set({10, 0, 30}, Block::Stone);

    Player clamped;
    player.setFeetPosition(world, start);
    clamped.setFeetPosition(world, start);
    player.update(world, {0, 1}, forward, false, false, 1000.0f);
    clamped.update(world, {0, 1}, forward, false, false, 0.1f);
    check(player.feetPosition() == clamped.feetPosition(), "large frame times are capped consistently");
    const auto beforeInvalidUpdate = player.feetPosition();
    for (const float invalid : {-1.0f, 0.0f, std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()}) {
        player.update(world, {1, 1}, forward, true, true, invalid);
        check(player.feetPosition() == beforeInvalidUpdate, "invalid frame time does not move the player");
    }

    // Stairs use the same movement entry point as normal physics and route tests.
    const glm::ivec3 stairStart{39, 1, 40};
    const auto stairFeet = glm::vec3(stairStart) + glm::vec3(0.5f, 0, 0.5f);
    for (const auto direction : {glm::ivec3(1,0,0), {-1,0,0}, {0,0,1}, {0,0,-1}}) {
        const auto obstacle = stairStart + direction;
        world.set(obstacle, Block::Stone);
        check(player.setFeetPosition(world, stairFeet), "stair approach is clear and supported");
        player.move(world, glm::vec3(direction));
        check(glm::distance(player.feetPosition(), stairFeet + glm::vec3(direction) + glm::vec3(0,1,0)) < 0.0001f
              && player.grounded() && !player.overlapsBlock(obstacle),
              "grounded movement climbs a one-voxel stair in every horizontal direction");
        world.set(obstacle, Block::Air);
    }
    const auto step = stairStart + glm::ivec3(1,0,0);
    world.set(step, Block::Stone);
    world.set(step + glm::ivec3(0,1,0), Block::Stone);
    player.setFeetPosition(world, stairFeet);
    player.move(world, {1,0,0});
    check(near(player.feetPosition().x, 39.7f) && near(player.feetPosition().y, 1),
          "auto-step cannot climb a two-voxel wall");
    world.set(step + glm::ivec3(0,1,0), Block::Air);
    for (const auto ceiling : {stairStart + glm::ivec3(0,2,0), step + glm::ivec3(0,2,0)}) {
        world.set(ceiling, Block::Stone);
        player.setFeetPosition(world, stairFeet);
        player.move(world, {1,0,0});
        check(near(player.feetPosition().x, 39.7f) && near(player.feetPosition().y, 1)
              && !player.overlapsBlock(ceiling), "auto-step checks headroom both before and above the stair");
        world.set(ceiling, Block::Air);
    }
    player.setFeetPosition(world, stairFeet + glm::vec3(0,0.5f,0));
    player.move(world, {1,0,0});
    check(near(player.feetPosition().x, 39.7f) && near(player.feetPosition().y, 1.5f)
          && !player.grounded(), "airborne movement cannot climb a nearby step");
    player.setFeetPosition(world, stairFeet);
    world.set(stairStart - glm::ivec3(0,1,0), Block::Air);
    player.move(world, {1,0,0});
    check(near(player.feetPosition().x, 39.7f) && near(player.feetPosition().y, 1)
          && !player.grounded(), "mined support cannot authorize a stale grounded step");
    world.set(stairStart - glm::ivec3(0,1,0), Block::Stone);
    player.setFeetPosition(world, stairFeet);
    player.move(world, {1,0.1f,0});
    check(near(player.feetPosition().x, 39.7f) && near(player.feetPosition().y, 1.1f),
          "upward jump movement does not gain extra lift from auto-step");
    world.set(step, Block::Air);

    // A diagonal input may gain one level, but must still collide with the next.
    world.set({40,1,39}, Block::Stone);
    world.set({40,1,40}, Block::Stone);
    world.set({40,2,40}, Block::Stone);
    player.setFeetPosition(world, {39.5f,1,39.5f});
    player.move(world, {1,0,1});
    check(near(player.feetPosition().x, 40.5f) && near(player.feetPosition().z, 39.7f)
          && near(player.feetPosition().y, 2), "one diagonal move cannot gain two stair levels");
    for (const auto cell : {glm::ivec3(40,1,39), {40,1,40}, {40,2,40}}) world.set(cell, Block::Air);

    for (int x = 40; x <= 45; ++x)
        for (int y = 1; y <= x - 39; ++y) world.set({x,y,40}, Block::Stone);
    player.setFeetPosition(world, stairFeet);
    for (int frame = 0; frame < 120; ++frame)
        player.update(world, {0,1}, {1,0,0}, false, false, 1.0f / 120.0f);
    check(near(player.feetPosition().x, 44.0f, 0.001f) && near(player.feetPosition().y, 6)
          && player.grounded(), "normal walking climbs consecutive stairs without jumping or losing support");
    player.update(world, {}, {1,0,0}, true, false, 1.0f / 120.0f);
    check(player.feetPosition().y > 6 && !player.grounded(), "jump still launches normally from a stair landing");
    for (int x = 40; x <= 45; ++x)
        for (int y = 1; y <= x - 39; ++y) world.set({x,y,40}, Block::Air);

    // A five-block-deep pool gives swimming room without touching the floor.
    for (int y = 1; y <= 5; ++y)
        for (int z = -30; z <= -11; ++z)
            for (int x = -30; x <= -11; ++x) world.set({x,y,z}, Block::Water);
    const glm::vec3 poolStart{-20.5f, 3.0f, -20.5f};
    check(player.setFeetPosition(world, poolStart) && player.inWater() && player.underwater()
        && !player.grounded(), "teleport immediately detects both body and eyes underwater");
    check(player.setFeetPosition(world, {-20.5f, 5.35f, -20.5f}) && player.inWater()
        && !player.underwater(), "surface swimmer has a wet body and dry eyes");
    player.move(world, {0, 1, 0});
    check(!player.inWater() && !player.underwater(), "moving clear of the surface immediately clears water state");
    player.move(world, {0, -3, 0});
    check(player.inWater() && player.underwater(), "direct movement refreshes submerged state");
    player.setFeetPosition(world, start);
    check(!player.inWater() && !player.underwater(), "dry teleport clears both water states");

    player.setFeetPosition(world, poolStart);
    for (int frame = 0; frame < 60; ++frame)
        player.update(world, {0,1}, forward, false, false, 1.0f / 60.0f);
    check(near(player.feetPosition().z, poolStart.z - 2.5f, 0.001f),
        "swimming moves 2.5 blocks per second");
    check(player.feetPosition().y < poolStart.y - 0.3f
        && player.feetPosition().y > poolStart.y - 0.6f && !player.grounded(),
        "idle swimmer sinks gently instead of falling at land gravity");
    player.setFeetPosition(world, poolStart);
    for (int frame = 0; frame < 60; ++frame)
        player.update(world, {1,1}, {0,-100,-1}, false, true, 1.0f / 60.0f);
    const auto swimDistance = player.feetPosition() - poolStart;
    check(near(std::hypot(swimDistance.x, swimDistance.z), 4.0f, 0.001f),
        "sprint swimming is four blocks per second with normalized diagonal input and no pitch boost");

    player.setFeetPosition(world, poolStart);
    for (int frame = 0; frame < 180; ++frame)
        player.update(world, {}, forward, true, false, 1.0f / 60.0f);
    check(near(player.feetPosition().y, 5.35f, 0.001f) && player.inWater() && !player.underwater(),
        "holding swim continuously rises and treads at the surface with the head above water");
    for (int frame = 0; frame < 60; ++frame)
        player.update(world, {}, forward, true, false, 1.0f / 60.0f);
    check(near(player.feetPosition().y, 5.35f, 0.001f),
        "held swim does not bounce or launch repeatedly out of open water");
    for (int frame = 0; frame < 36; ++frame)
        player.update(world, {}, forward, true, false, 1.0f / 60.0f, true);
    check(player.feetPosition().y < 4.5f && player.underwater(),
        "held descend dives below the surface and takes priority over swim up");
    for (int frame = 0; frame < 180; ++frame)
        player.update(world, {}, forward, false, false, 1.0f / 60.0f, true);
    check(near(player.feetPosition().y, 1.0f) && player.grounded() && player.inWater(),
        "continuous diving stops safely on the pool floor");
    for (int frame = 0; frame < 30; ++frame)
        player.update(world, {}, forward, true, false, 1.0f / 60.0f);
    check(player.feetPosition().y > 1.8f && !player.grounded(), "held swim lifts from the lakebed");

    world.set({-25,5,-25}, Block::Stone);
    player.setFeetPosition(world, {-24.5f, 2.0f, -24.5f});
    for (int frame = 0; frame < 120; ++frame)
        player.update(world, {}, forward, true, false, 1.0f / 60.0f);
    check(near(player.feetPosition().y, 3.2f, 0.001f) && !player.overlapsBlock({-25,5,-25}),
        "swimming into an underwater ceiling respects full body clearance");
    world.set({-25,5,-25}, Block::Water);

    for (int y = 1; y <= 9; ++y)
        for (int z = -30; z <= -11; ++z) world.set({-31,y,z}, Block::Stone);
    player.setFeetPosition(world, {-29.5f, 4.0f, -20.5f});
    for (int frame = 0; frame < 120; ++frame)
        player.update(world, {0,1}, {-1,0,0}, true, true, 1.0f / 60.0f);
    check(near(player.feetPosition().x, -29.7f, 0.001f) && near(player.feetPosition().y, 5.35f, 0.001f),
        "swim up against a tall wall cannot tunnel through or climb it");

    // A bank one voxel above the water is reachable only with a clear landing.
    for (int y = 1; y <= 6; ++y)
        for (int z = -30; z <= -11; ++z) world.set({-10,y,z}, Block::Stone);
    player.setFeetPosition(world, {-11.5f, 5.35f, -20.5f});
    for (int frame = 0; frame < 60; ++frame)
        player.update(world, {0,1}, {1,0,0}, false, false, 1.0f / 60.0f);
    check(near(player.feetPosition().x, -10.3f, 0.001f) && player.feetPosition().y < 5.35f,
        "swimming against a bank without holding up does not climb it");
    player.setFeetPosition(world, {-11.5f, 5.35f, -20.5f});
    for (int frame = 0; frame < 36; ++frame)
        player.update(world, {0,1}, {1,0,0}, true, false, 1.0f / 60.0f);
    check(player.feetPosition().x > -10.3f && near(player.feetPosition().y, 7.0f)
        && player.grounded() && !player.inWater(),
        "Space and forward climb a one-block-high shoreline with real support");
    player.setFeetPosition(world, {-11.5f, 5.35f, -20.5f});
    world.set({-10,8,-21}, Block::Stone);
    for (int frame = 0; frame < 90; ++frame)
        player.update(world, {0,1}, {1,0,0}, true, false, 1.0f / 60.0f);
    check(near(player.feetPosition().x, -10.3f, 0.001f) && near(player.feetPosition().y, 5.35f, 0.001f)
        && !player.overlapsBlock({-10,8,-21}), "low headroom prevents a shoreline exit");
    world.set({-10,8,-21}, Block::Air);
    for (int z = -30; z <= -11; ++z) world.set({-10,6,z}, Block::Air);
    player.setFeetPosition(world, {-11.5f, 5.35f, -20.5f});
    for (int frame = 0; frame < 36; ++frame)
        player.update(world, {0,1}, {1,0,0}, true, false, 1.0f / 60.0f);
    check(player.feetPosition().x > -10.3f && near(player.feetPosition().y, 6.0f)
        && player.grounded() && !player.inWater(),
        "swimming can also exit onto a bank exactly level with the water surface");

    player.setFeetPosition(world, {-29.5f, 5.35f, -15.5f});
    for (int frame = 0; frame < 30; ++frame)
        player.update(world, {0,1}, {0,0,1}, true, false, 1.0f / 60.0f);
    check(near(player.feetPosition().y, 5.35f, 0.001f) && !player.grounded(),
        "water movement without an obstructing supported bank never creates an air stair");

    Player slowFrames, fastFrames;
    slowFrames.setFeetPosition(world, poolStart);
    fastFrames.setFeetPosition(world, poolStart);
    for (int frame = 0; frame < 10; ++frame)
        slowFrames.update(world, {0,1}, forward, false, false, 0.1f, true);
    for (int frame = 0; frame < 120; ++frame)
        fastFrames.update(world, {0,1}, forward, false, false, 1.0f / 120.0f, true);
    check(glm::distance(slowFrames.feetPosition(), fastFrames.feetPosition()) < 0.001f,
        "swimming drag and movement agree at ten and 120 frames per second");
    slowFrames.setFeetPosition(world, poolStart);
    fastFrames.setFeetPosition(world, poolStart);
    slowFrames.update(world, {1,1}, forward, true, true, 1000.0f);
    fastFrames.update(world, {1,1}, forward, true, true, 0.1f);
    check(slowFrames.feetPosition() == fastFrames.feetPosition(), "water physics caps extreme frame times");

    player.setFeetPosition(world, {-20.5f, 12.0f, -20.5f});
    for (int frame = 0; frame < 54; ++frame)
        player.update(world, {}, forward, false, false, 1.0f / 60.0f);
    check(player.inWater() && player.feetPosition().y > 2.5f && !player.grounded(),
        "falling into the pool loses speed before striking the lakebed");
    // Water changes around a stationary player are sampled on the next update.
    player.setFeetPosition(world, {-25.5f, 2.0f, -25.5f});
    for (int y = 1; y <= 5; ++y) world.set({-26,y,-26}, Block::Air);
    player.update(world, {}, forward, false, false, 1.0f / 120.0f);
    check(!player.inWater() && !player.underwater(), "draining water refreshes contact without teleporting");
    player.setFeetPosition(world, start);
    player.update(world, {}, forward, true, false, 1.0f / 60.0f, true);
    check(player.feetPosition().y > start.y, "descend has no effect on ordinary dry jumps");

    World terrain(42);
    check(player.spawn(terrain), "generated terrain has a safe spawn");
    check(near(player.feetPosition().x, 0.5f) && near(player.feetPosition().z, 0.5f)
        && near(player.feetPosition().y, voxel::terrainHeight(0, 0, 42) + 1.0f)
        && player.grounded(), "default spawn stands on the preferred column's surface");
    check(player.spawn(terrain, {-17, -1}) && near(player.feetPosition().x, -16.5f)
        && near(player.feetPosition().z, -0.5f)
        && near(player.feetPosition().y, voxel::terrainHeight(-17, -1, 42) + 1.0f),
        "spawn works across negative chunk boundaries");

    check(player.spawn(world, {20, 20}), "missing support searches neighboring columns");
    check(near(player.feetPosition().x, 20.5f) && near(player.feetPosition().z, 19.5f),
        "equal-distance spawn choices use deterministic z-then-x order");
    const auto nearbySpawn = player.feetPosition();
    player.spawn(world, {20, 20});
    check(player.feetPosition() == nearbySpawn, "repeated spawn chooses the same safe location");
    world.set({20, World::MaxExclusive.y - 1, 20}, Block::Stone);
    check(player.spawn(world, {20, 20}) && near(player.feetPosition().y, World::MaxExclusive.y)
        && player.grounded(), "spawn can stand above a player-built column at maximum world height");
    check(!player.overlapsBlock({20, World::MaxExclusive.y - 1, 20}), "spawned body touches support without intersecting it");

    player.spawn(world, {10, 30});
    player.update(world, {}, forward, true, false, 0.1f);
    player.spawn(world, {10, 30});
    player.update(world, {}, forward, false, false, 0.1f);
    check(near(player.feetPosition().y, 1.0f) && player.grounded(),
        "respawning clears airborne velocity");
    player.update(world, {}, forward, true, false, 0.1f);
    check(player.feetPosition().y > 1.0f, "respawning permits a fresh jump");

    World empty;
    for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
                empty.set({x, y, z}, Block::Air);
            }
        }
    }
    const auto beforeFailedSpawn = player.feetPosition();
    const bool previouslyGrounded = player.grounded();
    check(!player.spawn(empty) && player.feetPosition() == beforeFailedSpawn
        && player.grounded() == previouslyGrounded, "empty world cannot spawn or alter the player");
    empty.set({-32, 5, -32}, Block::Stone);
    check(player.spawn(empty, {std::numeric_limits<int>::max(), std::numeric_limits<int>::min()})
        && player.feetPosition() == glm::vec3(-31.5f, 6.0f, -31.5f) && player.grounded(),
        "extreme preferred coordinates safely find the only supported spawn");

    if (failures == 0) std::cout << "Player tests passed\n";
    return failures == 0 ? 0 : 1;
}
