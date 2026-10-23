#include "loot.h"
#include "sections.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace voxel;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::size_t pickups(const LootSession& loot)
{
    return std::count_if(loot.sceneAssets().begin(),loot.sceneAssets().end(),
        [](const auto& asset) { return isItemPickup(asset.kind); });
}
}
int main()
{
    try {
        World world; buildBlankSection(world);
        std::vector<HubAsset> assets{
            {HubAssetKind::TrialSpawner,{.5f,9,.5f},0,0,{{ItemId::Ruby,8},{ItemId::Key,1}}},
            {HubAssetKind::LootChest,{8.5f,9,.5f},0,0,{{ItemId::IronOre,3}}},
            {HubAssetKind::TrialSpawner,{-8.5f,9,.5f},0,0,{{ItemId::GoldOre,7}}}
        };
        LootSession loot;
        loot.begin(assets,"room",{},&world);
        require(loot.size() == 4 && pickups(loot) == 0,"spawner rewards are hidden before completion");
        require(loot.recordKey(0) == "room/asset-1" && loot.recordKey(1) == "room/asset-0/reward-0",
            "virtual rewards never renumber the authored chest identities");
        require(loot.spawnerRecord(2) == "room/asset-2","each spawner has its own completion record");
        loot.releaseSpawnerRewards(0);
        require(pickups(loot) == 2 && loot.clearedSpawners() == std::vector<std::size_t>{0},
            "clearing one spawner releases only its rewards");
        bool popped = false;
        for (std::size_t i=0;i<loot.sceneAssets().size();++i) {
            const auto& asset = loot.sceneAssets()[i];
            if (!isItemPickup(asset.kind)) continue;
            const auto emitted = asset.position+PickupVisualCenter+loot.scenePickupOffsets()[i];
            require(glm::length(emitted-(assets[0].position+glm::vec3(0,1.3f,0)))<.001f,
                "fresh rewards emerge from the top of the spawner");
            require(!loot.canCollectPickup(1,world,asset.position),"rewards cannot be claimed mid-emission");
            popped = true;
        }
        require(popped,"the emission has visible reward meshes");
        for(int i=0;i<9;++i) loot.updateRewardAnimations(.1f);
        for(const auto offset : loot.scenePickupOffsets())
            require(glm::length(offset)<.001f,"emitted rewards settle onto their ground anchors");
        glm::vec3 landing{};
        for(const auto& asset : loot.sceneAssets()) if(isItemPickup(asset.kind)) { landing=asset.position; break; }
        require(loot.canCollectPickup(1,world,landing),"settled reward can be picked up by walking over it");
        loot.releaseSpawnerRewards(0);
        require(pickups(loot)==2 && loot.clearedSpawners().size()==1,"releasing twice cannot duplicate rewards");

        std::map<std::string,LootProgress> saved;
        saved["room/asset-0"].unlocked = true;
        saved["room/asset-0/reward-0"].taken[0] = 3;
        loot.begin(assets,"room",saved,&world);
        require(loot.clearedSpawners()==std::vector<std::size_t>{0} && pickups(loot)==2,
            "a permanently cleared spawner reloads only its unclaimed drops");
        require(loot.contents(1)[0].count==5 && loot.contents(2)[0].count==1,
            "partial claims persist while unspent keys remain available on another visit");
        saved["room/asset-0/reward-0"].taken[0]=8;
        saved["room/asset-0/reward-1"].taken[0]=1;
        loot.begin(assets,"room",saved,&world);
        require(pickups(loot)==0 && loot.clearedSpawners().size()==1,
            "claimed rewards stay gone while the encounter stays cleared");
        loot.begin(assets,"room",{},&world);
        require(loot.clearedSpawners().empty() && pickups(loot)==0,"fresh playtests ignore permanent progress");
        loot.clear();
        require(loot.size()==0 && loot.clearedSpawners().empty(),"leaving clears encounter reward state");
        std::cout << "Spawner reward tests passed\n";
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
