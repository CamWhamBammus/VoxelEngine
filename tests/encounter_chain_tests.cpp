#include "encounters.h"

#include <algorithm>
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

HubAsset spawner(glm::vec3 position, std::uint32_t id, std::uint32_t next = 0)
{
    HubAsset asset{HubAssetKind::TrialSpawner, position};
    asset.spawner = {{1, 0, 0}, id, next};
    return asset;
}

auto chain()
{
    // Array order and unrelated scenery must not determine encounter order.
    return std::array{spawner({.5f, 9, .5f}, 20, 30),
                      HubAsset{HubAssetKind::Fern, {25, 9, 25}},
                      spawner({18.5f, 9, .5f}, 30),
                      spawner({-18.5f, 9, .5f}, 10, 20)};
}

void position(Player& player, const World& world, glm::vec3 feet)
{
    require(player.setFeetPosition(world, feet), "fixture player position must fit");
}

void tick(EncounterSession& session, const World& world, const Player& player, int frames = 1)
{
    for (int i = 0; i < frames; ++i) session.update(world, player, {0, 0, -1}, false, .1f);
}

void defeat(EncounterSession& session, const World& world, Player& player)
{
    for (int i = 0; i < 80 && session.pendingCompletions().empty(); ++i) {
        if (session.monsters().empty()) tick(session, world, player);
        else {
            const auto target = session.monsters().front().position;
            position(player, world, target + glm::vec3(0, 0, 2.1f));
            const auto aim = glm::normalize(target + glm::vec3(0, .8f, 0) - player.eyePosition());
            session.update(world, player, aim, true, .1f);
        }
    }
    require(session.monsters().empty() && session.pendingCompletions().size() == 1,
            "the real sword fight finishes one encounter and leaves its clear pending");
}

void orderAndDurability(const World& world)
{
    const auto assets = chain();
    EncounterSession session;
    Player player;
    session.begin(assets, {});
    position(player, world, {.5f, 9, 6.5f});
    tick(session, world, player, 20);
    require(!session.active() && session.monsters().empty() && session.activationSerial() == 0,
            "standing beside a follower does not bypass its predecessor");
    position(player, world, {-18.5f, 9, 6.5f});
    tick(session, world, player, 5);
    require(session.monsters().size() == 1 && session.activationSerial() == 1
                && session.activations().size() == 1 && !session.activations().front().chained,
            "the actual root starts by proximity and emits one activation cue");
    defeat(session, world, player);
    require(session.pendingCompletions().front() == 3 && session.remaining() == 3,
            "chain completion uses the original placement index even when IDs are reordered");
    position(player, world, {18.5f, 9, 6.5f});
    tick(session, world, player, 40);
    session.markCleared(0);
    require(session.monsters().empty() && !session.active() && session.activationSerial() == 1
                && session.pendingCompletions().size() == 1 && session.remaining() == 3,
            "a failed save or premature acknowledgement cannot release a successor");
    session.markCleared(3);
    require(session.active() && session.remaining() == 2 && session.pendingCompletions().empty(),
            "the committed clear releases a handoff which remains an active encounter");
    tick(session, world, player, 19);
    require(session.activationSerial() == 1 && session.monsters().empty(),
            "no follower starts before the full two-second delay");
    session.markCleared(3);
    tick(session, world, player);
    require(session.activationSerial() == 2 && session.activations().size() == 1
                && session.activations().front().chained && session.activations().front().age == 0
                && session.activations().front().position == assets[0].position,
            "the successor starts at two seconds even far away, and duplicate clears do not restart its timer");
    tick(session, world, player, 4);
    require(session.monsters().empty(), "the chained cue retains normal preparation time before the first enemy");
    tick(session, world, player);
    require(session.monsters().size() == 1, "the chained encounter spawns its configured enemy after preparation");
    defeat(session, world, player);
    require(session.pendingCompletions().front() == 0, "the middle encounter completes second");
    session.markCleared(0);
    tick(session, world, player, 25);
    require(session.activationSerial() == 3 && session.monsters().size() == 1,
            "the second handoff reaches the final encounter");
    defeat(session, world, player);
    require(session.pendingCompletions().front() == 2, "the tail completes last");
    session.markCleared(2);
    tick(session, world, player, 30);
    require(session.remaining() == 0 && !session.active() && session.monsters().empty()
                && session.activationSerial() == 3,
            "clearing the tail finishes the finite chain without repeating enemies or cues");
}

void retriesAndReentry(const World& world)
{
    const auto assets = chain();
    EncounterSession session;
    Player player;
    position(player, world, {-18.5f, 9, 6.5f});
    session.begin(assets, {});
    tick(session, world, player, 5);
    defeat(session, world, player);
    session.resetUnfinished();
    tick(session, world, player, 30);
    require(session.pendingCompletions().size() == 1 && session.activationSerial() == 1,
            "dying with an unsaved completion preserves it without respawning or advancing");
    session.markCleared(3);
    tick(session, world, player, 8);
    session.resetUnfinished();
    tick(session, world, player, 30);
    require(!session.active() && session.monsters().empty() && session.activationSerial() == 1,
            "death during a handoff cancels the remote start while preserving cleared progress");
    position(player, world, {18.5f, 9, 6.5f});
    tick(session, world, player, 30);
    require(session.activationSerial() == 1, "the tail remains locked after a retry");
    position(player, world, {.5f, 9, 6.5f});
    tick(session, world, player, 5);
    require(session.monsters().size() == 1 && session.activationSerial() == 2
                && !session.activations().front().chained,
            "the first unfinished spawner resumes safely by proximity after death");
    session.resetUnfinished();
    require(session.monsters().empty() && session.activations().empty() && session.remaining() == 2,
            "death during a linked fight removes transient enemies and cues without undoing clears");
    tick(session, world, player, 5);
    require(session.monsters().size() == 1 && session.healthBars().front().current == 40,
            "retry restores the unfinished spawner's full enemy health and count");

    const std::array<std::size_t, 1> cleared{3};
    session.begin(assets, {}, cleared);
    position(player, world, {-18.5f, 9, 6.5f});
    const auto serial = session.activationSerial();
    tick(session, world, player, 30);
    require(!session.active() && session.activationSerial() == serial && session.remaining() == 2,
            "reentering a section keeps the cleared prefix quiet and does not start a distant follower");
    position(player, world, {.5f, 9, 6.5f});
    tick(session, world, player, 5);
    require(session.monsters().size() == 1 && session.activationSerial() == serial + 1,
            "saved cleared prefixes expose the first unfinished encounter for proximity activation");

    // Existing progress can be noncontiguous after the author reorders links.
    const std::array<std::size_t, 1> middleCleared{0};
    session.begin(assets, {}, middleCleared);
    position(player, world, {-18.5f, 9, 6.5f});
    tick(session, world, player, 5);
    defeat(session, world, player);
    session.markCleared(3);
    tick(session, world, player, 25);
    require(session.monsters().size() == 1 && session.remaining() == 1
                && session.activations().back().position == assets[2].position,
            "a previously cleared successor is skipped without replaying it or stranding the chain");
}

void feedbackAndDefensiveInput(const World& world)
{
    EncounterSession session;
    Player player;
    position(player, world, {.5f, 9, 6.5f});
    const auto standalone = spawner({.5f, 9, .5f}, 0);
    session.begin(std::span(&standalone, 1), {});
    tick(session, world, player);
    const auto serial = session.activationSerial();
    for (const float seconds : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN()})
        session.update(world, player, {}, false, seconds);
    require(session.activations().front().age == 0 && session.activationSerial() == serial,
            "invalid updates neither advance feedback nor emit duplicate cues");
    tick(session, world, player, 15);
    require(session.activations().empty() && session.activationSerial() == serial,
            "activation pulses expire independently of their monotonic audio serial");
    session.clear();
    require(session.activationSerial() == serial && session.activations().empty(),
            "clearing a session keeps the serial monotonic and removes its visual pulses");
    session.begin(std::span(&standalone, 1), {});
    tick(session, world, player);
    require(session.activationSerial() == serial + 1,
            "the next section's first activation remains observable to audio");

    auto invalid = chain();
    invalid[2].spawner.next = 10;
    session.begin(invalid, {});
    tick(session, world, player, 5);
    require(session.active() && session.monsters().size() == 1,
            "a corrupt circular graph falls back to ordinary proximity encounters instead of deadlocking");
    invalid = chain();
    invalid[0].spawner.next = 999;
    session.begin(invalid, {});
    tick(session, world, player, 5);
    require(session.active() && session.monsters().size() == 1,
            "a missing target cannot leave every spawner permanently dormant");

    std::array capped{spawner({-3.5f, 9, .5f}, 1, 2), spawner({18.5f, 9, .5f}, 2),
                      spawner({3.5f, 9, .5f}, 3, 4), spawner({-18.5f, 9, .5f}, 4)};
    for (auto& asset : capped) asset.spawner.counts = {20, 0, 0};
    session.begin(capped, {});
    const auto before = session.activationSerial();
    tick(session, world, player, 150);
    require(session.monsters().size() == EncounterSession::MaxAlive
                && session.activationSerial() == before + 2,
            "independent chain roots share the existing global live-mob cap while their followers wait");
}
} // namespace

int main()
{
    try {
        World world;
        for (int z = -32; z <= 32; ++z) for (int x = -32; x <= 32; ++x)
            for (int y = 0; y <= 40; ++y) world.set({x, y, z}, y == 8 ? Block::Stone : Block::Air);
        orderAndDurability(world);
        retriesAndReentry(world);
        feedbackAndDefensiveInput(world);
        std::cout << "Encounter chain tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
