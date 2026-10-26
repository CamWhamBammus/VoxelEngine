#include "section_presets.h"
#include "asset_collision.h"
#include "ores.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <unistd.h>
#include <glm/common.hpp>

namespace {
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory()
    {
        auto pattern = (std::filesystem::temp_directory_path() / "voxel-presets-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        if (!::mkdtemp(buffer.data())) throw std::runtime_error("Cannot create preset test directory");
        path = buffer.data();
    }
    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

bool luminous(voxel::Block block)
{
    using voxel::Block;
    return block == Block::Lantern || block == Block::Crystal || block == Block::AmberCrystal
        || block == Block::GlowLeaves || block == Block::GlowMoss;
}

bool chamber(glm::ivec3 p)
{
    return p.x >= -3 && p.x < 4 && p.y >= 25 && p.y < 30 && p.z >= 17 && p.z < 28;
}
} // namespace

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) { std::cerr << message << '\n'; ++failures; }
    };
    TemporaryDirectory temp;
    std::set<std::string_view> names;
    for (const auto& info : sectionWorldCatalog()) {
        names.insert(sectionPresetName(info.id, SectionPreset::WorldTheme));
        check(!sectionPresetDescription(info.id, SectionPreset::WorldTheme).empty(),
              "each world has a readable preset description");
        check(sectionPresetName(info.id, SectionPreset::PlainStone) == "Plain stone",
              "the plain canvas has the same name in every world");
        for (const bool cave : {false, true}) {
            World world(213);
            world.set({80,80,80}, Block::Crystal);
            std::vector<HubAsset> assets{{HubAssetKind::LootChest, {0,9,0}}};
            buildSectionPreset(world, assets, info.id, cave ? SectionLayout::Cave : SectionLayout::Parkour, SectionPreset::WorldTheme);
            check(world.seed() == 213 && world.get({80,80,80}) == Block::Air,
                  "presets clear the previous scene and preserve the document seed");
            bool layoutSafe = true, dark = true, shellProtected = true, specimensSupported = true;
            std::set<Block> materials;
            std::size_t neighbors = 0, matching = 0, specimens = 0;
            for (int y = 8; y <= 57; ++y)
                for (int z = -33; z <= 32; ++z)
                    for (int x = -33; x <= 32; ++x) {
                        const glm::ivec3 p{x,y,z};
                        const auto block = world.get(p);
                        const bool filled = cave ? !chamber(p) : !sectionInterior(p);
                        const bool specimen = !cave && sectionInterior(p) && isSolid(block);
                        // A few mineable samples may line the outermost edge;
                        // the rest of the parkour room remains empty.
                        layoutSafe &= filled ? isSolid(block) : specimen
                            ? x == SectionInteriorMax.x - 1 && y <= 11 && oreInfo(block)
                            : block == Block::Air;
                        if (!sectionInterior(p)) shellProtected &= oreInfo(block) == nullptr;
                        if (specimen) {
                            ++specimens;
                            specimensSupported &= isSolid(world.get(p + glm::ivec3(0,-1,0)))
                                && isSolid(world.get(p + glm::ivec3(1,0,0)))
                                && !oreInfo(world.get(p + glm::ivec3(1,0,0)));
                        }
                        dark &= !luminous(block);
                        if (isSolid(block)) materials.insert(block);
                        if (cave && isSolid(block) && x < 31) {
                            const auto next = world.get(p + glm::ivec3(1,0,0));
                            if (isSolid(next)) { ++neighbors; matching += block == next; }
                        }
                    }
            check(layoutSafe, "themes preserve the complete protected shell and the template's open space");
            check(shellProtected, "no mineable material can open a hole through the one-block protected shell");
            if (!cave && info.id != SectionWorld::LushCaves)
                check(specimens > 0 && specimens <= 24 && specimensSupported,
                      "mineable parkour samples are sparse, grounded and backed by non-mineable boundary rock");
            check(materials.size() >= 4, "each themed canvas uses several complementary terrain materials");
            if (info.id == SectionWorld::LushCaves)
                check(materials.contains(Block::RootedDirt) && materials.contains(Block::MossStone)
                      && materials.contains(Block::RootWood), "the lush palette includes soil, moss and winding roots");
            if (info.id == SectionWorld::CrystalHollows)
                check(materials.contains(Block::Calcite) && materials.contains(Block::AmethystOre)
                      && materials.contains(Block::QuartzOre), "the crystal palette includes pale rock and mineral seams");
            if (info.id == SectionWorld::EmberDepths)
                check(materials.contains(Block::Obsidian) && materials.contains(Block::Clay),
                      "the ember palette includes dark volcanic glass and warm strata");
            if (cave) {
                check(dark && buildAssetDressing(assets).lights.empty(),
                      "dark cave presets introduce no built-in light sources");
                check(matching > neighbors * .65, "geology forms continuous deposits rather than per-block noise");
                std::set<Block> excavatedMaterials;
                for (int z = -24; z < 17; ++z) {
                    const glm::ivec3 cut{0,26,z};
                    excavatedMaterials.insert(world.get(cut));
                }
                check(excavatedMaterials.size() >= 2,
                      "the theme continues behind the starting room for later excavation");
            }
            AssetCollision collision;
            collision.rebuild(assets);
            const SectionMetadata blank;
            check(sectionMarkerSafe(world, cave ? SectionCaveSpawn : blank.spawn, &collision)
                  && sectionMarkerSafe(world, cave ? SectionCaveFinish : blank.finish, &collision),
                  "dressing leaves both markers with a safe floor and full player clearance");
            if (!cave)
                check(!collision.overlaps({-26,9,-26}, {26,56,26}),
                      "parkour scenery leaves the central building space unobstructed");
            for (const auto& asset : assets) {
                check(sectionInterior(glm::ivec3(glm::floor(asset.position))),
                      "all preset scenery can be selected, removed and saved inside the editor");
                const bool hanging = asset.kind == HubAssetKind::HangingRoots
                    || asset.kind == HubAssetKind::HangingFern
                    || asset.kind == HubAssetKind::StalactiteCluster;
                const bool wall = asset.kind == HubAssetKind::RootFan || asset.kind == HubAssetKind::MossDrape;
                const auto support = asset.position + (hanging ? glm::vec3(0,.01f,0)
                    : wall ? glm::vec3(-.01f,0,0) : glm::vec3(0,-.01f,0));
                check(isSolid(world.get(glm::ivec3(glm::floor(support)))),
                      "scenery is anchored to a solid floor, ceiling or wall");
            }

            World repeated(213);
            std::vector<HubAsset> repeatedAssets;
            buildSectionPreset(repeated, repeatedAssets, info.id, cave ? SectionLayout::Cave : SectionLayout::Parkour, SectionPreset::WorldTheme);
            check(world.sameContents(repeated) && assets == repeatedAssets,
                  "rebuilding the same preset is deterministic");

            SectionMetadata metadata;
            metadata.world = info.id;
            metadata.name = std::string(sectionPresetName(info.id, SectionPreset::WorldTheme));
            if (cave) {
                metadata.category = SectionCategory::Exploration;
                metadata.spawn = SectionCaveSpawn;
                metadata.finish = SectionCaveFinish;
                metadata.lighting = SectionLighting::Dark;
            }
            const auto path = temp.path / (std::to_string(int(info.id)) + (cave ? "-cave.vxs" : "-parkour.vxs"));
            saveSection(world, metadata, assets, path);
            const auto loaded = loadSection(path);
            check(world.sameContents(loaded.world) && assets == loaded.assets
                  && loaded.meta.world == metadata.world && loaded.meta.lighting == metadata.lighting,
                  "a preset saves as ordinary editable section data without new format requirements");
        }
    }
    check(names.size() == sectionWorldCatalog().size(), "each world's preset has a distinct name");

    for (const bool cave : {false, true}) {
        World original(311), plain(311);
        if (cave) buildCaveSection(original);
        else buildBlankSection(original);
        std::vector<HubAsset> assets{{HubAssetKind::Fern, {1,9,1}}};
        buildSectionPreset(plain, assets, SectionWorld::LushCaves, cave ? SectionLayout::Cave : SectionLayout::Parkour, SectionPreset::PlainStone);
        check(original.sameContents(plain) && assets.empty(),
              "plain stone exactly preserves the earlier builders and adds no scenery");
    }
    return failures ? 1 : 0;
}
