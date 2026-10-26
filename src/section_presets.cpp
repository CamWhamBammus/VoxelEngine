#include "section_presets.h"
#include "ores.h"
#include "spawner_chains.h"

#include <array>
#include <cmath>
#include <stdexcept>

namespace voxel {
namespace {

// Broad continuous strata run through the whole volume, including corners and
// chunk boundaries. No per-block random choice: digging reveals the same seams.
Block geology(glm::ivec3 cell, SectionWorld theme, float phase)
{
    const float x = float(cell.x), y = float(cell.y), z = float(cell.z);
    const float fold = y + 3.5f * std::sin(x * .09f + phase)
        + 2.8f * std::cos(z * .11f - phase);
    const float patch = std::sin(x * .14f + phase) + std::cos(z * .12f - phase)
        + .55f * std::sin(y * .15f + (x + z) * .055f);
    switch (theme) {
    case SectionWorld::LushCaves: {
        // Root trunks wind through the soil rather than lying in flat stripes.
        constexpr std::array<glm::vec3, 4> roots{{{-21,0,-15}, {16,0,11}, {-9,0,25}, {23,0,-22}}};
        for (const auto root : roots) {
            const float dx = x - root.x - 2.1f * std::sin(y * .13f + root.z);
            const float dz = z - root.z - 1.8f * std::cos(y * .10f + root.x);
            if (dx * dx + dz * dz < 2.5f) return Block::RootWood;
        }
        const float layer = std::fmod(fold + 32.f, 17.f);
        if (layer < 5.f) return Block::RootedDirt;
        if (layer < 8.f) return patch > .35f ? Block::Moss : Block::Dirt;
        return patch > -.1f ? Block::MossStone : Block::Stone;
    }
    case SectionWorld::CrystalHollows: {
        const float deposit = std::cos(x * .25f + phase) + std::cos(z * .22f - phase)
            + std::cos(y * .29f);
        if (deposit > 2.35f) return patch > .1f ? Block::AmethystOre : Block::QuartzOre;
        const float layer = std::fmod(fold + 32.f, 20.f);
        if (layer < 7.f) return Block::Slate;
        return layer < 14.f ? Block::Limestone : Block::Calcite;
    }
    case SectionWorld::EmberDepths: {
        if (patch > .9f) return Block::Obsidian;
        const float layer = std::fmod(fold + 32.f, 18.f);
        if (layer < 4.f) return Block::Clay;
        return layer < 7.f ? Block::Stone : Block::Slate;
    }
    }
    return Block::Stone;
}

void dressCave(World& world, std::vector<HubAsset>& assets, SectionWorld theme)
{
    // The center aisle and both markers stay clear. Floor and ceiling models
    // touch existing surfaces; the wall root's +Z face turns into the chamber.
    if (theme == SectionWorld::LushCaves) {
        assets.push_back({HubAssetKind::HangingFern, {-1.8f,30,26.4f}, 0, 31});
        assets.push_back({HubAssetKind::HangingRoots, {2.5f,30,18.2f}, 0, 47});
        assets.push_back({HubAssetKind::RootFan, {-3,27,21.7f}, 1, 13});
        assets.push_back({HubAssetKind::MossDrape, {-3,28,24.2f}, 1, 51});
        assets.push_back({HubAssetKind::PitcherPlants, {-1.9f,25,18.3f}, 0, 29});
        assets.push_back({HubAssetKind::FiddleheadFerns, {2.6f,25,26.3f}, 0, 17});
    } else if (theme == SectionWorld::CrystalHollows) {
        assets.push_back({HubAssetKind::StalactiteCluster, {.5f,30,26.6f}, 0, 24});
        // Embedded mineral faces catch placed light without lighting themselves.
        for (const auto cell : {glm::ivec3{-4,27,21}, {-4,28,21}, {-4,27,22}, {-4,26,22}})
            world.set(cell, Block::AmethystOre);
        for (const auto cell : {glm::ivec3{4,27,24}, {4,28,24}, {4,28,25}, {4,29,25}})
            world.set(cell, Block::QuartzOre);
    } else {
        for (int y = 25; y < 30; ++y) {
            world.set({-4,y,21}, Block::Obsidian);
            world.set({4,y,25}, Block::Obsidian);
        }
    }
}

void dressParkour(World& world, std::vector<HubAsset>& assets, SectionWorld theme)
{
    // Only the edge is dressed. The large central floor remains an empty canvas
    // for jumps, puzzles, water and the author's own routes.
    // Saved anchors must be inside the room's exclusive upper bound.
    constexpr float ceiling = 56.999f;
    if (theme == SectionWorld::LushCaves) {
        for (const int z : {-22, -7, 9, 24}) {
            assets.push_back({z < 0 ? HubAssetKind::JungleMonstera : HubAssetKind::JungleCycad,
                              {-29.5f,9,float(z)}, 1, unsigned(z + 40)});
            assets.push_back({HubAssetKind::MossDrape, {-32,12,float(z)}, 1, unsigned(z + 50)});
            assets.push_back({z < 0 ? HubAssetKind::MossyPlatform : HubAssetKind::RuinedPillar,
                              {29.7f,9,float(z)}, 0, unsigned(z + 80)});
            assets.push_back({HubAssetKind::BambooClump, {29.8f,9,float(z) + 3}, 0, unsigned(z + 120)});
        }
        for (const int x : {-23, -11, 12, 24})
            assets.push_back({x < 0 ? HubAssetKind::HangingRoots : HubAssetKind::HangingFern,
                              {float(x),ceiling,-29}, 0, unsigned(x + 80)});
    } else if (theme == SectionWorld::CrystalHollows) {
        for (const int z : {-22, -6, 10, 25}) {
            assets.push_back({HubAssetKind::CrystalBeacon, {-30,9,float(z)}, 0, unsigned(z + 50)});
            assets.push_back({HubAssetKind::StalactiteCluster, {29,ceiling,float(z)}, 0, unsigned(z + 90)});
        }
    } else {
        for (const int z : {-23, -7, 9, 24})
            assets.push_back({HubAssetKind::StalactiteCluster, {-29,ceiling,float(z)}, 0, unsigned(z + 60)});
    }
    if (theme != SectionWorld::LushCaves) {
        // Mineable specimens sit inside the room, backed by the unbreakable
        // boundary. Removing one cannot open the protected shell to the void.
        for (int cluster = 0; cluster < 4; ++cluster) {
            const int z = -22 + cluster * 14;
            const auto mineral = theme == SectionWorld::EmberDepths ? Block::Obsidian
                : cluster % 2 ? Block::QuartzOre : Block::AmethystOre;
            world.set({31,9,z}, mineral);
            world.set({31,10,z}, mineral);
            world.set({31,9,z+1}, mineral);
        }
    }
}

void buildArena(World& world, std::vector<HubAsset>& assets, SectionWorld theme, bool themed)
{
    const bool lush = themed && theme == SectionWorld::LushCaves;
    const bool crystal = themed && theme == SectionWorld::CrystalHollows;
    const Block masonry = lush ? Block::MossStone : crystal ? Block::Limestone
        : themed ? Block::Slate : Block::Stone;
    const Block trim = lush ? Block::MossBrick : crystal ? Block::Calcite
        : themed ? Block::Obsidian : Block::Brick;
    const Block paving = lush ? Block::Path : crystal ? Block::Limestone
        : themed ? Block::Slate : Block::Stone;
    const Block post = lush ? Block::RootWood : masonry;
    const auto fill = [&](glm::ivec3 low, glm::ivec3 high, Block block) {
        for (int y = low.y; y <= high.y; ++y)
            for (int z = low.z; z <= high.z; ++z)
                for (int x = low.x; x <= high.x; ++x) world.set({x,y,z}, block);
    };

    // A broad octagonal court makes the fighting space readable. The shell's
    // floor remains solid; all cover, galleries and scenery above it is editable.
    for (int z = -28; z <= 28; ++z) for (int x = -26; x <= 26; ++x) {
        if (std::abs(x) + std::abs(z) > 46) continue;
        const bool edge = std::abs(x) == 26 || std::abs(z) == 28
            || std::abs(x) + std::abs(z) == 46;
        world.set({x,8,z}, edge ? trim : paving);
    }
    // Two side galleries have a staircase at either end, so taking the high
    // ground never requires a jump or leaves a player cornered on a platform.
    for (const int side : {-1, 1}) {
        const int first = side < 0 ? -25 : 19;
        fill({first,9,-12}, {first+5,10,12}, masonry);
        for (int x = first; x <= first+5; ++x)
            for (const int end : {-1, 1}) {
                world.set({x,9,end*14}, stairBlock(masonry, end > 0 ? 0 : 2));
                world.set({x,9,end*13}, masonry);
                world.set({x,10,end*13}, stairBlock(masonry, end > 0 ? 0 : 2));
            }
        // Keep lamps outside the six-block-wide gallery route.
        for (const int z : {-21,0,21}) {
            const int x = side*16;
            fill({x,9,z}, {x,11,z}, post);
            assets.push_back({HubAssetKind::LanternCap, {float(x)+.5f,12,float(z)+.5f}});
        }
    }
    // Staggered cover breaks sightlines without closing the central dodge lanes
    // or any spawner's landing ring. These are ordinary removable blocks.
    for (const auto corner : {glm::ivec3{-16,9,-4}, {15,9,7}, {15,9,-15}}) {
        fill(corner, corner+glm::ivec3(2,0,2), masonry);
        world.set(corner, trim);
        world.set(corner+glm::ivec3(2,0,2), trim);
    }
    // Recessed lights illuminate the fight floor even under dark section
    // lighting. They sit flush below the player's feet, never in a dodge lane.
    for (const int z : {-24,-12,0,12,24})
        for (const int x : {-2,2}) world.set({x,8,z}, Block::Lantern);

    if (lush) {
        for (const int side : {-1,1}) {
            const int x = side < 0 ? -30 : 26;
            for (const int z : {-26,20}) {
                // Water rests on the unbroken floor and is enclosed on every
                // side. These small ponds cannot spill into the combat routes.
                fill({x,9,z}, {x+3,9,z+5}, Block::MossStone);
                fill({x+1,9,z+1}, {x+2,9,z+4}, Block::Water);
                assets.push_back({HubAssetKind::CaveLotus, {float(x)+1.5f,10,float(z)+2.5f}});
            }
            for (const int z : {-17,0,17}) {
                const float rootX = side*29.f;
                fill({int(rootX),9,z}, {int(rootX),16,z}, Block::RootWood);
                assets.push_back({HubAssetKind::RootFan, {rootX+.5f,12,float(z)+.5f}, side > 0 ? 3 : 1});
                assets.push_back({HubAssetKind::JungleMonstera, {side*27.5f,9,float(z)+3}, 0, unsigned(z+41)});
                assets.push_back({HubAssetKind::FiddleheadFerns, {side*27.5f,9,float(z)-3}, 0, unsigned(z+61)});
                for (int dx = -1; dx <= 1; ++dx)
                    for (int dz = -2; dz <= 2; ++dz)
                        world.set({int(rootX)+dx,16,z+dz}, Block::Leaves);
            }
            assets.push_back({HubAssetKind::MossyLog, {side*23.f,9,25}, 1});
        }
    } else if (themed) {
        for (const int side : {-1,1}) for (const int z : {-24,-8,8,24}) {
            const int x = side < 0 ? -30 : 28;
            fill({x,9,z}, {x+1,10,z+1}, trim);
            if (crystal)
                assets.push_back({HubAssetKind::CrystalBeacon, {float(x)+.5f,11,float(z)+.5f}});
            else {
                world.set({x,11,z}, Block::AmberCrystal);
                assets.push_back({HubAssetKind::LanternCap, {float(x)+1.5f,11,float(z)+1.5f}});
            }
        }
    }

    const std::size_t first = assets.size();
    const auto wave = [&](glm::vec3 position, std::array<int,3> counts, std::vector<ItemStack> loot) {
        HubAsset spawner{HubAssetKind::TrialSpawner, position};
        spawner.spawner.counts = counts;
        spawner.chestItems = std::move(loot);
        assets.push_back(std::move(spawner));
    };
    wave({-6.5f,9,15.5f}, {0,0,2}, {{ItemId::CopperOre,3}});
    wave({8.5f,9,1.5f}, {2,0,1}, {{ItemId::IronOre,3}});
    wave({-6.5f,9,-12.5f}, {2,1,1}, {{ItemId::Jade,2}, {ItemId::GoldOre,2}});
    // Use the same identity allocator as authored links, so these are ordinary
    // editable spawners with independent counts, loot and links from the outset.
    for (std::size_t i = first; i < first+2; ++i)
        if (const auto error = linkSpawners(assets, i, i+1); !error.empty())
            throw std::logic_error(error);
}

} // namespace

std::string_view sectionPresetName(SectionWorld world, SectionPreset preset)
{
    if (preset == SectionPreset::PlainStone) return "Plain stone";
    switch (world) {
    case SectionWorld::LushCaves: return "Root jungle";
    case SectionWorld::CrystalHollows: return "Crystal grotto";
    case SectionWorld::EmberDepths: return "Ember rock";
    }
    return "World theme";
}

std::string_view sectionPresetDescription(SectionWorld world, SectionPreset preset)
{
    if (preset == SectionPreset::PlainStone) return "A clean stone canvas";
    switch (world) {
    case SectionWorld::LushCaves: return "Rooted earth, moss and tangled greenery";
    case SectionWorld::CrystalHollows: return "Pale calcite, slate and mineral seams";
    case SectionWorld::EmberDepths: return "Dark shale, obsidian and warm clay";
    }
    return "Materials and scenery from this world";
}

void buildSectionPreset(World& world, std::vector<HubAsset>& assets,
                        SectionWorld theme, SectionLayout layout, SectionPreset preset)
{
    assets.clear();
    const bool cave = layout == SectionLayout::Cave;
    if (cave) buildCaveSection(world);
    else buildBlankSection(world);
    if (preset == SectionPreset::PlainStone) {
        if (layout == SectionLayout::Arena) buildArena(world, assets, theme, false);
        return;
    }

    const float phase = float(world.seed() % 1009) * .00623f;
    for (int y = SectionInteriorMin.y - 1; y <= SectionInteriorMax.y; ++y)
        for (int z = SectionInteriorMin.z - 1; z <= SectionInteriorMax.z; ++z)
            for (int x = SectionInteriorMin.x - 1; x <= SectionInteriorMax.x; ++x) {
                const glm::ivec3 cell{x,y,z};
                if (world.get(cell) != Block::Stone) continue;
                auto material = geology(cell, theme, phase);
                if (theme == SectionWorld::LushCaves && world.get(cell + glm::ivec3(0,1,0)) == Block::Air) {
                    if (material == Block::Dirt || material == Block::RootedDirt) material = Block::Grass;
                    else if (material == Block::Stone) material = Block::MossStone;
                }
                // Editor protection does not stop the gameplay pickaxe. The
                // one-block boundary must therefore contain no mineable ore.
                if (!sectionInterior(cell) && oreInfo(material))
                    material = theme == SectionWorld::CrystalHollows ? Block::Calcite : Block::Slate;
                world.set(cell, material);
            }
    if (layout == SectionLayout::Arena) buildArena(world, assets, theme, true);
    else if (cave) dressCave(world, assets, theme);
    else dressParkour(world, assets, theme);
}

} // namespace voxel
