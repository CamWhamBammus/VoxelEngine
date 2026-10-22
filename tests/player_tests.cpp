#include "player.h"
#include "asset_collision.h"
#include "hub_assets.h"
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

    // Full cubes now require jumping; actual stairs expose two half-height treads.
    const glm::ivec3 stairStart{39, 1, 40};
    const auto stairFeet = glm::vec3(stairStart) + glm::vec3(0.5f, 0, 0.5f);
    for (int facing = 0; facing < 4; ++facing) {
        const auto stair = voxel::stairBlock(Block::Stone, facing);
        const auto direction = voxel::stairUpDirection(stair);
        const auto obstacle = stairStart + direction;
        world.set(obstacle, Block::Stone);
        check(player.setFeetPosition(world, stairFeet), "block approach is clear and supported");
        player.move(world, glm::vec3(direction));
        check(glm::distance(player.feetPosition(), stairFeet + glm::vec3(direction) * .2f) < .0001f
              && player.grounded(), "walking into a full block does not automatically climb it in any direction");
        world.set(obstacle, stair);
        player.setFeetPosition(world, stairFeet);
        player.move(world, glm::vec3(direction));
        check(glm::distance(player.feetPosition(), stairFeet + glm::vec3(direction) + glm::vec3(0,1,0)) < .0001f
              && player.grounded(), "one-meter movement crosses both half treads in every stair rotation");
        player.move(world, -glm::vec3(direction));
        check(glm::distance(player.feetPosition(), stairFeet) < .0001f && player.grounded(),
              "descending both stair treads returns smoothly to the floor");
        player.setFeetPosition(world, stairFeet + glm::vec3(direction) * 2.f);
        player.move(world, -glm::vec3(direction));
        check(glm::distance(player.feetPosition(), stairFeet + glm::vec3(direction) * 1.8f) < .0001f,
              "the full-height back of a stair cannot be climbed without jumping");
        world.set(obstacle, Block::Air);
    }
    const auto step = stairStart + glm::ivec3(1,0,0);
    const auto eastStair = voxel::stairBlock(Block::Stone, 3);
    world.set(step, Block::Stone);
    for (int x = 41; x <= 44; ++x) world.set({x,1,40}, Block::Stone);
    player.setFeetPosition(world, stairFeet);
    for (int frame = 0; frame < 60; ++frame)
        player.update(world, {0,1}, {1,0,0}, false, false, 1.f/60);
    check(near(player.feetPosition().x,39.7f) && near(player.feetPosition().y,1),
          "continued walking against a full-block ledge cannot climb it over multiple frames");
    for (int frame = 0; frame < 48; ++frame)
        player.update(world, {0,1}, {1,0,0}, true, false, 1.f/60);
    check(player.feetPosition().x > 42 && near(player.feetPosition().y,2) && player.grounded(),
          "a deliberate jump still clears a full block and lands on its top");
    for (int x = 41; x <= 44; ++x) world.set({x,1,40}, Block::Air);

    world.set(step, eastStair);
    for (const auto ceiling : {stairStart + glm::ivec3(0,2,0), step + glm::ivec3(0,2,0)}) {
        world.set(ceiling, Block::Stone);
        player.setFeetPosition(world, stairFeet);
        player.move(world, {1,0,0});
        check(near(player.feetPosition().x, 39.7f) && near(player.feetPosition().y, 1)
              && !player.overlapsBlock(ceiling), "half-step checks headroom before and above the stair");
        world.set(ceiling, Block::Air);
    }
    player.setFeetPosition(world, stairFeet + glm::vec3(0,.25f,0));
    player.move(world, {1,0,0});
    check(near(player.feetPosition().x,39.7f) && near(player.feetPosition().y,1.25f)
          && !player.grounded(), "airborne movement cannot climb a nearby half-step");
    player.setFeetPosition(world, stairFeet);
    world.set(stairStart - glm::ivec3(0,1,0), Block::Air);
    player.move(world, {1,0,0});
    check(near(player.feetPosition().x,39.7f) && near(player.feetPosition().y,1)
          && !player.grounded(), "mined support cannot authorize a stale grounded half-step");
    world.set(stairStart - glm::ivec3(0,1,0), Block::Stone);
    player.setFeetPosition(world, stairFeet);
    player.move(world, {1,.1f,0});
    check(near(player.feetPosition().x,39.7f) && near(player.feetPosition().y,1.1f),
          "upward jump movement does not gain extra lift from auto-step");
    check(player.setFeetPosition(world,{40.15f,1.5f,40.5f}) && player.grounded(),
          "the open space over a lower tread is empty and supports a fractional landing");
    check(!player.setFeetPosition(world,{40.85f,1.5f,40.5f}),
          "the upper half of the same stair remains solid");
    world.set(step,Block::Air);

    // Diagonal movement must not turn a full-block corner into a staircase.
    for (const auto cell : {glm::ivec3(40,1,39), {40,1,40}, {40,2,40}, {39,1,40}})
        world.set(cell,Block::Stone);
    player.setFeetPosition(world,{39.5f,1,39.5f});
    player.move(world,{1,0,1});
    check(near(player.feetPosition().x,39.7f) && near(player.feetPosition().z,39.7f)
          && near(player.feetPosition().y,1), "diagonal input cannot climb full cubes at an inside corner");
    for (const auto cell : {glm::ivec3(40,1,39), {40,1,40}, {40,2,40}, {39,1,40}})
        world.set(cell,Block::Air);

    for (int facing = 0; facing < 4; ++facing) {
        const auto stair = voxel::stairBlock(Block::Planks, facing);
        const auto direction = voxel::stairUpDirection(stair);
        for (int index = 0; index < 6; ++index) {
            const auto cell = stairStart + direction * (index + 1);
            for (int y = 1; y <= index; ++y) world.set({cell.x,y,cell.z},Block::Stone);
            world.set({cell.x,index+1,cell.z},stair);
        }
        for (const int fps : {30,60,120}) {
            player.setFeetPosition(world,stairFeet);
            for (int frame = 0; frame < fps; ++frame)
                player.update(world,{0,1},glm::vec3(direction),false,false,1.f/fps);
            check(glm::distance(player.feetPosition(),stairFeet + glm::vec3(direction)*4.5f + glm::vec3(0,4.5f,0)) < .001f
                  && player.grounded(), "consecutive stairs climb consistently in all rotations at 30, 60 and 120 FPS");
            for (int frame = 0; frame < fps; ++frame)
                player.update(world,{0,1},-glm::vec3(direction),false,false,1.f/fps);
            check(glm::distance(player.feetPosition(),stairFeet) < .001f && player.grounded(),
                  "stair descent follows each tread in all rotations at 30, 60 and 120 FPS");
            check(near(player.consumeFallDistance(), 0),
                  "following successive stair treads never accumulates a fall");
        }
        player.setFeetPosition(world,stairFeet + glm::vec3(direction)*3.f + glm::vec3(0,3,0));
        player.update(world,{},glm::vec3(direction),true,false,1.f/120);
        check(player.feetPosition().y > 4 && !player.grounded(), "jump launches normally from a stair landing");
        for (int index = 0; index < 6; ++index) {
            const auto cell = stairStart + direction * (index + 1);
            for (int y = 1; y <= index+1; ++y) world.set({cell.x,y,cell.z},Block::Air);
        }
    }

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

    // Authored models have their actual fractional surfaces and openings.
    using voxel::HubAsset;
    using voxel::HubAssetKind;
    const glm::vec3 assetBase{0, 1, -45};
    const auto setAssets = [&](std::initializer_list<HubAsset> assets) {
        auto collision = std::make_shared<voxel::AssetCollision>();
        collision->rebuild(std::span<const HubAsset>(assets.begin(), assets.size()));
        player.setAssetCollision(collision);
        return collision;
    };
    setAssets({{HubAssetKind::CrateStack, assetBase, 0, 1}});
    check(player.setFeetPosition(world, assetBase + glm::vec3(0, 8, 0)),
        "player can start above a real crate model");
    player.move(world, {0,-50,0});
    check(near(player.feetPosition().y, 1.82f) && player.grounded(),
        "fast falls land exactly on a fractional crate top");
    const auto onCrate = player.feetPosition();
    check(!player.setFeetPosition(world, assetBase + glm::vec3(0,.1f,0))
        && player.feetPosition() == onCrate, "teleport rejects a body intersecting a model");
    Player copiedPlayer = player;
    copiedPlayer.move(world, {0,-10,0});
    check(copiedPlayer.grounded() && near(copiedPlayer.feetPosition().y, 1.82f),
        "copied player safely retains its shared scene collider");
    player.update(world, {}, forward, true, false, 1.0f/60.0f);
    check(player.feetPosition().y > onCrate.y && !player.grounded(), "jump launches from a model top");
    for (int frame=0; frame<90; ++frame)
        player.update(world, {}, forward, false, false, 1.0f/60.0f);
    check(near(player.feetPosition().y, onCrate.y) && player.grounded(),
        "jump returns to the same fractional model surface");
    player.move(world, {3,0,0});
    check(!player.grounded(), "walking off a model does not retain phantom support");
    player.update(world, {}, forward, false, false, .1f);
    check(player.feetPosition().y < onCrate.y, "gravity starts after stepping off a model");

    player.setFeetPosition(world, assetBase + glm::vec3(-2,0,0));
    player.move(world, {2,0,0});
    check(near(player.feetPosition().y,1) && player.grounded() && player.feetPosition().x < -.5f,
        "a crate taller than a half-block step now requires a jump");
    setAssets({{HubAssetKind::CrateStack, assetBase, 0, 0}});
    player.setFeetPosition(world, assetBase + glm::vec3(-3,.2f,0));
    player.move(world, {20,0,1});
    check(player.feetPosition().x < -.8f && player.feetPosition().z > assetBase.z + .9f,
        "fast airborne movement stops at a model side and slides along it");
    setAssets({{HubAssetKind::CrateStack, assetBase + glm::vec3(0,3,0), 0, 1}});
    player.setFeetPosition(world, assetBase);
    player.move(world, {0,20,0});
    check(near(player.feetPosition().y, 2.2f), "upward sweep stops below an asset ceiling");

    setAssets({{HubAssetKind::PathPaving, assetBase + glm::vec3(1,0,0), 0, 0}});
    world.set({0,3,-46}, Block::Stone);
    world.set({0,3,-45}, Block::Stone);
    world.set({1,3,-46}, Block::Stone);
    world.set({1,3,-45}, Block::Stone);
    player.setFeetPosition(world, assetBase);
    player.move(world, {1,0,0});
    check(near(player.feetPosition().x, 1) && player.feetPosition().y > 1.01f
        && player.feetPosition().y < 1.05f && player.grounded(),
        "thin paving needs only its own headroom instead of a whole block");
    for (const auto cell : {glm::ivec3(0,3,-46), {0,3,-45}, {1,3,-46}, {1,3,-45}})
        world.set(cell, Block::Air);

    setAssets({{HubAssetKind::PathPaving, {12.f,2,-44.5f}, 0, 7}});
    world.set({11,1,-45},voxel::stairBlock(Block::Stone,3));
    player.setFeetPosition(world, {10.5f,1,-44.5f});
    player.move(world, {1,0,0});
    check(near(player.feetPosition().x,11.5f) && player.feetPosition().y > 2.01f
        && player.feetPosition().y < 2.05f && player.grounded(),
        "two half-treads remain climbable with thin model paving fitted over the upper tread");
    world.set({11,1,-45}, Block::Air);

    setAssets({{HubAssetKind::GiantMushroom, assetBase, 0, 7}});
    player.setFeetPosition(world, assetBase + glm::vec3(0,8,0));
    player.move(world, {0,-20,0});
    check(player.feetPosition().y > 4.0f && player.feetPosition().y < 4.3f && player.grounded(),
        "giant mushroom cap is a usable parkour landing");
    bool followedCap = true;
    for (int stepIndex=0; stepIndex<12; ++stepIndex) {
        player.move(world, {.05f,0,0});
        followedCap &= player.grounded();
    }
    check(followedCap && near(player.feetPosition().x, .6f, .001f),
        "walking over mushroom facets keeps moving and follows the cap surface");

    for (int facing=0; facing<4; ++facing) {
        setAssets({{HubAssetKind::MossyLog, assetBase, facing, 2}});
        const glm::vec3 along = facing % 2 == 0 ? glm::vec3(1,0,0) : glm::vec3(0,0,1);
        const glm::vec3 cross = facing % 2 == 0 ? glm::vec3(0,0,1) : glm::vec3(1,0,0);
        player.setFeetPosition(world, assetBase + along + glm::vec3(0,6,0));
        player.move(world, {0,-10,0});
        check(player.feetPosition().y > 1.68f && player.feetPosition().y < 2.05f && player.grounded(),
            "all quarter-turn log placements support the visible long axis");
        player.setFeetPosition(world, assetBase + cross + glm::vec3(0,6,0));
        player.move(world, {0,-10,0});
        check(near(player.feetPosition().y, 1), "rotated log has no invisible box beyond its narrow side");
    }
    setAssets({{HubAssetKind::RockLedge, assetBase, 0, 5}});
    player.setFeetPosition(world, assetBase + glm::vec3(-.5f,6,.6f));
    player.move(world, {0,-10,0});
    const float ledgeTop = player.feetPosition().y;
    bool followedLedge = player.grounded();
    for (int stepIndex=0; stepIndex<20; ++stepIndex) {
        player.move(world, {.05f,0,0});
        followedLedge &= player.grounded();
    }
    check(followedLedge && ledgeTop > 1.1f && ledgeTop < 1.3f
        && near(player.feetPosition().x, .5f, .001f),
        "sloping rock facets support continuous walking across triangle seams");

    for (int facing=0; facing<4; ++facing) {
        setAssets({{HubAssetKind::RootArch, assetBase, facing, 5}});
        const glm::vec3 through = facing % 2 == 0 ? glm::vec3(0,0,1) : glm::vec3(1,0,0);
        const glm::vec3 across = facing % 2 == 0 ? glm::vec3(1,0,0) : glm::vec3(0,0,1);
        check(player.setFeetPosition(world, assetBase - through*2.0f), "arch approach has body clearance");
        player.move(world, through*4.0f);
        check(glm::distance(player.feetPosition(), assetBase + through*2.0f) < .001f,
            "rotated arch opening stays traversable instead of using one solid bounding box");
        player.setFeetPosition(world, assetBase + across*3.3f - through*2.0f + glm::vec3(0,.1f,0));
        player.move(world, through*4.0f);
        check(glm::dot(player.feetPosition() - assetBase, through) < 0,
            "the same arch blocks an airborne sweep through its solid side");
    }

    setAssets({{HubAssetKind::RockLedge, {-20,6,-18}, 0, 5}});
    player.setFeetPosition(world, {-20,5.35f,-15.5f});
    for (int frame=0; frame<36; ++frame)
        player.update(world, {0,1}, {0,0,-1}, true, false, 1.0f/60.0f);
    check(player.feetPosition().z < -16.6f && player.feetPosition().y > 6.0f
        && player.grounded() && !player.inWater(),
        "swim up and forward climbs onto a fractional asset landing at the shore");
    setAssets({{HubAssetKind::PathPaving, {0.5f,1,-44.5f}, 0, 7}});
    check(player.spawn(world, {0,-45}) && near(player.feetPosition().x,.5f)
        && near(player.feetPosition().z,-44.5f) && player.feetPosition().y > 1.01f
        && player.feetPosition().y < 1.05f && player.grounded(),
        "spawning settles onto fitted paving without moving to another voxel column");
    player.setAssetCollision(nullptr);
    check(player.setFeetPosition(world, assetBase), "clearing scene collision restores voxel-only movement");

    // Landing events belong to the physics sweep, not the health system. A
    // consumer receives one distance after contact, even on a slow frame.
    const auto settle = [&](Player& body, float seconds, float frameTime, bool descend = false) {
        for (int frame = 0; frame < int(std::ceil(seconds / frameTime)); ++frame)
            body.update(world, {}, forward, false, false, frameTime, descend);
    };
    player.setFeetPosition(world, start);
    player.update(world, {}, forward, true, false, 1.f / 60);
    settle(player, 1, 1.f / 60);
    const float hopFall = player.consumeFallDistance();
    check(player.grounded() && hopFall > 1.3f && hopFall < 1.35f,
          "ordinary hop reports its actual apex height below the damaging fall threshold");
    check(near(player.consumeFallDistance(), 0), "a landing is consumed exactly once");

    for (const float frameTime : {.1f, 1.f / 30, 1.f / 60, 1.f / 144}) {
        player.setFeetPosition(world, start + glm::vec3(0, 8, 0));
        settle(player, 2, frameTime);
        check(player.grounded() && near(player.consumeFallDistance(), 8, .001f),
              "high drop records the same distance at 10, 30, 60 and 144 FPS");
        settle(player, .2f, frameTime);
        check(near(player.consumeFallDistance(), 0), "remaining grounded cannot repeat a fall event");
    }

    player.setFeetPosition(world, start + glm::vec3(0, 8, 0));
    settle(player, 2, 1.f / 60);
    player.update(world, {}, forward, true, false, 1.f / 60);
    settle(player, 1, 1.f / 60);
    check(near(player.consumeFallDistance(), 8),
          "multiple landings retain the largest distance until the consumer reads it");

    player.setFeetPosition(world, start + glm::vec3(0, 8, 0));
    settle(player, .2f, 1.f / 60);
    player.setFeetPosition(world, start + glm::vec3(0, 2, 0));
    settle(player, 1, 1.f / 60);
    check(near(player.consumeFallDistance(), 2), "teleport resets the previous airborne apex");
    player.setFeetPosition(world, start + glm::vec3(0, 8, 0));
    settle(player, 2, 1.f / 60);
    player.setFeetPosition(world, start);
    check(near(player.consumeFallDistance(), 0), "teleport clears an unconsumed landing");
    player.setFeetPosition(world, start + glm::vec3(0, 8, 0));
    settle(player, 2, 1.f / 60);
    player.setAssetCollision(nullptr);
    check(near(player.consumeFallDistance(), 0), "changing the scene collider clears an unconsumed landing");

    player.setFeetPosition(world, start + glm::vec3(0, 8, 0));
    settle(player, .2f, 1.f / 60);
    const auto airbornePosition = player.feetPosition();
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    player.update(world, {}, forward, false, false, invalid);
    player.update(world, {invalid, 0}, forward, false, false, .1f);
    player.update(world, {}, {invalid, 0, 0}, false, false, .1f);
    check(player.feetPosition() == airbornePosition && near(player.consumeFallDistance(), 0),
          "invalid input neither moves an airborne player nor fabricates a landing");
    check(!player.setFeetPosition(world, {10.5f, .5f, 30.5f}), "invalid teleport during a fall is rejected");
    settle(player, 2, 1.f / 60);
    check(near(player.consumeFallDistance(), 8),
          "invalid input and rejected teleports do not corrupt an active fall");

    player.setFeetPosition(world, {-20.5f, 20, -20.5f});
    settle(player, 5, 1.f / 60, true);
    check(player.grounded() && player.inWater() && near(player.consumeFallDistance(), 0),
          "water cushions a high drop all the way to the pool floor");

    setAssets({{HubAssetKind::CrateStack, assetBase, 0, 1}});
    player.setFeetPosition(world, assetBase + glm::vec3(0, 8, 0));
    settle(player, 2, 1.f / 60);
    check(player.grounded() && near(player.feetPosition().y, 1.82f)
          && near(player.consumeFallDistance(), 7.18f, .001f),
          "landing on a model reports the drop to its actual fractional top");
    player.setAssetCollision(nullptr);

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
