#include "player.h"
#include "world.h"

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

bool near(float first, float second)
{
    return std::abs(first - second) < 0.0001f;
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

    if (failures == 0) std::cout << "Player tests passed\n";
    return failures == 0 ? 0 : 1;
}
