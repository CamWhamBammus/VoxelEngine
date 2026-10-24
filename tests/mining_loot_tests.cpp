#include "loot.h"
#include "ores.h"
#include "player.h"
#include "sections.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace voxel;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void settle(LootSession& loot)
{
    for (int frame = 0; frame < 9; ++frame) loot.updateRewardAnimations(.1f);
}
void dropsAndCollection()
{
    World world;
    buildBlankSection(world);
    LootSession loot;
    loot.begin({}, "lush/room", {}, &world);
    const auto collision = loot.collision();
    const glm::ivec3 ore{0, 12, 0};
    world.set(ore, Block::CopperOre);
    loot.releaseOre(world, ore, Block::CopperOre);
    require(world.get(ore) == Block::Air && loot.size() == 1, "mining removes exactly the ore and releases one pickup");
    require(loot.contents(0) == std::vector<ItemStack>{{ItemId::CopperOre, 1}}, "copper blocks drop copper ore items");
    require(loot.collision() == collision, "dynamic ore does not rebuild scenery collision");
    const auto position = loot.sceneAssets()[0].position;
    require(std::abs(position.y - 9) < .01f, "high ore drops land on the floor rather than floating at the mined height");
    require(!loot.canCollectPickup(0, world, position), "a newly released ore waits for its short drop animation");
    settle(loot);
    require(loot.canCollectPickup(0, world, position), "a settled ore uses walking pickup behavior");
    require(!loot.pullPickup(0, position + glm::vec3(1, 0, 0), .05f)
            && glm::length(loot.scenePickupOffsets()[0]) > .01f, "ore is attracted toward the player before collection");
    loot.stopPickupPull(0);
    require(loot.contents(0)[0].count == 1 && loot.scenePickupOffsets()[0] == glm::vec3(0),
            "an interrupted pickup or full inventory leaves the ore available");
    world.set({1, 9, 0}, Block::Stone);
    require(!loot.canCollectPickup(0, world, {1.9f, 9, .5f}), "ore cannot be collected through a nearby wall");
    loot.releaseOre(world, ore, Block::CopperOre);
    require(loot.size() == 1, "releasing the same voxel twice cannot create a second item");
    loot.releaseOre(world, {4, 12, 0}, Block::Stone);
    require(loot.size() == 1, "ordinary materials are not mining rewards");
    for (const auto& info : oreCatalog()) {
        if (info.block == Block::CopperOre) continue;
        const glm::ivec3 cell{static_cast<int>(loot.size()) + 2, 12, 0};
        world.set(cell, info.block);
        loot.releaseOre(world, cell, info.block);
        require(loot.contents(loot.size() - 1)[0] == ItemStack{info.item, 1}, "every resource block drops its matching item");
    }
    require(loot.size() == oreCatalog().size() && loot.collision() == collision,
            "many mined items preserve stable collision and one drop per resource");
}

void persistence()
{
    World world;
    buildBlankSection(world);
    const glm::ivec3 cell{-4, 15, 3};
    world.set(cell, Block::IronOre);
    LootSession loot;
    loot.begin({}, "first", {}, &world);
    const auto key = loot.oreRecord(cell, Block::IronOre);
    require(key != loot.oreRecord(cell + glm::ivec3(1, 0, 0), Block::IronOre)
            && key != loot.oreRecord(cell, Block::GoldOre), "ore identity includes coordinate and original material");
    std::map<std::string, LootProgress> progress;
    progress[key].unlocked = true;
    loot.releaseOre(world, cell, Block::IronOre);
    world.set(cell, Block::IronOre); // Reload the authored room from disk.
    loot.begin({}, "first", progress, &world);
    loot.restoreMinedOres(world, "first", progress);
    require(world.get(cell) == Block::Air && loot.size() == 1 && loot.contents(0)[0].count == 1,
            "reloading a mined room removes the block and restores an unclaimed drop");
    require(loot.canCollectPickup(0, world, loot.sceneAssets()[0].position), "restored ore is immediately collectible");
    loot.restoreMinedOres(world, "first", progress);
    require(loot.size() == 1, "restoring twice cannot duplicate a drop");
    progress[key].taken[0] = 1;
    world.set(cell, Block::IronOre);
    loot.begin({}, "first", progress, &world);
    loot.restoreMinedOres(world, "first", progress);
    require(world.get(cell) == Block::Air && loot.size() == 0, "collected ore and its source stay gone after restart");
    loot.releaseOre(world, cell, Block::IronOre);
    require(loot.size() == 0, "a restored collection record cannot release another copy");
    world.set(cell, Block::IronOre);
    loot.begin({}, "second", progress, &world);
    loot.restoreMinedOres(world, "second", progress);
    require(world.get(cell) == Block::IronOre && loot.size() == 0 && loot.oreRecord(cell, Block::IronOre) != key,
            "matching coordinates in another room are independent");
    world.set(cell, Block::GoldOre);
    loot.begin({}, "first", progress, &world);
    loot.restoreMinedOres(world, "first", progress);
    require(world.get(cell) == Block::GoldOre && loot.size() == 0, "old ore progress cannot erase a differently authored replacement");
    progress["first/ore-99999999999999999999,12,0,7"].unlocked = true;
    progress["first/ore-4,12,0,256"].unlocked = true;
    progress["first/ore-4,12,0,7,trailing"].unlocked = true;
    progress["first/ore-4,12,0,3"].unlocked = true;
    progress["first/ore-04,12,0,7"].unlocked = true;
    loot.restoreMinedOres(world, "first", progress);
    require(loot.size() == 0, "malformed and noncanonical records do not create phantom rewards");
    world.set(cell, Block::IronOre);
    loot.begin({}, {}, {}, &world);
    loot.restoreMinedOres(world, {}, {});
    require(world.get(cell) == Block::IronOre && loot.size() == 0, "a fresh playtest ignores saved player progress");
}

void landingAndStableIndices()
{
    World world;
    buildBlankSection(world);
    std::vector<HubAsset> assets{
        {HubAssetKind::TrialSpawner, {12.5f, 9, .5f}, 0, 0, {{ItemId::Ruby, 2}}},
        {HubAssetKind::LootChest, {8.5f, 9, .5f}, 0, 0, {{ItemId::Coal, 3}}}
    };
    LootSession loot;
    loot.begin(assets, "stable", {}, &world);
    const auto collision = loot.collision();
    world.set({0, 11, 0}, Block::CoalOre);
    world.set({0, 12, 0}, Block::IronOre);
    loot.releaseOre(world, {0, 12, 0}, Block::IronOre);
    require(std::abs(loot.sceneAssets().back().position.y - 12) < .01f, "a solid lower ore supports the first drop");
    settle(loot);
    loot.releaseOre(world, {0, 11, 0}, Block::CoalOre);
    for (const auto& asset : loot.sceneAssets())
        if (isItemPickup(asset.kind)) require(std::abs(asset.position.y - 9) < .01f, "mining away a drop support settles existing ore to the next floor");
    require(loot.recordKey(0) == "stable/asset-1" && loot.recordKey(1) == "stable/asset-0/reward-0"
            && loot.spawnerRecord(0) == "stable/asset-0", "mining preserves authored chest and virtual spawner indices");
    loot.releaseSpawnerRewards(0);
    require(loot.size() == 4 && loot.clearedSpawners() == std::vector<std::size_t>{0}
            && loot.collision() == collision, "spawner rewards remain independent after adding mined drops");

    // A two-block-high wall leaves no room to stand inside its mined recess.
    for (int y = 9; y < 18; ++y) world.set({-5, y, 0}, Block::Stone);
    world.set({-5, 14, 0}, Block::CopperOre);
    loot.releaseOre(world, {-5, 14, 0}, Block::CopperOre);
    const auto wallLanding = loot.sceneAssets().back().position;
    Player probe;
    probe.setAssetCollision(collision);
    require(wallLanding.y < 10 && probe.setFeetPosition(world, wallLanding) && probe.grounded(),
            "wall ores settle beside the wall in a space the player can reach");

    const HubAsset paving{HubAssetKind::PathPaving, {.5f, 9, 5.5f}};
    loot.begin(std::span(&paving, 1), "paving", {}, &world);
    world.set({0, 20, 5}, Block::IronOre);
    loot.releaseOre(world, {0, 20, 5}, Block::IronOre);
    const auto modelLanding = loot.sceneAssets().back().position;
    probe.setAssetCollision(loot.collision());
    require(modelLanding.y > 9 && modelLanding.y < 10 && probe.setFeetPosition(world, modelLanding) && probe.grounded(),
            "ore landing respects the visible top of non-block scenery");
}
} // namespace

int main()
{
    try {
        dropsAndCollection();
        persistence();
        landingAndStableIndices();
        std::cout << "Mining loot tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
