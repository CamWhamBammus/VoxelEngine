#include "game.h"
#include "input.h"
#include "section_tools.h"
#include "spawner_chains.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
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
void click(voxel::Game& game, int id)
{
    voxel::InputActions input; input.active = true; input.devClick = id; game.update(input);
}
}

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool value, const char* message) {
        if (!value) { std::cerr << message << '\n'; ++failures; }
    };
    char temporary[] = "/tmp/voxel-spawner-chains-editor-XXXXXX";
    if (!mkdtemp(temporary)) return 1;
    const std::filesystem::path root(temporary);
    try {
        World world; buildBlankSection(world);
        SectionMetadata meta;
        std::vector<HubAsset> original{
            {HubAssetKind::TrialSpawner, {5.5f,9,14.5f}},
            {HubAssetKind::TrialSpawner, {11.5f,9,14.5f}},
            {HubAssetKind::TrialSpawner, {17.5f,9,14.5f}}
        };
        const auto draft = saveSectionDraft(world, meta, original, root);
        Options options; options.dev = true; options.sectionLibrary = root.string();
        Game game(options); auto& editor = game.sectionEditor();
        click(game, SectionUiWorldBase); click(game, SectionUiLibraryDraftBase);
        const auto target = [&](std::size_t index) {
            const auto position = editor.assets().at(index).position;
            game.camera.setPosition(position + glm::vec3(0,2,3));
            aim(game.camera, position + glm::vec3(0,.5f,0));
        };
        const auto open = [&](std::size_t index) {
            target(index); press(game, &InputActions::devPalette);
            check(editor.screen() == SectionEditorScreen::SpawnerSettings
                  && editor.editingSpawnerIndex() == index, "E opens the chosen spawner");
        };
        const auto choose = [&](std::size_t index) {
            target(index); press(game, &InputActions::removePressed);
        };
        open(0);
        click(game, SectionUiSpawnerPlusBase);
        click(game, SectionUiSpawnerRewards);
        click(game, SectionUiChestItemBase + static_cast<int>(ItemId::Ruby));
        click(game, SectionUiChestApply);
        click(game, SectionUiSpawnerLink);
        check(editor.screen() == SectionEditorScreen::Build && editor.flying() && !editor.menuOpen()
              && editor.linkingSpawner() == 0, "Link selection allows flying with a source index");
        for (const auto key : {&InputActions::save, &InputActions::devTest, &InputActions::devAccept,
                              &InputActions::devLibrary, &InputActions::undo, &InputActions::redo,
                              &InputActions::devRename, &InputActions::devTools, &InputActions::devCopy,
                              &InputActions::devPaste, &InputActions::devPalette}) {
            press(game, key);
            check(editor.linkingSpawner() == 0 && editor.assets() == original && !editor.dirty(),
                  "Global shortcuts cannot mutate or leave an active link selection");
        }
        target(1); press(game, &InputActions::remove); press(game, &InputActions::place);
        check(editor.linkingSpawner() == 0 && editor.assets() == original,
              "Repeated creative mouse actions cannot remove or place assets while linking");
        choose(0);
        check(editor.linkingSpawner() == 0 && editor.assets() == original, "Selecting the source is rejected without edits");
        game.camera.setPosition({0,12,0}); aim(game.camera, {0,9,0});
        press(game, &InputActions::removePressed);
        check(editor.linkingSpawner() == 0 && editor.assets() == original, "Clicking a block is not a spawner link");
        press(game, &InputActions::devCancel);
        check(editor.screen() == SectionEditorScreen::SpawnerSettings && !editor.linkingSpawner()
              && editor.spawnerCounts()[0] == 3 && editor.spawnerRewards() == std::vector<ItemStack>{{ItemId::Ruby,1}},
              "Cancelling target selection returns to all prior draft settings");
        click(game, SectionUiSpawnerLink); choose(1);
        check(editor.screen() == SectionEditorScreen::SpawnerSettings && editor.spawnerNext() == 1
              && editor.assets() == original && !editor.dirty(), "Choosing a valid target only stages the link");
        click(game, SectionUiSpawnerLink); press(game, &InputActions::closeMenu);
        check(editor.screen() == SectionEditorScreen::SpawnerSettings && editor.spawnerNext() == 1,
              "Escape from selection preserves a previously staged target");
        click(game, SectionUiSpawnerCancel);
        check(editor.assets() == original && !editor.dirty() && !editor.editingSpawnerIndex(),
              "Cancelling settings discards counts, rewards and links together");

        open(0); click(game, SectionUiSpawnerPlusBase);
        click(game, SectionUiSpawnerRewards);
        click(game, SectionUiChestItemBase + static_cast<int>(ItemId::Ruby));
        click(game, SectionUiChestApply);
        click(game, SectionUiSpawnerLink); choose(1); click(game, SectionUiSpawnerApply);
        const auto firstLink = editor.assets();
        check(firstLink[0].spawner.id != 0 && firstLink[0].spawner.next == firstLink[1].spawner.id
              && firstLink[0].spawner.counts[0] == 3 && firstLink[0].chestItems == std::vector<ItemStack>{{ItemId::Ruby,1}},
              "Save commits settings and newly allocated link identities atomically");
        press(game, &InputActions::undo);
        check(editor.assets() == original, "One undo restores counts, rewards and both endpoint identities");
        press(game, &InputActions::redo);
        check(editor.assets() == firstLink, "Redo restores the exact chain identities");
        open(0); click(game, SectionUiSpawnerPlusBase); click(game, SectionUiSpawnerApply);
        check(editor.assets()[0].spawner.id == firstLink[0].spawner.id
              && editor.assets()[0].spawner.next == firstLink[0].spawner.next
              && editor.assets()[1].spawner == firstLink[1].spawner,
              "Editing enemy counts does not reset a saved chain or its endpoint identities");
        press(game, &InputActions::undo);
        check(editor.assets() == firstLink, "Undoing a count edit preserves the preexisting chain");
        open(1); click(game, SectionUiSpawnerLink); choose(2); click(game, SectionUiSpawnerApply);
        const auto chain = editor.assets();
        check(chain[0].spawner.next == chain[1].spawner.id && chain[1].spawner.next == chain[2].spawner.id,
              "A second link extends the first chain without renumbering it");
        open(2); click(game, SectionUiSpawnerLink); choose(0);
        check(editor.linkingSpawner() == 2 && editor.assets() == chain, "Editor rejects a cycle");
        choose(1);
        check(editor.linkingSpawner() == 2 && editor.assets() == chain, "Editor rejects two predecessors for one spawner");
        press(game, &InputActions::devCancel); click(game, SectionUiSpawnerCancel);
        open(0); click(game, SectionUiSpawnerUnlink);
        check(!editor.spawnerNext() && editor.assets() == chain, "Unlink remains a draft change");
        click(game, SectionUiSpawnerCancel);
        check(editor.assets() == chain, "Cancelling an unlink preserves the chain");
        open(0); click(game, SectionUiSpawnerUnlink); click(game, SectionUiSpawnerApply);
        check(editor.assets()[0].spawner.next == 0 && editor.assets()[0].spawner.id == chain[0].spawner.id
              && editor.assets()[1].spawner == chain[1].spawner, "Unlink disconnects only the selected edge");
        press(game, &InputActions::undo);
        check(editor.assets() == chain, "Undo restores a removed edge");
        target(1); press(game, &InputActions::remove);
        check(editor.assets().size() == 2 && editor.assets()[0].spawner.next == 0
              && editor.assets()[1] == chain[2], "Deleting the middle spawner disconnects rather than skipping a fight");
        press(game, &InputActions::undo);
        check(editor.assets() == chain, "Undo of deletion restores the entire chain and removed model");
        press(game, &InputActions::save);
        check(loadSection(draft).assets == chain, "Saved editor chains reopen with their settings and identities intact");
        open(0); click(game, SectionUiSpawnerPlusBase); click(game, SectionUiSpawnerLink);
        press(game, &InputActions::devToggle);
        check(!game.devMode() && !editor.linkingSpawner() && loadSection(draft).assets == chain,
              "Leaving the editor while picking discards unconfirmed settings");

        const auto copied = copySectionBox(world, chain, {{5,9,14},{17,9,14}});
        check(copied.assets.size() == 3 && copied.assets[0].spawner == chain[0].spawner,
              "Copying a whole chain preserves its internal graph in the clipboard");
        const auto partial = copySectionBox(world, chain, {{5,9,14},{5,9,14}});
        check(partial.assets.size() == 1 && partial.assets[0].spawner.next == 0,
              "Partial copy drops links to unselected destinations");
        const auto pasted = planSectionPaste(world, chain, copied, {-20,9,-10}, 0, false, {0,20,0});
        check(pasted.error.empty() && pasted.assets.size() == 6 && validateSpawnerChains(pasted.assets).empty(),
              "Pasting creates a separate valid three-spawner chain");
        if (pasted.assets.size() == 6) {
            check(std::equal(chain.begin(), chain.end(), pasted.assets.begin()), "Pasting leaves the original chain unchanged");
            check(pasted.assets[3].spawner.next == pasted.assets[4].spawner.id
                  && pasted.assets[4].spawner.next == pasted.assets[5].spawner.id
                  && pasted.assets[3].chestItems == chain[0].chestItems,
                  "Copied links point to copied targets and retain reward payloads");
            for (std::size_t i = 3; i < 6; ++i)
                check(std::none_of(chain.begin(), chain.end(), [&](const HubAsset& asset) {
                    return asset.spawner.id == pasted.assets[i].spawner.id;
                }), "Every copied identity is distinct from the existing graph");
            const auto repeated = planSectionPaste(world, pasted.assets, copied, {-20,9,-10}, 0, false, {0,20,0});
            check(repeated.error.empty() && repeated.assets == pasted.assets,
                  "Pasting onto identical placements preserves existing identities and links");
        }
        const auto pair = copySectionBox(world, chain, {{5,9,14},{11,9,14}});
        std::vector<HubAsset> duplicate{chain[1]};
        duplicate[0].position = {-13.5f,9,-9.5f};
        duplicate[0].spawner.id = 900; duplicate[0].spawner.next = 0;
        const auto skippedTarget = planSectionPaste(world, duplicate, pair, {-20,9,-10}, 0, false, {0,20,0});
        check(skippedTarget.error.empty() && skippedTarget.assets.size() == 2
              && skippedTarget.assets[0] == duplicate[0] && skippedTarget.assets[1].spawner.next == 0,
              "A duplicate target skipped during paste cannot connect the copied source to the existing spawner");
        const auto erased = planSectionFill(world, chain, {{11,9,14},{11,9,14}}, Block::Air, SectionFillMode::Erase, {0,20,0});
        const auto carved = planSectionCarve(world, chain, {11,9,14}, 1, SectionCarveShape::Box);
        for (const auto& plan : {erased, carved})
            check(plan.error.empty() && plan.assets.size() == 2 && plan.assets[0].spawner.next == 0
                  && plan.assets[1] == chain[2] && validateSpawnerChains(plan.assets).empty(),
                  "Bulk erasing and carving remove dangling links along with their target");
        SectionClipboard empty; empty.size = {1,1,1}; empty.blocks = {Block::Air};
        const auto replaced = planSectionPaste(world, chain, empty, {11,9,14}, 0, true, {0,20,0});
        check(replaced.error.empty() && replaced.assets.size() == 2 && replaced.assets[0].spawner.next == 0,
              "Air-inclusive paste disconnects a spawner whose successor was replaced");
        auto malformed = copied; malformed.assets[2].spawner.next = malformed.assets[0].spawner.id;
        const auto rejected = planSectionPaste(world, chain, malformed, {-20,9,-10}, 0, false, {0,20,0});
        check(!rejected.error.empty() && rejected.blocks.empty() && rejected.assets == chain,
              "Malformed copied cycles fail before any world or graph edits");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
