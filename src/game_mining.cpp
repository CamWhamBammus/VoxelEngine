#include "game.h"
#include "input.h"
#include "ores.h"

#include <algorithm>

namespace voxel {

void Game::updateMining(const InputActions& actions, float seconds)
{
    miningMessageSeconds_ = std::max(0.0f, miningMessageSeconds_ - std::min(seconds, .1f));
    if (miningMessageSeconds_ == 0 || actions.attack) miningMessage_.clear();
    if (equipment.selectedTool() != Tool::Pickaxe) {
        mining_.reset();
        miningMessage_.clear();
        miningMessageSeconds_ = 0;
        return;
    }
    std::optional<MiningTarget> aimedOre;
    if (sectionLootActive_) {
        // Keep physical aiming separate from the cosmetic hurt-camera jolt.
        const auto hit = raycast(camera.position(), camera.forward(), MiningSession::Reach,
            [&](glm::ivec3 cell) {
                const auto block = world.get(cell);
                return isSolid(block) ? block : Block::Air;
            });
        if (hit && oreInfo(world.get(hit->position))) {
            const auto obstruction = loot_.collision()->raycast(camera.position(), camera.forward(), MiningSession::Reach);
            if (!obstruction || obstruction->distance >= hit->distance - .001f)
                aimedOre = MiningTarget{hit->position, world.get(hit->position)};
        }
    }
    const auto completed = mining_.update(aimedOre, actions.mineHeld || actions.attack, equipment.pickaxeTier, seconds);
    if (!completed) return;
    const auto record = loot_.oreRecord(completed->position, completed->block);
    auto next = equipment;
    next.lootProgress[record].unlocked = true;
    if (!commitEquipment(next, "MINED " + std::string(blockName(completed->block)) + " | WALK OVER TO COLLECT")) {
        miningMessage_ = inventoryMessage_;
        miningMessageSeconds_ = 1.8f;
        status = inventoryMessage_;
        return;
    }
    // A successful profile write owns both removal and the unclaimed reward.
    // Reloading can recreate the item even if the game closes before pickup.
    loot_.releaseOre(world, completed->position, completed->block);
    pickupAttempts_.resize(loot_.size());
    miningMessage_.clear();
    miningMessageSeconds_ = 0;
    status = inventoryMessage_;
}

} // namespace voxel
