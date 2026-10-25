#include "section_presets.h"
#include "ores.h"

#include <array>
#include <cmath>

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
        assets.push_back({HubAssetKind::HangingRoots, {-1.8f,30,26.4f}, 0, 31});
        assets.push_back({HubAssetKind::HangingRoots, {2.5f,30,18.2f}, 0, 47});
        assets.push_back({HubAssetKind::RootFan, {-3,27,21.7f}, 1, 13});
        assets.push_back({HubAssetKind::Fern, {-2,25,18.3f}, 0, 29});
        assets.push_back({HubAssetKind::CaveOrchid, {2.6f,25,26.3f}, 0, 17});
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
            assets.push_back({HubAssetKind::FernBed, {-29.5f,9,float(z)}, 1, unsigned(z + 40)});
            assets.push_back({HubAssetKind::RootFan, {-32,12,float(z)}, 1, unsigned(z + 50)});
            assets.push_back({HubAssetKind::MossyStones, {29.7f,9,float(z)}, 0, unsigned(z + 80)});
            assets.push_back({HubAssetKind::TreeFern, {29.8f,9,float(z) + 3}, 0, unsigned(z + 120)});
        }
        for (const int x : {-23, -11, 12, 24})
            assets.push_back({HubAssetKind::HangingRoots, {float(x),ceiling,-29}, 0, unsigned(x + 80)});
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
                        SectionWorld theme, bool cave, SectionPreset preset)
{
    assets.clear();
    if (cave) buildCaveSection(world);
    else buildBlankSection(world);
    if (preset == SectionPreset::PlainStone) return;

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
    if (cave) dressCave(world, assets, theme);
    else dressParkour(world, assets, theme);
}

} // namespace voxel
