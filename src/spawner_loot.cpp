#include "loot.h"
#include "player.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace voxel {
namespace {
glm::vec3 rewardLanding(const World* world, std::shared_ptr<const AssetCollision> collision,
                        glm::vec3 source, std::size_t slot)
{
    // Pick a supported spot beside the pedestal, with room to walk over the drop.
    if (world) {
        Player probe;
        probe.setAssetCollision(std::move(collision));
        for (int attempt = 0; attempt < 32; ++attempt) {
            const float angle = float((slot * 3 + attempt) % 16) * 6.2831853f / 16;
            const float radius = attempt < 16 ? 1.65f : 2.7f;
            const auto start = source + glm::vec3(std::cos(angle)*radius,2,std::sin(angle)*radius);
            if (!probe.setFeetPosition(*world,start)) continue;
            probe.move(*world,{0,-5,0});
            if (probe.grounded() && !probe.inWater()) return probe.feetPosition();
        }
    }
    // A completely enclosed pedestal still keeps its rewards reachable on top.
    return source + glm::vec3(0,1.0f,0);
}
}

void LootSession::prepareSpawnerRewards(const World* world, std::string_view section,
                                        const std::map<std::string,LootProgress>& progress)
{
    const auto count = authoredAssets_.size();
    for (std::size_t index = 0; index < count; ++index) {
        // Appending pickups can reallocate the authored vector.
        const auto source = authoredAssets_[index];
        if (!isSpawnerAsset(source.kind)) continue;
        const auto key = std::string(section.empty() ? "playtest" : section) + "/asset-" + std::to_string(index);
        const auto saved = progress.find(key);
        const bool cleared = saved != progress.end() && saved->second.unlocked;
        spawnerRewards_.push_back({index,key});
        if (cleared) {
            clearedSpawners_.push_back(index);
            authoredAssets_[index].variation |= 0x80000000u;
        }
        for (std::size_t slot = 0; slot < source.chestItems.size(); ++slot) {
            const auto landing = rewardLanding(world,obstacles_,source.position,slot);
            const auto record = key + "/reward-" + std::to_string(slot);
            auto stack = source.chestItems[slot];
            const auto claim = progress.find(record);
            if (claim != progress.end()) stack.count = std::max(0,stack.count-claim->second.taken[0]);
            HubAsset drop{HubAssetKind::ItemPickup,landing};
            drop.pickupItem = source.chestItems[slot];
            Chest chest{drop.kind,authoredAssets_.size(),landing,record,false,{}, {stack}};
            chest.enabled = cleared;
            chest.spawnerAsset = index;
            chest.launchOffset = source.position + glm::vec3(0,1.3f,0) - (landing + PickupVisualCenter);
            chests_.push_back(std::move(chest));
            authoredAssets_.push_back(std::move(drop));
        }
    }
}

const std::string& LootSession::spawnerRecord(std::size_t assetIndex) const
{
    const auto found = std::find_if(spawnerRewards_.begin(),spawnerRewards_.end(),
        [&](const auto& reward) { return reward.assetIndex == assetIndex; });
    if (found == spawnerRewards_.end()) throw std::out_of_range("Unknown spawner reward");
    return found->key;
}

void LootSession::releaseSpawnerRewards(std::size_t assetIndex)
{
    if (std::find(clearedSpawners_.begin(),clearedSpawners_.end(),assetIndex) != clearedSpawners_.end()) return;
    spawnerRecord(assetIndex); // Validate before changing session state.
    clearedSpawners_.push_back(assetIndex);
    authoredAssets_[assetIndex].variation |= 0x80000000u;
    for (auto& chest : chests_) {
        if (chest.spawnerAsset != assetIndex) continue;
        chest.enabled = true;
        chest.launchSeconds = 0;
        chest.pickupOffset = chest.launchOffset;
    }
    refreshAssets();
}

void LootSession::updateRewardAnimations(float seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0) return;
    seconds = std::min(seconds,.1f);
    for (auto& chest : chests_) {
        if (!chest.enabled || chest.launchSeconds >= .8f || chest.contents[0].count == 0) continue;
        chest.launchSeconds = std::min(.8f,chest.launchSeconds+seconds);
        const float t = chest.launchSeconds/.8f;
        chest.pickupOffset = chest.launchOffset*(1-t) + glm::vec3(0,std::sin(t*3.14159265f)*.7f,0);
        if (t >= 1) chest.pickupOffset = {};
        scenePickupOffsets_[chest.sceneIndex] = chest.pickupOffset;
    }
}
} // namespace voxel
