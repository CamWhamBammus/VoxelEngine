#include "monster_preview.h"
#include "sections.h"
#include <algorithm>
#include <cmath>

namespace voxel {
namespace {
void changeAnimation(MonsterInstance& monster, MonsterAnimation animation)
{
    monster.transitionFrom = blendedMonsterPose(monster);
    monster.animation = animation;
    monster.seconds = 0;
    monster.transition = 0;
}
}

void MonsterPreview::build(World& world)
{
    buildBlankSection(world);
    scenery_.clear();
    // Low stages in a fern-lined grotto keep feet, silhouettes and shadows easy
    // to see. Plenty of space behind them lets the player inspect the rigs.
    for (int z = -12; z <= 13; ++z) for (int x = -17; x <= 17; ++x) {
        world.set({x,8,z}, std::abs(x) > 12 || z < -7 ? Block::Moss : Block::Path);
        const int wall = 5 + (x*x + z*z) % 3;
        if (std::abs(x) > 14 || z < -10 || z > 11)
            for (int y = 9; y < 9 + wall; ++y)
                world.set({x,y,z}, y == 8 + wall ? Block::Moss : Block::Slate);
    }
    for (int i = 0; i < 3; ++i) {
        const int x = (i - 1) * 5;
        for (int dx = -2; dx <= 1; ++dx) for (int z = -3; z <= 0; ++z)
            world.set({x+dx,8,z}, i == 1 ? Block::MossBrick : Block::Brick);
        monsters_[i] = {};
        monsters_[i].kind = static_cast<MonsterKind>(i);
        monsters_[i].position = {float(x),9,-1};
        monsters_[i].yaw = i == 0 ? 12.0f : i == 2 ? -12.0f : 0.0f;
        scenery_.push_back({HubAssetKind::FernBed,{x-1.8f,9,-6},0,std::uint32_t(i)});
        scenery_.push_back({HubAssetKind::MossyStones,{x+1.7f,9,-5.5f},i,std::uint32_t(i)});
        scenery_.push_back({HubAssetKind::TreeFern,{x+.5f,9,-8},i,std::uint32_t(i)});
        scenery_.push_back({HubAssetKind::CrystalFlower,{x-2.5f,9,-3.2f},0,0});
    }
    for (int x : {-10,10}) for (int z : {-5,4}) {
        world.set({x,9,z},Block::MossBrick);
        world.set({x,10,z},Block::Timber);
        world.set({x,11,z},Block::Lantern);
        scenery_.push_back({HubAssetKind::LanternCap,{x+.5f,11.5f,z+.5f},0,0});
    }
    // Broad warm front lights and cool rear crystals, independent of the player.
    for (int x : {-6,0,6}) {
        world.set({x,13,4},Block::Lantern);
        world.set({x,14,4},Block::Timber);
        for (int z = -10; z <= 4; ++z) world.set({x,15,z},Block::Timber);
        for (int y = 9; y < 11; ++y) world.set({x,y,-8},Block::MossStone);
        world.set({x,11,-8},Block::Crystal);
    }
    elapsed_ = 0;
    automatic_ = true;
}

void MonsterPreview::select(MonsterAnimation animation)
{
    automatic_ = false;
    for (auto& monster : monsters_) changeAnimation(monster,animation);
}

void MonsterPreview::automatic()
{
    select(MonsterAnimation::Idle);
    automatic_ = true;
    elapsed_ = 0;
}

void MonsterPreview::update(float seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0) return;
    seconds = std::min(seconds, .1f);
    if (automatic_) elapsed_ = std::fmod(elapsed_ + seconds, 24.0f);
    for (auto& monster : monsters_) {
        const auto next = static_cast<MonsterAnimation>(static_cast<int>(elapsed_ / 8));
        const float duration = monsterCycleDuration(monster.kind,monster.animation);
        const bool finished = duration == 0 || std::floor((monster.seconds+seconds)/duration)
            > std::floor(monster.seconds/duration);
        if (automatic_ && next != monster.animation && finished) changeAnimation(monster,next);
        monster.seconds += seconds;
        monster.transition = std::min(1.0f, monster.transition + seconds / .25f);
    }
}

} // namespace voxel
