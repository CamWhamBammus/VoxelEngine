#include "challenge.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            ++failures;
        }
    };
    using voxel::ChallengeState;
    using voxel::Interaction;
    voxel::World world(42);
    world.takeDirtyChunks();
    const voxel::World unchanged = world;
    const glm::vec3 camp(8.5f, voxel::terrainHeight(8, 8, world.seed()) + 2.0f, 8.5f);
    voxel::Challenge challenge;
    challenge.reset(world, camp);
    check(challenge.state() == ChallengeState::Playing && challenge.collectedCount() == 0,
          "a fresh trail must start with three uncollected crystals");
    check(challenge.camp() == camp, "trail must retain the camp marker center");
    voxel::Challenge repeated;
    repeated.reset(world, camp);
    for (std::size_t index = 0; index < challenge.crystals().size(); ++index) {
        const auto& crystal = challenge.crystals()[index];
        check(crystal.position == repeated.crystals()[index].position && !crystal.collected,
              "the same world must produce the same crystal positions");
        const int x = static_cast<int>(std::floor(crystal.position.x));
        const int z = static_cast<int>(std::floor(crystal.position.z));
        const int surface = voxel::terrainHeight(x, z, world.seed());
        check(voxel::World::contains({x, surface, z})
              && std::abs(crystal.position.y - (surface + 1.8f)) < 0.0001f,
              "each crystal must float above a valid world surface");
        for (std::size_t previous = 0; previous < index; ++previous) {
            check(crystal.position != challenge.crystals()[previous].position,
                  "each objective must occupy a distinct location");
        }
    }
    check(challenge.interact(camp) == Interaction::NeedCrystals
          && challenge.state() == ChallengeState::Playing,
          "camp must require every crystal before awarding victory");
    const auto first = challenge.crystals()[0].position;
    check(challenge.interact(first + glm::vec3(3.01f, 0, 0)) == Interaction::None,
          "crystals outside interaction range must not be collected");
    check(challenge.interact(first + glm::vec3(3, 0, 0)) == Interaction::Collected
          && challenge.collectedCount() == 1, "exactly three blocks must be within interaction reach");
    check(challenge.interact(first) == Interaction::None && challenge.collectedCount() == 1,
          "repeated interactions cannot collect the same crystal twice");
    check(challenge.interact({std::numeric_limits<float>::quiet_NaN(), 0, 0}) == Interaction::None
          && challenge.interact({0, std::numeric_limits<float>::infinity(), 0}) == Interaction::None,
          "nonfinite eye positions must not collect objectives");
    check(challenge.interact(challenge.crystals()[1].position) == Interaction::Collected
          && challenge.interact(challenge.crystals()[2].position) == Interaction::Collected,
          "all remaining crystals must be collectible");
    check(challenge.collectedCount() == 3 && challenge.state() == ChallengeState::Playing,
          "collecting all crystals must still require a return to camp");
    check(challenge.interact(camp) == Interaction::Won && challenge.state() == ChallengeState::Won,
          "returning all crystals to camp must win the trail");
    check(challenge.interact(camp) == Interaction::Finished,
          "a finished trail must not award victory repeatedly");
    check(world.sameContents(unchanged) && world.takeDirtyChunks().empty(),
          "challenge progress must not change terrain or mesh state");

    challenge.reset(world, camp);
    check(challenge.state() == ChallengeState::Playing && challenge.collectedCount() == 0,
          "restart must restore every objective and playing state");
    challenge.reset(world, first);
    check(challenge.interact(first) == Interaction::Collected,
          "crystal pickup must take priority over an overlapping camp");

    // Surface sampling follows edits rather than assuming the seed's height.
    world.set({-20, 15, -18}, voxel::Block::Stone);
    challenge.reset(world, camp);
    check(std::abs(challenge.crystals()[0].position.y - 16.8f) < 0.0001f,
          "crystals must sit above edited surface heights");
    const auto preserved = challenge.crystals()[0].position;
    for (int y = voxel::World::Min.y; y < voxel::World::MaxExclusive.y; ++y) {
        world.set({-20, y, -18}, voxel::Block::Air);
    }
    bool rejected = false;
    try {
        challenge.reset(world, camp);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && challenge.crystals()[0].position == preserved,
          "missing objective terrain must reject reset without partially changing the trail");
    rejected = false;
    try {
        challenge.reset(unchanged, {0, std::numeric_limits<float>::infinity(), 0});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "nonfinite camp positions must be rejected");
    return failures == 0 ? 0 : 1;
}
