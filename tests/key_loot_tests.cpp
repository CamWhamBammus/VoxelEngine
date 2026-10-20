#include "game.h"
#include "input.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void press(Game& game, bool InputActions::* key)
{
    InputActions actions;
    actions.active = true;
    actions.*key = true;
    game.update(actions);
}

void click(Game& game, int id)
{
    InputActions actions;
    actions.active = true;
    actions.devClick = id;
    game.update(actions);
}

void take(Game& game, int row)
{
    InputActions actions;
    actions.active = true;
    actions.lootClick = row;
    game.update(actions);
}

void close(Game& game)
{
    if (game.inventoryOpen()) press(game, &InputActions::closeMenu);
}

std::string read(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int keys(const Game& game) { return game.equipment.inventory.count(ItemId::Key); }

struct Fixture {
    std::filesystem::path root, sections, section, emptySection;
    SectionMetadata meta;
    std::vector<HubAsset> assets{
        {HubAssetKind::KeyPickup, {-12.5f,9,.5f}, 0, 0},
        {HubAssetKind::LootChest, {-6.5f,9,.5f}, 0, 0,
            {{ItemId::Key,2}, {ItemId::CopperOre,12}, {ItemId::Ruby,7}}},
        {HubAssetKind::CrystalChest, {.5f,9,.5f}, 0, 0,
            {{ItemId::Key,1}, {ItemId::IronOre,9}}},
        {HubAssetKind::IronboundChest, {6.5f,9,.5f}, 0, 0,
            {{ItemId::GoldOre,10}, {ItemId::Emerald,3}}},
        {HubAssetKind::IronboundChest, {12.5f,9,.5f}, 1, 0,
            {{ItemId::MithrilOre,2}}},
    };
    LevelDefinition level;

    Fixture()
    {
        char pattern[] = "/tmp/voxel-key-loot-XXXXXX";
        if (!mkdtemp(pattern)) throw std::runtime_error("Cannot create key test folder");
        root = pattern;
        sections = root / "sections";
        World world;
        buildBlankSection(world);
        meta.name = "Authored key room";
        meta.spawn = {.5f,9,20.5f};
        meta.finish = {.5f,9,-20.5f};
        meta.checkpoint = glm::vec3{20.5f,9,12.5f};
        meta.tested = true;
        section = acceptSection(world, meta, assets, sections);
        auto empty = meta;
        empty.name = "Following room";
        emptySection = acceptSection(world, empty, {}, sections);
        level.name = "Key expedition";
        level.number = 1;
        level.sections = {section.lexically_relative(sections), emptySection.lexically_relative(sections)};
    }

    ~Fixture()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    Options options(std::string_view name) const
    {
        Options result;
        result.sectionLibrary = sections.string();
        result.levelLibrary = (root / "levels").string();
        result.progressPath = (root / (std::string(name) + ".vxp")).string();
        return result;
    }

    std::string record(std::size_t index) const
    {
        return section.lexically_relative(sections).generic_string() + "/asset-" + std::to_string(index);
    }

    void start(Game& game) const
    {
        close(game);
        require(game.levelController().start(game, level), "key expedition must load");
    }

    void interact(Game& game, std::size_t asset) const
    {
        close(game);
        const auto& model = assets.at(asset);
        require(game.player.setFeetPosition(game.world, model.position + glm::vec3(0,0,isItemPickup(model.kind) ? .7f : 2.5f)),
                "key/chest approach must fit the player");
        game.camera.setPosition(game.player.eyePosition());
        const auto point = model.position + glm::vec3(0, isKeyPickup(model.kind) ? .3f : .65f, 0);
        const auto next = glm::normalize(point - game.camera.position());
        const auto previous = game.camera.forward();
        game.camera.rotate(glm::degrees(std::atan2(next.z,next.x) - std::atan2(previous.z,previous.x)),
                           glm::degrees(std::asin(next.y) - std::asin(previous.y)));
        if (isItemPickup(model.kind)) {
            InputActions walk; walk.active = true;
            for (int frame = 0; frame < 3; ++frame) game.update(walk, .1f);
        } else press(game, &InputActions::interact);
    }

    bool pickupVisible(const Game& game) const
    {
        return std::any_of(game.sectionSceneAssets().begin(), game.sectionSceneAssets().end(),
            [](const HubAsset& asset) { return isKeyPickup(asset.kind); });
    }

    void finish(Game& game) const
    {
        close(game);
        require(game.player.setFeetPosition(game.world, meta.finish), "section end must fit the player");
        InputActions idle;
        idle.active = true;
        for (int frame = 0; frame < 5; ++frame) game.update(idle, .1f);
    }
};

void persistentUnlocks(const Fixture& f)
{
    const auto options = f.options("persistent");
    Game game(options);
    f.start(game);
    const auto beforeDenied = game.equipment;
    f.interact(game, 3);
    require(!game.chestOpen() && game.equipment == beforeDenied,
            "an ironbound chest rejects opening without a key and changes no state");

    f.interact(game, 0);
    require(keys(game) == 1 && !f.pickupVisible(game),
            "walking over a placed key collects it and removes its visible pickup");
    require(game.player.setFeetPosition(game.world, f.assets[0].position),
            "a collected key no longer leaves collision in the section");
    press(game, &InputActions::respawn);
    require(keys(game) == 1 && !f.pickupVisible(game),
            "checkpoint respawns retain the held key without duplicating its pickup");

    f.interact(game, 3);
    require(game.chestOpen() && keys(game) == 0,
            "one generic key unlocks an ironbound chest and is consumed exactly once");
    require(game.equipment.lootProgress.at(f.record(3)).unlocked
            && game.equipment.lootProgress.at(f.record(0)).taken[0] == 1,
            "unlock and spent-key source are recorded together");
    take(game, 0);
    require(game.equipment.inventory.count(ItemId::GoldOre) == 10
            && game.chestContents()[0].count == 0 && game.chestContents()[1].count == 3,
            "ordinary loot is claimed individually while uncollected stacks remain");
    close(game);
    f.interact(game, 4);
    require(!game.chestOpen() && keys(game) == 0, "a consumed key cannot unlock a second chest");
    press(game, &InputActions::returnToHub);
    require(keys(game) == 0 && game.equipment.inventory.count(ItemId::GoldOre) == 10,
            "claimed ore returns to the hub while section keys cannot");

    Game resumed(options);
    auto otherLevel = f.level;
    otherLevel.number = 9;
    require(resumed.levelController().start(resumed, otherLevel), "the same section loads in another level");
    require(!f.pickupVisible(resumed), "a spent key source stays gone after profile reload and another level entry");
    f.interact(resumed, 3);
    require(resumed.chestOpen() && keys(resumed) == 0 && resumed.chestContents()[0].count == 0
            && resumed.chestContents()[1].count == 3,
            "section identity preserves unlocking and partial claims across level numbers and game restarts");
    const auto saved = resumed.equipment;
    close(resumed);
    press(resumed, &InputActions::restartCrossing);
    f.interact(resumed, 3);
    require(resumed.chestOpen() && resumed.equipment == saved && resumed.chestContents()[0].count == 0,
            "restarting a level does not replenish already claimed items or charge another key");
}

void unusedKeys(const Fixture& f)
{
    const auto options = f.options("unused");
    Game game(options);
    f.start(game);
    f.interact(game, 0);
    f.interact(game, 1);
    take(game, 0);
    take(game, 1);
    f.interact(game, 2);
    take(game, 0);
    require(keys(game) == 4, "keys from a pickup, wooden chest and crystal chest share the inventory");
    require(loadEquipment(options.progressPath).inventory.count(ItemId::Key) == 0,
            "saving other loot never puts transient section keys into the persistent inventory");
    close(game);
    press(game, &InputActions::respawn);
    require(keys(game) == 4, "checkpoint returns retain keys from every source");
    press(game, &InputActions::returnToHub);
    require(keys(game) == 0, "unused keys are removed when returning to the hub");

    f.start(game);
    require(f.pickupVisible(game), "an unused pickup key returns on the next section visit");
    f.interact(game, 1);
    require(game.chestContents()[0].count == 2 && game.chestContents()[1].count == 0,
            "unused chest keys return while ordinary ore claimed beside them stays gone");
    f.interact(game, 2);
    require(game.chestContents()[0].count == 1, "unused crystal-chest keys also return");
    close(game);
    f.interact(game, 0);
    press(game, &InputActions::restartCrossing);
    require(keys(game) == 0 && f.pickupVisible(game), "a full restart returns unused keys to their sources");

    f.interact(game, 0);
    f.finish(game);
    require(game.levelController().sectionIndex() == 1 && keys(game) == 0,
            "keys cannot cross the transition into the next section");
    f.start(game);
    require(f.pickupVisible(game), "transitioned-away unused keys remain collectible on return");
    f.interact(game, 0);
    // A simulated process restart must recover a held but unspent source.
    Game afterQuit(options);
    f.start(afterQuit);
    require(keys(afterQuit) == 0 && f.pickupVisible(afterQuit),
            "quitting with a key does not save the key or permanently remove its source");

    auto singleSection = f.level;
    singleSection.sections.resize(1);
    close(game);
    require(game.levelController().start(game, singleSection), "single-section completion fixture loads");
    f.interact(game, 0);
    f.finish(game);
    require(game.levelController().screen() == LevelScreen::Complete && keys(game) == 0,
            "the final completion screen also clears unused section keys");
}

void partiallyUsedKeyStack(const Fixture& f)
{
    Game game(f.options("partial-keys"));
    f.start(game);
    f.interact(game, 1);
    take(game, 0);
    require(keys(game) == 2, "an authored key stack can be collected together");
    f.interact(game, 4);
    require(game.chestOpen() && keys(game) == 1, "a key from a regular chest can unlock either ironbound chest");
    require(game.equipment.lootProgress.at(f.record(1)).taken[0] == 1,
            "only the used key quantity becomes permanently claimed");
    close(game);
    press(game, &InputActions::returnToHub);
    f.start(game);
    f.interact(game, 1);
    require(keys(game) == 0 && game.chestContents()[0].count == 1,
            "returning a partly used stack restores only its unused key");
    f.interact(game, 4);
    require(game.chestOpen() && keys(game) == 0, "the previously unlocked chest opens without consuming the remaining key");
}

void capacityAndClaims(const Fixture& f)
{
    const auto options = f.options("capacity");
    Game game(options);
    f.start(game);
    for (auto& slot : game.equipment.inventory.slots) slot = {ItemId::Coal, Inventory::StackLimit};
    const auto full = game.equipment;
    f.interact(game, 0);
    require(game.equipment == full && f.pickupVisible(game), "a full bag leaves a pickup key and all its state untouched");
    f.interact(game, 3);
    require(!game.chestOpen() && game.equipment == full, "a full bag cannot open a locked chest without a key");
    f.interact(game, 1);
    take(game, 0);
    require(keys(game) == 0 && game.chestContents()[0].count == 2 && game.equipment == full,
            "keys in a regular chest also remain when the bag has no space");
    close(game);
    game.equipment.inventory.slots[0] = {};
    f.interact(game, 0);
    f.interact(game, 3);
    require(game.chestOpen() && keys(game) == 0, "using a key frees its inventory slot");
    game.equipment.inventory.slots[0] = {ItemId::GoldOre, 97};
    press(game, &InputActions::lootAll);
    require(game.equipment.inventory.slots[0].count == 99 && game.chestContents()[0].count == 8
            && game.chestContents()[1].count == 3,
            "a partial collection claims only the quantity that fits and leaves other resources");
    require(game.equipment.lootProgress.at(f.record(3)).taken[0] == 2,
            "persistent claims record the actual partial transfer");
    const auto after = game.equipment;
    const auto contents = game.chestContents();
    press(game, &InputActions::lootAll);
    require(game.equipment == after && game.chestContents() == contents,
            "repeated Take All on a full bag neither duplicates nor loses items");
    Game resumed(options);
    f.start(resumed);
    f.interact(resumed, 3);
    require(resumed.chestOpen() && resumed.chestContents() == contents,
            "partial chest claims survive restarting the game");
}

void failedUnlockWrite(const Fixture& f)
{
    const auto options = f.options("write-failure");
    const std::filesystem::path profile = options.progressPath;
    Game game(options);
    f.start(game);
    f.interact(game, 0);
    // Ensure there is a real save to preserve even if pickup collection itself
    // does not need a permanent write.
    saveEquipment(game.equipment, profile);
    const auto originalBytes = read(profile);
    const auto before = game.equipment;
    const auto backup = profile.string() + ".backup";
    std::filesystem::rename(profile, backup);
    std::filesystem::create_directory(profile);
    { std::ofstream keep(profile / "keep"); keep << "preserve"; }
    f.interact(game, 3);
    require(!game.chestOpen() && keys(game) == 1 && game.equipment == before,
            "a failed unlock save preserves the held key, source claim and lock");
    require(read(profile / "keep") == "preserve" && read(backup) == originalBytes,
            "a failed unlock write does not damage either existing destination or old profile");
    std::filesystem::remove_all(profile);
    std::filesystem::rename(backup, profile);
    f.interact(game, 3);
    require(game.chestOpen() && keys(game) == 0 && game.equipment.lootProgress.at(f.record(3)).unlocked,
            "repairing the save destination allows exactly one normal unlock");
}

void temporaryPlaytest(const Fixture& f)
{
    auto options = f.options("temporary");
    options.dev = true;
    Equipment initial;
    saveEquipment(initial, options.progressPath);
    const auto savedBytes = read(options.progressPath);
    Game game(options);
    const auto before = game.equipment;
    click(game, SectionUiWorldBase);
    const auto& entries = game.sectionEditor().entries();
    const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& item) { return item.path == f.section; });
    require(entry != entries.end(), "key room is available in the section library");
    click(game, SectionUiLibraryEntryBase + static_cast<int>(entry - entries.begin()));
    press(game, &InputActions::devTest);
    require(game.sectionEditor().screen() == SectionEditorScreen::Test, "F8 starts the temporary key test");
    f.interact(game, 0);
    press(game, &InputActions::respawn);
    require(keys(game) == 1 && !f.pickupVisible(game)
            && game.player.setFeetPosition(game.world, f.assets[0].position),
            "F8 checkpoint reset preserves the collected key and keeps its invisible collision removed");
    f.interact(game, 3);
    require(game.chestOpen() && keys(game) == 0, "F8 permits temporary key collection and unlocking");
    press(game, &InputActions::lootAll);
    require(game.equipment.inventory.count(ItemId::GoldOre) == 10
            && !game.equipment.lootProgress.empty() && read(options.progressPath) == savedBytes,
            "F8 loot and claim records remain entirely in memory");
    close(game);
    press(game, &InputActions::devTest);
    require(game.equipment == before && read(options.progressPath) == savedBytes,
            "leaving F8 restores the original inventory and claim ledger");
}
} // namespace

int main()
{
    try {
        Fixture fixture;
        persistentUnlocks(fixture);
        unusedKeys(fixture);
        partiallyUsedKeyStack(fixture);
        capacityAndClaims(fixture);
        failedUnlockWrite(fixture);
        temporaryPlaytest(fixture);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Section keys, persistent claims and failure recovery passed\n";
    return 0;
}
