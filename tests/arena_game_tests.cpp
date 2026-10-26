#include "game.h"
#include "input.h"
#include "spawner_chains.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace {
using namespace voxel;

void require(bool result, const char* message)
{
    if (!result) throw std::runtime_error(message);
}
void tick(Game& game, int frames = 1)
{
    InputActions input; input.active = true;
    for (int i = 0; i < frames; ++i) game.update(input, .1f);
}
void press(Game& game, bool InputActions::*key)
{
    InputActions input; input.active = true; input.*key = true; game.update(input, .1f);
}
void click(Game& game, int id)
{
    InputActions input; input.active = true; input.devClick = id; game.update(input);
}
void type(Game& game, std::string text)
{
    InputActions input; input.active = true; input.devText = std::move(text); game.update(input);
}
void position(Game& game, glm::vec3 feet)
{
    require(game.player.setFeetPosition(game.world, feet), "arena test position must have clearance");
    game.camera.setPosition(game.player.eyePosition());
}
void aim(Game& game, glm::vec3 target)
{
    const auto direction = glm::normalize(target - game.camera.position()), old = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                       glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}
std::string read(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<HubAsset> chain(const std::vector<HubAsset>& assets)
{
    require(validateSpawnerChains(assets).empty(), "arena starts with a valid spawner graph");
    std::vector<HubAsset> result;
    const auto first = std::find_if(assets.begin(), assets.end(), [&](const auto& asset) {
        return isSpawnerAsset(asset.kind) && std::none_of(assets.begin(), assets.end(), [&](const auto& other) {
            return isSpawnerAsset(other.kind) && other.spawner.next == asset.spawner.id;
        });
    });
    require(first != assets.end(), "arena has a proximity-triggered first wave");
    result.push_back(*first);
    while (result.back().spawner.next) {
        const auto next = spawnerById(assets, result.back().spawner.next);
        require(next.has_value() && result.size() < assets.size(), "arena chain resolves without cycling");
        result.push_back(assets[*next]);
    }
    require(result.size() == 3, "starter arena supplies three linked waves");
    return result;
}

using Quantities = std::array<int, ItemCount>;
Quantities availableItems(const Game& game)
{
    Quantities result{};
    for (const auto& info : itemCatalog()) result[static_cast<int>(info.id)] = game.equipment.inventory.count(info.id);
    for (const auto& asset : game.sectionSceneAssets()) {
        if (!isItemPickup(asset.kind)) continue;
        const auto stack = itemPickupStack(asset);
        result[static_cast<int>(stack.item)] += stack.count;
    }
    return result;
}

void approach(Game& game, const HubAsset& spawner)
{
    constexpr float pi = 3.14159265359f;
    for (int candidate = 0; candidate < 16; ++candidate) {
        const float angle = float(candidate) * pi / 8;
        const auto feet = spawner.position + glm::vec3(std::sin(angle) * 5, 0, std::cos(angle) * 5);
        if (!sectionMarkerSafe(game.world, feet, game.sectionSceneCollision().get())) continue;
        position(game, feet);
        return;
    }
    throw std::runtime_error("arena spawner has no nearby accessible approach");
}

void walkIntoArena(Game& game, glm::vec3 spawn)
{
    position(game, spawn);
    InputActions walk; walk.active = true; walk.movement = {0, 1};
    aim(game, game.camera.position() + glm::vec3(0, 0, -1));
    for (int frame = 0; frame < 180 && !game.encounters().active(); ++frame)
        game.update(walk, 1.f / 60);
    require(game.encounters().active(), "walking down the starter arena's entrance activates its first wave");
}

void swing(Game& game, int frame)
{
    const auto target = game.encounters().monsters().front();
    constexpr float pi = 3.14159265359f;
    bool placed = false;
    // Rotate the approach around scenery rather than firing through cover.
    for (int candidate = 0; candidate < 16; ++candidate) {
        const float angle = float((candidate + frame / 5) % 16) * pi / 8;
        const auto feet = target.position + glm::vec3(std::sin(angle) * 1.8f, 0, std::cos(angle) * 1.8f);
        if (game.player.setFeetPosition(game.world, feet)) { placed = true; break; }
    }
    require(placed, "the arena leaves space to reach its spawned enemies with the sword");
    game.camera.setPosition(game.player.eyePosition());
    aim(game, target.position + glm::vec3(0, target.kind == MonsterKind::Slime ? .55f : 1.f, 0));
    InputActions attack; attack.active = attack.attack = true; attack.selectedTool = 3;
    game.update(attack, .1f);
}

void clearWaves(Game& game, const std::vector<HubAsset>& waves, Quantities expected)
{
    std::array<bool, 3> kindsSeen{};
    for (std::size_t wave = 0; wave < waves.size(); ++wave) {
        const auto before = game.encounters().remaining();
        require(before == waves.size() - wave, "exactly the previous waves are cleared");
        bool spawned = false;
        for (int frame = 0; frame < 500 && game.encounters().remaining() == before; ++frame) {
            for (const auto& monster : game.encounters().monsters()) {
                spawned = true;
                kindsSeen[static_cast<int>(monster.kind)] = true;
            }
            if (game.encounters().monsters().empty()) tick(game);
            else swing(game, frame);
            require(!game.health().dead(), "controlled arena sword combat finishes without player death");
        }
        if (!spawned || game.encounters().remaining() + 1 != before) {
            std::cerr << "Wave " << wave + 1 << ": remaining=" << game.encounters().remaining()
                      << ", spawned=" << spawned << ", alive=" << game.encounters().monsters().size()
                      << ", active=" << game.encounters().active() << ", serial=" << game.encounters().activationSerial()
                      << ", status=" << game.status << '\n';
            for (const auto& monster : game.encounters().monsters())
                std::cerr << "Mob " << static_cast<int>(monster.kind) << " at " << monster.position.x << ','
                          << monster.position.y << ',' << monster.position.z << '\n';
            throw std::runtime_error("every authored wave spawns its enemies and can be cleared with sword attacks");
        }
        require(!waves[wave].chestItems.empty(), "each starter wave has editable rewards of its own");
        for (const auto& stack : waves[wave].chestItems) expected[static_cast<int>(stack.item)] += stack.count;
        require(availableItems(game) == expected,
                "each wave immediately releases exactly its own loot, without waiting for the final wave");
        if (wave + 1 < waves.size()) {
            const auto serial = game.encounters().activationSerial();
            tick(game, 10);
            require(game.encounters().activationSerial() == serial && game.encounters().monsters().empty(),
                    "the next wave leaves a pause after the current wave's payout");
        }
    }
    require(std::all_of(kindsSeen.begin(), kindsSeen.end(), [](bool value) { return value; }),
            "the complete starter chain exercises slimes, skeletons, and a wizard");
}

void testArena(const std::filesystem::path& root)
{
    Options options; options.dev = true;
    options.sectionLibrary = (root / "sections").string();
    options.levelLibrary = (root / "levels").string();
    options.progressPath = (root / "progress.vxp").string();
    Game game(options);
    click(game, SectionUiWorldBase);
    click(game, SectionUiLibraryNew);
    click(game, SectionUiLayoutBase + static_cast<int>(SectionLayout::Arena));
    click(game, SectionUiCreate);
    auto& editor = game.sectionEditor();
    require(editor.screen() == SectionEditorScreen::Build && editor.meta().category == SectionCategory::Arena,
            "Arena creation enters an ordinary editable builder with the Arena category");
    const auto authoredWorld = game.world;
    const auto authoredAssets = editor.assets();
    const auto metadata = editor.meta();
    const auto waves = chain(authoredAssets);
    const auto equipment = game.equipment;
    const auto drafts = listSectionDrafts(options.sectionLibrary);
    require(drafts.size() == 1, "creating the arena saves a single recoverable draft");
    const auto draftBytes = read(drafts[0].path);

    press(game, &InputActions::devTest);
    require(editor.screen() == SectionEditorScreen::Test && game.encounters().remaining() == 3,
            "F8 starts the actual template with three unfinished waves");
    approach(game, waves.back()); tick(game, 15);
    require(!game.encounters().active() && game.encounters().monsters().empty(),
            "approaching the last arena spawner cannot skip its linked predecessors");
    position(game, metadata.finish); tick(game, 3);
    require(!editor.testComplete(), "the arena's End is blocked before the waves are cleared");
    walkIntoArena(game, metadata.spawn); tick(game, 8);
    require(!game.encounters().monsters().empty(), "the real starter arena provides viable enemy spawn ground");
    std::optional<glm::ivec3> removableCell;
    for (int z = SectionInteriorMin.z; z < SectionInteriorMax.z && !removableCell; ++z)
        for (int x = SectionInteriorMin.x; x < SectionInteriorMax.x; ++x) {
            const glm::ivec3 cell{x, SectionInteriorMin.y, z};
            if (isSolid(game.world.get(cell))) { removableCell = cell; break; }
        }
    require(removableCell.has_value(), "the arena contains editable cover above the protected floor");
    game.world.set(*removableCell, Block::Air); // Temporary gameplay terrain changes must not enter the draft.
    press(game, &InputActions::devTest);
    require(editor.screen() == SectionEditorScreen::Build && game.world.sameContents(authoredWorld)
            && editor.assets() == authoredAssets && game.equipment == equipment,
            "leaving an unfinished arena test restores authored terrain, spawners, and equipment");
    require(read(drafts[0].path) == draftBytes && !std::filesystem::exists(options.progressPath),
            "an arena playtest cannot rewrite the draft or create permanent combat progress");

    press(game, &InputActions::devTest);
    require(game.encounters().remaining() == 3 && game.encounters().monsters().empty() && !game.encounters().active(),
            "a new F8 test starts every wave fresh after an interrupted attempt");
    const auto initialLoot = availableItems(game);
    walkIntoArena(game, metadata.spawn);
    clearWaves(game, waves, initialLoot);
    position(game, metadata.finish); tick(game, 2);
    require(editor.testComplete(), "clearing all arena waves opens End and passes the test");
    press(game, &InputActions::devAccept);
    require(editor.screen() == SectionEditorScreen::Accept && game.world.sameContents(authoredWorld)
            && editor.assets() == authoredAssets && game.equipment == equipment,
            "accepting a completed arena restores the creative document before publication");
    type(game, "Overgrown arena"); click(game, SectionUiConfirm);
    const auto accepted = listSections(options.sectionLibrary, SectionWorld::LushCaves);
    require(accepted.size() == 1 && accepted[0].meta.category == SectionCategory::Arena && accepted[0].meta.tested
            && accepted[0].path.parent_path().filename() == "arena",
            "a tested arena is accepted into its world's Arena library");
    const auto arenaPath = accepted[0].path;
    const auto arenaBytes = read(arenaPath);
    const auto saved = loadSection(arenaPath);
    require(saved.assets == authoredAssets && saved.world.sameContents(authoredWorld),
            "accepting preserves the arena geometry and individually configured linked spawners");

    World nextWorld(73); buildBlankSection(nextWorld);
    SectionMetadata nextMeta; nextMeta.name = "After the arena"; nextMeta.tested = true;
    const auto nextPath = acceptSection(nextWorld, nextMeta, {}, options.sectionLibrary);
    press(game, &InputActions::levelCreator);
    click(game, LevelUiWorldBase);
    auto& controller = game.levelController();
    const auto indexOf = [&](const std::filesystem::path& path) {
        const auto& sections = controller.sections();
        const auto entry = std::find_if(sections.begin(), sections.end(), [&](const auto& value) { return value.path == path; });
        require(entry != sections.end(), "level creation offers the accepted Arena alongside other section categories");
        return static_cast<int>(entry - sections.begin());
    };
    const int arenaIndex = indexOf(arenaPath), nextIndex = indexOf(nextPath);
    click(game, LevelUiNew);
    click(game, LevelUiSectionBase + arenaIndex); click(game, LevelUiSectionBase + nextIndex);
    click(game, LevelUiPublish);
    const auto levels = listLevels(options.levelLibrary);
    require(levels.size() == 1 && levels[0].definition.sections.size() == 2,
            "an Arena can be published in a level together with a different section type");
    click(game, LevelUiPlay);
    require(controller.screen() == LevelScreen::Play && controller.meta().category == SectionCategory::Arena
            && game.encounters().remaining() == 3,
            "normal level play loads all arena waves without inheriting F8 test clears");
    position(game, metadata.finish); tick(game, 5);
    require(controller.sectionIndex() == 0 && controller.screen() == LevelScreen::Play,
            "normal expedition play also blocks the arena exit until its fights are cleared");
    const auto playLoot = availableItems(game);
    walkIntoArena(game, metadata.spawn);
    clearWaves(game, waves, playLoot);
    require(std::filesystem::is_regular_file(options.progressPath), "real arena clears save permanent player progress");
    position(game, metadata.finish); tick(game, 5);
    require(controller.sectionIndex() == 1 && controller.meta().name == nextMeta.name
            && game.player.feetPosition() == nextMeta.spawn,
            "finishing the published arena transitions to the following section's Start");
    require(read(arenaPath) == arenaBytes, "normal combat never modifies the accepted arena document");
    require(controller.start(game, levels[0].definition) && game.encounters().remaining() == 0,
            "replaying the arena keeps each of its completed trial spawners permanently cleared");
    position(game, metadata.finish); tick(game, 5);
    require(controller.sectionIndex() == 1, "a permanently cleared arena can be crossed without replaying its waves");
}
} // namespace

int main()
{
    char pattern[] = "/tmp/voxel-arena-game-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern);
    int result = 0;
    try { testArena(root); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    std::filesystem::remove_all(root);
    return result;
}
