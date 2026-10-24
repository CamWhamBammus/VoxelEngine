#include "game.h"
#include "input.h"

#include <algorithm>
#include <cmath>

namespace voxel {

bool Game::updateCombat(const InputActions& actions, float seconds)
{
    if (!actions.active || !healthVisible() || health_.dead()
        || !std::isfinite(seconds) || seconds <= 0) return health_.dead();
    updateMining(actions, seconds);
    const bool swing = equipment.selectedTool() == Tool::Sword && actions.attack;
    const int damage = encounters_.update(world,player,camera.forward(),swing,seconds);
    damagePlayer(damage, encounters_.playerHitDirection());
    if (playerKnockbackRemaining_ > 0 && !health_.dead()) {
        const float before = playerKnockbackRemaining_ / PlayerKnockbackDuration;
        playerKnockbackRemaining_ = std::max(0.0f, playerKnockbackRemaining_ - std::min(seconds, .1f));
        const float after = playerKnockbackRemaining_ / PlayerKnockbackDuration;
        // The regular movement update still owns gravity and controls. This
        // small additional sweep decelerates without teleporting through walls.
        player.move(world, playerHitDirection_ * PlayerKnockbackDistance
                           * (before * before - after * after));
        camera.setPosition(player.eyePosition());
    }
    encounterSaveRetry_ = std::max(0.0f,encounterSaveRetry_-std::min(seconds,.1f));
    if (encounterSaveRetry_ > 0) return health_.dead();
    // Persist the clear before exposing rewards. A failed write stays pending,
    // so neither the exit nor a reward claim can bypass durable progress.
    const auto pending = encounters_.pendingCompletions();
    const std::vector<std::size_t> completed(pending.begin(),pending.end());
    for (const auto index : completed) {
        auto next = equipment;
        next.lootProgress[loot_.spawnerRecord(index)].unlocked = true;
        if (!commitEquipment(next,"SPAWNER CLEARED | REWARDS RELEASED")) {
            encounterSaveRetry_ = 1;
            status = inventoryMessage_;
            break;
        }
        loot_.releaseSpawnerRewards(index);
        encounters_.markCleared(index);
        status = "SPAWNER CLEARED | COLLECT YOUR REWARDS";
    }
    return health_.dead();
}

} // namespace voxel
