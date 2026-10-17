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

    challenge.reset(unchanged, camp);
    check(challenge.timeRemaining() == 300.0f, "a fresh trail must allow five minutes");
    challenge.update(7.5f);
    check(challenge.timeRemaining() == 292.5f, "timer must consume the full positive elapsed duration");
    challenge.update(90.0f, true);
    check(challenge.timeRemaining() == 292.5f, "pausing must freeze the countdown");
    for (const float invalid : {0.0f, -5.0f, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        challenge.update(invalid);
    }
    check(challenge.timeRemaining() == 292.5f && challenge.state() == ChallengeState::Playing,
          "invalid elapsed time must not extend or corrupt the countdown");
    challenge.penalizeFall();
    check(challenge.timeRemaining() == 272.5f && challenge.state() == ChallengeState::Playing,
          "falling must cost twenty seconds");
    challenge.update(271.5f);
    check(challenge.timeRemaining() == 1.0f && challenge.state() == ChallengeState::Playing,
          "one remaining second must keep the trail active");
    check(challenge.interact(challenge.crystals()[0].position) == Interaction::Collected,
          "objectives remain collectible before the timer expires");
    challenge.update(1.0f);
    check(challenge.timeRemaining() == 0.0f && challenge.state() == ChallengeState::Lost,
          "reaching zero must end the trail in a loss");
    check(challenge.interact(challenge.crystals()[1].position) == Interaction::Finished
          && challenge.collectedCount() == 1, "a lost trail must not accept more pickups");
    challenge.update(100.0f);
    challenge.penalizeFall();
    check(challenge.timeRemaining() == 0.0f && challenge.state() == ChallengeState::Lost,
          "a lost trail must stay finished without negative time");

    challenge.reset(unchanged, camp);
    check(challenge.timeRemaining() == 300.0f && challenge.collectedCount() == 0
          && challenge.state() == ChallengeState::Playing,
          "restarting a lost trail must restore time and objectives together");
    challenge.update(290.0f);
    challenge.penalizeFall();
    check(challenge.timeRemaining() == 0.0f && challenge.state() == ChallengeState::Lost,
          "a fall penalty larger than the remaining time must cause a loss");
    challenge.reset(unchanged, camp);
    challenge.update(std::numeric_limits<float>::max());
    check(challenge.timeRemaining() == 0.0f && challenge.state() == ChallengeState::Lost,
          "large finite elapsed time must expire the trail without overflow");

    challenge.reset(unchanged, camp);
    challenge.update(17.0f);
    for (const auto& crystal : challenge.crystals()) {
        check(challenge.interact(crystal.position) == Interaction::Collected,
              "a timed trail must allow collecting every crystal");
    }
    check(challenge.interact(camp) == Interaction::Won, "returning in time must win");
    challenge.update(500.0f);
    challenge.penalizeFall();
    check(challenge.state() == ChallengeState::Won && challenge.timeRemaining() == 283.0f,
          "victory must freeze the finishing time and ignore further fall penalties");
    challenge.reset(unchanged, camp);
    check(challenge.state() == ChallengeState::Playing && challenge.timeRemaining() == 300.0f
          && challenge.collectedCount() == 0, "restarting after victory must restore the challenge");
    return failures == 0 ? 0 : 1;
}
