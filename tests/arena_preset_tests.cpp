#include "section_presets.h"
#include "asset_collision.h"
#include "ores.h"
#include "player.h"
#include "spawner_chains.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace {
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory()
    {
        auto pattern = (std::filesystem::temp_directory_path() / "voxel-arena-presets-XXXXXX").string();
        std::vector<char> bytes(pattern.begin(), pattern.end());
        bytes.push_back('\0');
        if (!::mkdtemp(bytes.data())) throw std::runtime_error("Cannot create arena test directory");
        path = bytes.data();
    }
    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
} // namespace

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool value, const char* message) {
        if (!value) { std::cerr << message << '\n'; ++failures; }
    };
    TemporaryDirectory temp;
    check(int(SectionCategory::Parkour) == 0 && int(SectionCategory::Exploration) == 2
          && int(SectionCategory::Treasure) == 4 && int(SectionCategory::Arena) == 5,
          "Arena appends its category without changing existing saved identifiers");
    const auto category = sectionCategoryCatalog().back();
    check(category.id == SectionCategory::Arena && category.name == "Arena" && category.folder == "arena",
          "Arena has a readable category and separate library folder");

    for (const auto& theme : sectionWorldCatalog()) {
        for (const auto preset : {SectionPreset::WorldTheme, SectionPreset::PlainStone}) {
            World world(719);
            world.set({80,80,80}, Block::Crystal);
            std::vector<HubAsset> assets{{HubAssetKind::LootChest, {0,9,0}}};
            buildSectionPreset(world, assets, theme.id, SectionLayout::Arena, preset);
            check(world.seed() == 719 && world.get({80,80,80}) == Block::Air,
                  "Arena clears the old scene while preserving its seed");
            auto collision = std::make_shared<AssetCollision>();
            collision->rebuild(assets);
            SectionMetadata metadata;
            metadata.world = theme.id;
            metadata.category = SectionCategory::Arena;
            metadata.name = std::string(theme.name) + " arena";
            metadata.lighting = SectionLighting::Dark;
            check(sectionMarkerSafe(world, metadata.spawn, collision.get())
                  && sectionMarkerSafe(world, metadata.finish, collision.get()),
                  "Arena markers have supported floors and full body clearance");

            bool safeShell = true, waterSupported = true;
            int water = 0, stairs = 0;
            std::set<Block> materials;
            for (int y = 8; y <= 57; ++y)
                for (int z = -33; z <= 32; ++z)
                    for (int x = -33; x <= 32; ++x) {
                        const glm::ivec3 cell{x,y,z};
                        const auto block = world.get(cell);
                        materials.insert(block);
                        if (!sectionInterior(cell))
                            safeShell &= isSolid(block) && !oreInfo(block);
                        stairs += isStair(block);
                        if (block == Block::Water) {
                            ++water;
                            waterSupported &= y == 9 && isSolid(world.get(cell+glm::ivec3(0,-1,0)));
                            for (const auto neighbor : {glm::ivec3(-1,0,0), {1,0,0}, {0,0,-1}, {0,0,1}})
                                waterSupported &= world.get(cell+neighbor) != Block::Air;
                        }
                    }
            check(safeShell, "Arena keeps its entire protected shell solid and non-mineable");
            check(stairs == 48, "Both raised galleries have six-wide actual stairs at both ends");
            check(waterSupported, "All decorative water has a solid bottom and contained sides");
            if (preset == SectionPreset::WorldTheme && theme.id == SectionWorld::LushCaves)
                check(water > 0 && materials.contains(Block::RootWood) && materials.contains(Block::Leaves)
                      && materials.contains(Block::MossStone),
                      "The lush arena has roots, leafy canopies, mossy ruins and water");
            else check(water == 0, "Other themes and plain stone do not inherit lush ponds");
            check(buildAssetDressing(assets).lights.size() >= 9,
                  "Every arena has actual light models for its dark lighting setting");
            for (const auto& asset : assets)
                check(sectionInterior(glm::ivec3(glm::floor(asset.position))),
                      "Every arena model is inside the editable region");

            Player walker;
            walker.setAssetCollision(collision);
            check(walker.setFeetPosition(world, metadata.spawn), "Arena entrance can receive a walking player");
            for (int frame = 0; frame < 300 && walker.feetPosition().z > metadata.finish.z; ++frame)
                walker.update(world, {0,1}, {0,0,-1}, false, false, .05f);
            check(walker.feetPosition().z <= metadata.finish.z && std::abs(walker.feetPosition().y-9) < .01f,
                  "A wide central route connects entrance and exit without jumping or swimming");
            // Climb each real staircase using ordinary player movement. A
            // geometric height check alone would miss backward-facing treads.
            for (const int x : {-22,22}) for (const int end : {-1,1}) {
                Player climber;
                climber.setAssetCollision(collision);
                check(climber.setFeetPosition(world, {float(x)+.5f,9,float(end)*15.5f}),
                      "Gallery stair approach is clear");
                for (int frame = 0; frame < 24; ++frame)
                    climber.update(world, {0,1}, {0,0,float(-end)}, false, false, .05f);
                check(climber.feetPosition().y > 10.99f && climber.grounded(),
                      "Players can walk up either end of either gallery without jumping");
            }

            std::vector<std::size_t> waves;
            for (std::size_t i = 0; i < assets.size(); ++i)
                if (isSpawnerAsset(assets[i].kind)) waves.push_back(i);
            check(waves.size() == 3 && validateSpawnerChains(assets).empty(),
                  "Every arena has a valid chain of three editable trial spawners");
            if (waves.size() != 3) continue;
            constexpr std::array expectedCounts{std::array{0,0,2}, std::array{2,0,1}, std::array{2,1,1}};
            for (std::size_t wave = 0; wave < waves.size(); ++wave) {
                const auto& asset = assets[waves[wave]];
                check(asset.spawner.id != 0 && asset.spawner.counts == expectedCounts[wave],
                      "Arena waves progress from slimes to a modest mixed fight");
                check(validChestItems(asset) && !asset.chestItems.empty(),
                      "Every wave has its own authored loot, including the intermediate fights");
                check(wave == 2 ? asset.spawner.next == 0
                      : spawnerById(assets, asset.spawner.next) == waves[wave+1],
                      "The three waves form one linear chain in intended order");
                // All 48 floor candidates used by the actual spawner have
                // enough room for the widest slime and tallest wizard.
                for (int candidate = 0; candidate < 48; ++candidate) {
                    const float angle = float(candidate % 16) * 3.14159265359f / 8;
                    const float radius = 2.2f + float(candidate/16);
                    auto point = asset.position + glm::vec3(std::sin(angle)*radius,0,std::cos(angle)*radius);
                    point.x = std::floor(point.x)+.5f; point.z = std::floor(point.z)+.5f;
                    check(sectionMarkerSafe(world, point, collision.get()),
                          "Every enemy landing candidate has a supported floor and body clearance");
                    check(!collision->overlaps(point+glm::vec3(-.7f,.01f,-.7f),
                                                point+glm::vec3(.7f,2.5f,.7f)),
                          "Enemy spawn rings are clear of scenery even for broad slimes");
                    bool volumeClear = true;
                    for (int y = 9; y < 12; ++y)
                        for (int z = int(std::floor(point.z-.7f)); z <= int(std::floor(point.z+.7f)); ++z)
                            for (int x = int(std::floor(point.x-.7f)); x <= int(std::floor(point.x+.7f)); ++x)
                                volumeClear &= world.get({x,y,z}) == Block::Air;
                    check(volumeClear, "Cover and water never occupy enemy landing volumes");
                }
            }
            check(assets[waves[2]].chestItems.size() > assets[waves[0]].chestItems.size(),
                  "The last wave starts with a richer reward that authors can change independently");

            World repeated(719);
            std::vector<HubAsset> repeatedAssets;
            buildSectionPreset(repeated, repeatedAssets, theme.id, SectionLayout::Arena, preset);
            check(world.sameContents(repeated) && assets == repeatedAssets,
                  "Arena templates and stable chain identities are deterministic");
            const auto path = saveSectionDraft(world, metadata, assets, temp.path);
            const auto loaded = loadSection(path);
            check(loaded.meta.category == SectionCategory::Arena && loaded.meta.world == theme.id
                  && loaded.world.sameContents(world) && loaded.assets == assets,
                  "Arena geometry, category, all per-wave loot and links survive saving and loading");
            metadata.tested = true;
            const auto accepted = acceptSection(world, metadata, assets, temp.path);
            check(accepted.parent_path().filename() == "arena",
                  "Accepted arenas are stored under their world's arena category");
        }
    }
    check(listSections(temp.path).size() == 6 && listSectionDrafts(temp.path).size() == 6,
          "Arena drafts and accepted sections are discoverable in the existing libraries");
    for (const auto& theme : sectionWorldCatalog())
        check(listSections(temp.path, theme.id).size() == 2,
              "World filtering finds both arena variants in the correct world");
    return failures ? 1 : 0;
}
