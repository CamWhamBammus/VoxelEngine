#include "mining.h"
#include "equipment.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace voxel;
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
int breakFrames(MiningSession& session, MiningTarget target, int tier, float seconds)
{
    for (int i = 1; i <= 1000; ++i)
        if (const auto broken = session.update(target, true, tier, seconds)) {
            require(*broken == target, "completed mining returns the exact targeted cell and material");
            return i;
        }
    throw std::runtime_error("eligible ore did not finish mining");
}
}

int main()
{
    try {
        const MiningTarget coal{{0,10,0}, Block::CoalOre};
        const MiningTarget copper{{1,10,0}, Block::CopperOre};
        MiningSession session;
        require(!session.update(coal, false, 1, .1f) && session.target() == coal
                && session.progress() == 0 && session.swing() == 0,
                "aiming identifies ore without mining or swinging");
        session.update(coal, true, 1, .1f);
        require(session.progress() > 0 && session.swing() == 1, "holding begins work and a swing");
        session.update(coal, false, 1, .1f);
        require(session.progress() == 0 && session.swing() > 0 && session.swing() < 1,
                "release clears work while the existing swing finishes");
        for (int i = 0; i < 6; ++i) session.update(coal, false, 1, .1f);
        require(session.swing() == 0, "released pickaxe returns to idle");
        session.update(coal, true, 1, .1f);
        session.update(coal, true, 1, .1f);
        session.update(copper, true, 2, .1f);
        require(std::abs(session.progress() - .107f / oreInfo(Block::CopperOre)->mineSeconds) < .0001f,
                "target changes cannot transfer mining progress");
        session.update({}, true, 1, .1f);
        require(!session.target() && session.progress() == 0 && session.swing() > 0,
                "swinging into empty space keeps the animation but clears work");
        session.update(MiningTarget{{0,10,0},Block::Stone}, true, 3, .1f);
        require(!session.target(), "ordinary stone cannot be mined");
        session.update(MiningTarget{{0,-1,0},Block::CoalOre}, true, 3, .1f);
        require(!session.target(), "out-of-world targets are ignored");

        const MiningTarget ruby{{2,10,0}, Block::RubyOre};
        for (int i = 0; i < 50; ++i)
            require(!session.update(ruby, true, 2, .1f), "low tier cannot break a gem source");
        require(session.target() == ruby && session.blocked() && session.progress() == 0,
                "locked ore exposes the tier warning with zero progress");
        breakFrames(session, ruby, 14, 1.0f/60);
        require(!session.target() && session.progress() == 0, "completion consumes accumulated work");
        for (const auto& ore : oreCatalog()) {
            const MiningTarget target{{3,10,0}, ore.block};
            session.reset();
            for (int frame = 0; frame < 30; ++frame)
                require(!session.update(target, true, ore.requiredTier - 1, .1f),
                        "no resource can be mined below its own material tier");
            require(session.blocked() && session.progress() == 0,
                    "every locked material reports zero progress");
            breakFrames(session, target, ore.requiredTier, 1.0f/60);
        }
        float previousProgress = 0;
        for (int tier = 1; tier <= GearTierCount; ++tier) {
            session.reset();
            session.update(coal, true, tier, .01f);
            require(session.progress() > previousProgress, "every material tier mines familiar ore faster");
            previousProgress = session.progress();
        }
        session.reset();
        const int starter = breakFrames(session, coal, 1, 1.0f/60);
        const int upgraded = breakFrames(session, coal, 3, 1.0f/60);
        require(upgraded < starter, "an upgraded pickaxe mines familiar ore faster");
        session.reset();
        const int at30 = breakFrames(session, coal, 1, 1.0f/30);
        require(std::abs(at30/30.0f - starter/60.0f) <= 1.0f/30,
                "mining duration is consistent across frame rates");
        session.reset();
        session.update(coal, true, 1, .1f);
        const float before = session.progress(), swing = session.swing();
        for (const float dt : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()})
            require(!session.update(copper, true, 1, dt) && session.target() == coal
                    && session.progress() == before && session.swing() == swing,
                    "invalid frame times cannot advance or corrupt mining");
        session.reset();
        session.update(coal, true, 1, 100);
        require(session.progress() < .2f, "a stalled frame cannot instantly break a block");
        session.cancelTarget();
        require(!session.target() && session.progress() == 0 && session.swing() > 0,
                "pausing discards work without snapping the hand");
        session.reset();
        require(!session.target() && session.progress() == 0 && session.swing() == 0 && !session.blocked(),
                "tool or section changes clear every mining state");
        std::cout << "Mining timing tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
