#include "game.h"
#include "input.h"
#include "section_tools.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
void aim(voxel::Camera& camera, glm::vec3 point)
{
    const auto direction = glm::normalize(point - camera.position()), old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}
void press(voxel::Game& game, bool voxel::InputActions::*key)
{
    voxel::InputActions input; input.active = true; input.*key = true; game.update(input);
}
void click(voxel::Game& game, int id, bool shift = false)
{
    voxel::InputActions input; input.active = true; input.devClick = id; input.sprint = shift; game.update(input);
}
template<class Operation> bool fails(Operation operation)
{
    try { operation(); return false; } catch (const std::exception&) { return true; }
}
std::string bytes(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void integer(std::string& data, std::size_t offset, std::uint32_t value)
{
    for (int i = 0; i < 4; ++i) data.at(offset + i) = static_cast<char>(value >> (8 * i));
}
void writeChecked(const std::filesystem::path& path, std::string data)
{
    std::uint32_t checksum = 2166136261u;
    for (std::size_t i = 8; i < data.size() - 4; ++i)
        checksum = (checksum ^ static_cast<unsigned char>(data[i])) * 16777619u;
    integer(data, data.size() - 4, checksum);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
}
}

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool value, const char* message) {
        if (!value) { std::cerr << message << '\n'; ++failures; }
    };
    char temporary[] = "/tmp/voxel-spawner-authoring-XXXXXX";
    if (!mkdtemp(temporary)) return 1;
    const std::filesystem::path root(temporary);
    try {
        World world; buildBlankSection(world);
        SectionMetadata meta;
        std::vector<HubAsset> assets{{HubAssetKind::TrialSpawner, {5.5f, 9, 14.5f}}};
        check(sectionAssetCatalog().back().kind == HubAssetKind::TrialSpawner
              && isCreativeAsset(HubAssetKind::TrialSpawner), "Spawner is a separate creative asset");
        check(buildingGridSize(HubAssetKind::TrialSpawner) == glm::ivec3(1), "Spawner occupies one grid cell");
        check(validSpawnerSettings(assets[0].spawner), "Default spawner has valid enemies");
        check(!validSpawnerSettings({{0,0,0}}) && !validSpawnerSettings({{-1,1,1}})
              && !validSpawnerSettings({{21,1,1}}), "Spawner counts reject empty, negative and oversized encounters");
        const auto mesh = buildHubAsset(assets[0]);
        check(!mesh.vertices.empty() && !mesh.indices.empty(), "Spawner has a real creative-preview model");
        for (const auto& vertex : mesh.vertices) {
            const auto local = vertex.position - assets[0].position;
            check(std::abs(local.x) <= .501f && std::abs(local.z) <= .501f && local.y >= 0 && local.y <= 1,
                  "Spawner mesh fits one block without overhanging adjacent builds");
        }
        AssetCollision collision; collision.rebuild(assets);
        check(collision.overlaps(assets[0].position + glm::vec3(-.2f,.4f,-.2f),
                                 assets[0].position + glm::vec3(.2f,.6f,.2f)), "Spawner core is solid");
        auto clearedAsset = assets[0]; clearedAsset.variation |= 0x80000000u;
        const auto clearedMesh = buildHubAsset(clearedAsset);
        check(clearedMesh.vertices.size() == mesh.vertices.size() && clearedMesh.indices == mesh.indices,
              "Clearing changes spawner appearance without replacing its physical model");
        for (std::size_t i = 0; i < std::min(clearedMesh.vertices.size(),mesh.vertices.size()); ++i)
            check(clearedMesh.vertices[i].position == mesh.vertices[i].position,
                  "Cleared spawner geometry cannot move into nearby buildings or the player");
        check(buildAssetDressing(assets).lights.size() == 1
              && buildAssetDressing(std::span(&clearedAsset,1)).lights.empty(),
              "Cleared spawners extinguish their encounter light");

        const auto draft = saveSectionDraft(world, meta, assets, root);
        Options options; options.dev = true; options.sectionLibrary = root.string();
        Game game(options); auto& editor = game.sectionEditor();
        click(game, SectionUiWorldBase); click(game, SectionUiLibraryDraftBase);
        const auto open = [&] {
            game.camera.setPosition(assets[0].position + glm::vec3(0, 2, 3));
            aim(game.camera, assets[0].position + glm::vec3(0, .5f, 0));
            press(game, &InputActions::devPalette);
            check(editor.screen() == SectionEditorScreen::SpawnerSettings, "E opens the targeted spawner settings");
        };
        open();
        check(editor.menuOpen() && !editor.flying(), "Spawner settings pause creative flight");
        for (int kind = 0; kind < 3; ++kind) click(game, SectionUiSpawnerMinusBase + kind, true);
        click(game, SectionUiSpawnerApply);
        check(editor.screen() == SectionEditorScreen::SpawnerSettings && editor.assets()[0].spawner == SpawnerSettings{},
              "Zero enemies cannot be committed");
        for (int kind = 0; kind < 3; ++kind) {
            for (int step = 0; step < 8; ++step) click(game, SectionUiSpawnerPlusBase + kind, true);
            check(editor.spawnerCounts()[kind] == SpawnerMobLimit, "Count controls cap each kind at twenty");
        }
        click(game, SectionUiSpawnerMinusBase + 1);
        click(game, SectionUiSpawnerMinusBase + 2, true);
        for (auto key : {&InputActions::save, &InputActions::devTest, &InputActions::devLibrary,
                         &InputActions::devAccept, &InputActions::undo, &InputActions::redo}) {
            press(game, key);
            check(editor.screen() == SectionEditorScreen::SpawnerSettings && !editor.dirty(),
                  "Global shortcuts cannot apply or discard staged spawner edits");
        }
        click(game, SectionUiSpawnerRewards);
        check(editor.screen() == SectionEditorScreen::ChestContents
              && editor.editingChestKind() == HubAssetKind::TrialSpawner, "Rewards reuse item contents editing");
        click(game, SectionUiChestItemBase + static_cast<int>(ItemId::Ruby));
        click(game, SectionUiChestPlus, true);
        click(game, SectionUiChestSlotBase + 1);
        click(game, SectionUiChestItemBase + static_cast<int>(ItemId::Key));
        click(game, SectionUiChestApply);
        const std::vector<ItemStack> rewards{{ItemId::Ruby,11},{ItemId::Key,1}};
        check(editor.screen() == SectionEditorScreen::SpawnerSettings && editor.spawnerRewards() == rewards
              && editor.assets()[0].chestItems.empty() && !editor.dirty(), "Rewards remain staged until the entire spawner is saved");
        click(game, SectionUiSpawnerCancel);
        check(editor.screen() == SectionEditorScreen::Build && editor.assets() == assets,
              "Cancel discards staged counts and staged rewards together");
        open(); click(game, SectionUiSpawnerPlusBase + 1, true);
        click(game, SectionUiSpawnerRewards);
        click(game, SectionUiChestItemBase + static_cast<int>(ItemId::Ruby));
        click(game, SectionUiChestPlus, true);
        click(game, SectionUiChestApply);
        click(game, SectionUiSpawnerRewards); click(game, SectionUiChestClear); click(game, SectionUiChestCancel);
        check(editor.spawnerRewards() == std::vector<ItemStack>{{ItemId::Ruby,11}}, "Cancelling reward subview preserves its earlier staged rewards");
        click(game, SectionUiSpawnerApply);
        const auto configured = editor.assets();
        check(configured[0].spawner.counts == std::array<int,3>{2,6,1}
              && configured[0].chestItems == std::vector<ItemStack>{{ItemId::Ruby,11}}, "Save applies enemy counts and exact rewards");
        press(game, &InputActions::undo);
        check(editor.assets() == assets, "One undo reverses both encounter and reward configuration");
        press(game, &InputActions::redo);
        check(editor.assets() == configured, "Redo restores the complete configured spawner");
        press(game, &InputActions::save);
        check(loadSection(draft).assets == configured, "Current section documents preserve enemy counts and rewards on disk");

        const auto clipboard = copySectionBox(world, configured, {{5,9,14},{5,9,14}});
        const auto plan = planSectionPaste(world, configured, clipboard, {15,9,14}, 1, false, {0,15,0});
        check(plan.error.empty() && plan.assets.size() == 2 && plan.assets.back().spawner == configured[0].spawner
              && plan.assets.back().chestItems == configured[0].chestItems && plan.assets.back().facing == 1,
              "Copy, rotate and paste preserve the encounter payload");

        const auto original = bytes(draft);
        for (const auto counts : {std::array<int,3>{0,0,0}, {-1,0,1}, {0,21,1}}) {
            auto invalid = configured; invalid[0].spawner.counts = counts;
            check(fails([&] { saveSection(world, meta, invalid, draft, true); }) && bytes(draft) == original,
                  "Invalid authored counts cannot replace a valid section");
        }
        const auto malformed = root / "invalid.vxs";
        const std::size_t firstAsset = 60 + meta.name.size();
        const std::size_t countOffset = firstAsset + 36 + configured[0].chestItems.size() * 8;
        for (const auto value : {21u, 0xffffffffu}) {
            auto corrupt = original; integer(corrupt, countOffset, value); writeChecked(malformed, corrupt);
            check(fails([&] { static_cast<void>(loadSection(malformed)); }), "Untrusted file counts are rejected before use");
        }
        auto corrupt = original;
        for (int kind = 0; kind < 3; ++kind) integer(corrupt, countOffset + kind * 4, 0);
        writeChecked(malformed, corrupt);
        check(fails([&] { static_cast<void>(loadSection(malformed)); }), "Empty encounter in a valid-checksum file is rejected");
        corrupt = original; integer(corrupt, 8, 3); writeChecked(malformed, corrupt);
        check(fails([&] { static_cast<void>(loadSection(malformed)); }), "Version three cannot introduce a newer spawner kind");

        const auto legacyPath = root / "legacy-v3.vxs";
        std::vector<HubAsset> legacy{{HubAssetKind::LootChest,{12.5f,9,14.5f},0,0,{{ItemId::CopperOre,5}}},
                                   {HubAssetKind::ItemPickup,{14.5f,9,14.5f},0,0,{}, {ItemId::Key,1}}};
        saveSection(world, meta, legacy, legacyPath);
        auto legacyBytes = bytes(legacyPath);
        legacyBytes.erase(legacyBytes.size() - 8, 4); // v3 has no lighting footer.
        integer(legacyBytes, 8, 3); writeChecked(legacyPath, legacyBytes);
        check(loadSection(legacyPath).assets == legacy, "Older section files still preserve chest and pickup payloads");
        open(); click(game, SectionUiSpawnerPlusBase);
        press(game, &InputActions::devToggle);
        check(!game.devMode() && loadSection(draft).assets == configured, "Leaving the builder discards unconfirmed configuration");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
