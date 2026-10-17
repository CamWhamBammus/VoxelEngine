#include "player.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

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
    check(near(player.feetPosition().x, 47.7f) && near(player.feetPosition().z, 47.7f),
        "positive world edge keeps the full body inside");
    player.move(world, {-1000, 0, -1000});
    check(near(player.feetPosition().x, -31.7f) && near(player.feetPosition().z, -31.7f),
        "negative world edge keeps the full body inside");
    check(!player.setFeetPosition(world, {-32, 1, 0})
        && !player.setFeetPosition(world, {48, 1, 0}), "teleport rejects positions across world edges");

    const auto beforeInvalidMove = player.feetPosition();
    for (const float invalid : {std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()}) {
        player.move(world, {invalid, 0, 0});
        check(!player.setFeetPosition(world, {0, invalid, 0})
            && player.feetPosition() == beforeInvalidMove, "nonfinite movement and positions are ignored");
    }
    player.move(world, {std::numeric_limits<float>::max(), 0, 0});
    check(near(player.feetPosition().x, 47.7f), "extreme finite sweep remains bounded by the world");

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
    world.set({20, 15, 20}, Block::Stone);
    check(player.spawn(world, {20, 20}) && near(player.feetPosition().y, 16.0f)
        && player.grounded(), "spawn can stand above a player-built column at maximum world height");
    check(!player.overlapsBlock({20, 15, 20}), "spawned body touches support without intersecting it");

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
