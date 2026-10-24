#include "game.h"
#include "input.h"
#include "ores.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void tick(Game& game, int frames = 1)
{
    InputActions input; input.active = true;
    for (int i = 0; i < frames; ++i) game.update(input, .1f);
}
void hold(Game& game, int frames)
{
    InputActions input; input.active = input.mineHeld = true;
    for (int i = 0; i < frames; ++i) game.update(input, .1f);
}
void press(Game& game, bool InputActions::* key)
{
    InputActions input; input.active = true; input.*key = true; game.update(input, .1f);
}
void click(Game& game, int id)
{
    InputActions input; input.active = true; input.devClick = id; game.update(input, .1f);
}
void position(Game& game, glm::vec3 feet)
{
    require(game.player.setFeetPosition(game.world, feet), "test player has clear headroom");
    game.camera.setPosition(game.player.eyePosition());
}
void aim(Game& game, glm::vec3 point)
{
    const auto direction = glm::normalize(point - game.camera.position()), old = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                       glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}
void approach(Game& game, glm::ivec3 cell)
{
    position(game, glm::vec3(cell.x + .5f, 9, cell.z + 3.5f));
    aim(game, glm::vec3(cell) + .5f);
}
void mine(Game& game, glm::ivec3 cell)
{
    approach(game, cell);
    for (int i = 0; i < 40 && game.world.get(cell) != Block::Air; ++i) hold(game, 1);
    require(game.world.get(cell) == Block::Air, "holding the pickaxe mines the target ore");
}
std::size_t pickups(const Game& game)
{
    return std::count_if(game.sectionSceneAssets().begin(), game.sectionSceneAssets().end(),
        [](const auto& asset) { return isItemPickup(asset.kind); });
}
glm::vec3 drop(const Game& game, ItemId item)
{
    for (const auto& asset : game.sectionSceneAssets())
        if (isItemPickup(asset.kind) && itemPickupStack(asset).item == item) return asset.position;
    throw std::runtime_error("expected a visible mined item");
}
std::string read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
glm::ivec3 oreCell(std::size_t i) { return {static_cast<int>(i % 6) * 4 - 10, 10, static_cast<int>(i / 6) * 6 - 12}; }

void mappingAndPersistence(const Options& options, const LevelDefinition& level,
                            const std::filesystem::path& section)
{
    const auto authored = read(section);
    Game game(options);
    require(game.levelController().start(game, level), "ore room starts");
    game.equipment.pickaxeTier = 3;
    game.equipment.inventory = {};
    for (std::size_t i = 0; i < oreCatalog().size(); ++i) {
        const auto info = oreCatalog()[i];
        mine(game, oreCell(i));
        require(pickups(game) == i + 1 && itemPickupStack(game.sectionSceneAssets().back()) == ItemStack{info.item, 1},
                "each ore block releases exactly its matching resource item");
    }
    require(read(section) == authored, "mining never rewrites the accepted section file");
    Game reloaded(options);
    require(reloaded.levelController().start(reloaded, level), "mined room reloads");
    require(pickups(reloaded) == oreCatalog().size(), "unclaimed ore drops survive a game restart");
    for (std::size_t i = 0; i < oreCatalog().size(); ++i)
        require(reloaded.world.get(oreCell(i)) == Block::Air, "mined source blocks stay absent after restart");
    require(reloaded.damagePlayer(100), "mined room can trigger a death");
    for (int i = 0; i < 20 && reloaded.health().dead(); ++i) tick(reloaded);
    require(!reloaded.health().dead() && pickups(reloaded) == oreCatalog().size(), "death keeps the room's unclaimed mined rewards");
    for (const auto& info : oreCatalog()) {
        position(reloaded, drop(reloaded, info.item)); tick(reloaded, 15);
        require(reloaded.equipment.inventory.count(info.item) == 1, "walking over mined ore transfers one item to inventory");
    }
    require(pickups(reloaded) == 0, "all gathered resources disappear from the floor");
    Game claimed(options);
    require(claimed.levelController().start(claimed, level) && pickups(claimed) == 0,
            "collected ore cannot return on another expedition");
    for (const auto& info : oreCatalog()) require(claimed.equipment.inventory.count(info.item) == 1, "ore inventory survives reload exactly once");
}

void controls(const Options& options, const LevelDefinition& level)
{
    auto temporary = options; temporary.progressPath.clear();
    Game game(temporary);
    require(game.levelController().start(game, level), "control room starts");
    const auto cell = oreCell(0);
    approach(game, cell);
    hold(game, 2);
    require(game.mining().progress() > 0 && game.world.get(cell) == Block::CoalOre, "mining needs a sustained hold");
    tick(game);
    require(game.mining().progress() == 0, "releasing the mouse cancels mining charge");
    hold(game, 2);
    InputActions sword; sword.active = sword.attack = true; sword.selectedTool = 3;
    game.update(sword, .1f);
    require(game.mining().progress() == 0 && game.world.get(cell) == Block::CoalOre, "switching to the sword clears mining and cannot break ore");
    InputActions pickaxe; pickaxe.active = true; pickaxe.selectedTool = 0;
    game.update(pickaxe, .1f); hold(game, 2);
    press(game, &InputActions::inventory); hold(game, 30);
    require(game.inventoryOpen() && game.world.get(cell) == Block::CoalOre, "opening inventory prevents mining in the background");
    press(game, &InputActions::closeMenu);
    hold(game, 2);
    InputActions paused;
    for (int i = 0; i < 30; ++i) game.update(paused, .1f);
    require(game.world.get(cell) == Block::CoalOre, "losing focus cannot complete mining");

    approach(game, oreCell(4)); hold(game, 30);
    require(game.world.get(oreCell(4)) == Block::SilverOre && game.mining().blocked(), "silver requires an upgraded pickaxe");
    game.equipment.pickaxeTier = 2;
    mine(game, oreCell(4));
    approach(game, cell);
    position(game, glm::vec3(cell.x + .5f, 9, cell.z + 7.5f)); aim(game, glm::vec3(cell) + .5f);
    hold(game, 30);
    require(game.world.get(cell) == Block::CoalOre && !game.mining().target(), "distant ore is outside pickaxe reach");
    approach(game, cell);
    const auto wall = cell + glm::ivec3(0, 0, 2);
    game.world.set(wall, Block::Stone); hold(game, 30);
    require(game.world.get(cell) == Block::CoalOre && !game.mining().target(), "solid terrain blocks mining rays");
    game.world.set(wall, Block::Air);
    const HubAsset pillar{HubAssetKind::TimberPillar, glm::vec3(wall) + glm::vec3(.5f, 0, .5f)};
    game.beginSectionLoot(std::span(&pillar, 1), true);
    hold(game, 30);
    require(game.world.get(cell) == Block::CoalOre && !game.mining().target(), "visible model collision blocks mining behind a prop");
    game.beginSectionLoot({}, true);
    mine(game, cell);
    require(game.world.get(wall) == Block::Air, "mining removes only the ore target");

    Game hub(temporary);
    buildBlankSection(hub.world); hub.player.setAssetCollision({});
    hub.world.set(cell, Block::CoalOre); approach(hub, cell);
    InputActions swing; swing.active = swing.mineHeld = true;
    hub.updateCombat(swing, .1f);
    require(hub.mining().swing() > 0, "the held pickaxe can freely swing in the hub");
    hold(hub, 30);
    require(hub.world.get(cell) == Block::CoalOre && hub.equipment.lootProgress.empty(), "the hub remains protected from mining");
}

void fullBagAndSaveFailure(const Options& options, const LevelDefinition& level, const std::filesystem::path& root)
{
    auto fullOptions = options; fullOptions.progressPath = (root / "full.vxp").string();
    Game full(fullOptions);
    for (auto& slot : full.equipment.inventory.slots) slot = {ItemId::IronOre, Inventory::StackLimit};
    require(full.levelController().start(full, level), "full-bag room starts");
    mine(full, oreCell(0));
    position(full, drop(full, ItemId::Coal)); tick(full, 20);
    require(pickups(full) == 1 && full.equipment.inventory.count(ItemId::Coal) == 0, "a full bag leaves mined ore on the ground");
    const auto saved = read(fullOptions.progressPath);
    tick(full, 20);
    require(read(fullOptions.progressPath) == saved, "a full bag does not repeatedly save failed pickup attempts");
    full.equipment.inventory.slots[0] = {};
    tick(full, 10);
    require(full.equipment.inventory.count(ItemId::Coal) == 1 && pickups(full) == 0, "making room allows the waiting ore to be collected");

    auto failureOptions = options; failureOptions.progressPath = (root / "missing" / "profile.vxp").string();
    Game failure(failureOptions);
    require(failure.levelController().start(failure, level), "failed-save room starts");
    approach(failure, oreCell(0)); hold(failure, 30);
    require(failure.world.get(oreCell(0)) == Block::CoalOre && pickups(failure) == 0
            && failure.equipment.lootProgress.empty() && failure.miningMessage().starts_with("NOT SAVED:"),
            "a failed profile write leaves the source and inventory untouched with clear feedback");
    tick(failure, 20);
    require(failure.miningMessage().empty(), "mining save feedback clears after its brief display time");
    std::filesystem::create_directories(std::filesystem::path(failureOptions.progressPath).parent_path());
    mine(failure, oreCell(0));
    require(pickups(failure) == 1 && loadEquipment(failureOptions.progressPath).lootProgress.size() == 1,
            "after storage recovers a new completed mining attempt releases one durable reward");
}

void builderPreservation(const Options& options, const std::filesystem::path& section)
{
    auto dev = options; dev.dev = true;
    Game builder(dev);
    const auto before = builder.equipment;
    const auto profile = read(options.progressPath), accepted = read(section);
    click(builder, SectionUiWorldBase);
    const auto& entries = builder.sectionEditor().entries();
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) { return entry.path == section; });
    require(found != entries.end(), "ore room appears in the creative library");
    click(builder, SectionUiLibraryEntryBase + static_cast<int>(found - entries.begin()));
    const auto authored = builder.world;
    press(builder, &InputActions::devTest);
    require(builder.world.get(oreCell(0)) == Block::CoalOre, "F8 starts with original ores despite permanently mined player progress");
    mine(builder, oreCell(0));
    position(builder, drop(builder, ItemId::Coal)); tick(builder, 15);
    require(builder.equipment.inventory.count(ItemId::Coal) == before.inventory.count(ItemId::Coal) + 1,
            "F8 mining uses a temporary inventory");
    require(builder.damagePlayer(100), "F8 mining test can die");
    for (int i = 0; i < 20 && builder.health().dead(); ++i) tick(builder);
    require(builder.world.get(oreCell(0)) == Block::Air && pickups(builder) == 0,
            "F8 death keeps the current attempt's mined and collected ore absent");
    press(builder, &InputActions::save);
    auto drafts = listSectionDrafts(options.sectionLibrary);
    require(drafts.size() == 1 && loadSection(drafts.front().path).world.sameContents(authored),
            "saving during a walk test writes the original authored ore blocks");
    position(builder, builder.sectionEditor().meta().finish); tick(builder);
    require(builder.sectionEditor().testComplete(), "a mining test can still complete at End");
    tick(builder, 210);
    require(loadSection(drafts.front().path).world.sameContents(authored), "test completion autosave also preserves authored ores");
    press(builder, &InputActions::devTest);
    require(builder.world.sameContents(authored) && builder.equipment == before,
            "returning to creative restores the complete authored world and real equipment");
    press(builder, &InputActions::devTest);
    require(builder.world.get(oreCell(0)) == Block::CoalOre && pickups(builder) == 0,
            "every new F8 run starts fresh without leftover mining pickups");
    mine(builder, oreCell(0));
    press(builder, &InputActions::devToggle);
    require(!builder.devMode() && builder.equipment == before && read(options.progressPath) == profile
            && read(section) == accepted, "leaving the builder during a test preserves profile and accepted section files");
    require(loadSection(drafts.front().path).world.sameContents(authored), "leaving a test cannot save mined holes in the draft");
}

void minedMarkerRespawns(const Options& options, const std::filesystem::path& root)
{
    World room; buildBlankSection(room);
    const glm::ivec3 startOre{0,9,20}, checkpointOre{0,9,0};
    room.set(startOre, Block::CoalOre); room.set(checkpointOre, Block::CopperOre);
    SectionMetadata meta; meta.name = "Mineable marker supports"; meta.tested = true;
    meta.spawn = {.5f,10,20.5f}; meta.checkpoint = {.5f,10,.5f};
    const auto section = acceptSection(room, meta, {}, options.sectionLibrary);
    const auto authored = read(section);
    LevelDefinition level; level.name = "Marker fallback";
    level.sections = {section.lexically_relative(options.sectionLibrary)};
    auto liveOptions = options; liveOptions.progressPath = (root / "marker-respawn.vxp").string();
    Game live(liveOptions);
    require(live.levelController().start(live, level), "mineable marker room starts");

    const auto deathAndReturn = [](Game& game) {
        require(game.damagePlayer(100), "marker fixture can die");
        for (int i = 0; i < 20 && game.health().dead(); ++i) tick(game);
        require(!game.health().dead() && game.player.grounded(), "marker fallback restores a supported live player");
    };
    const auto checkStart = [&](Game& game) {
        mine(game, startOre);
        position(game, {4.5f,9,20.5f});
        press(game, &InputActions::respawn);
        require(game.player.feetPosition() == glm::vec3(.5f,9,20.5f)
                && game.world.get(startOre) == Block::Air && pickups(game) == 1,
                "manual R lands below a mined Start without restoring its ore or losing the unclaimed drop");
        deathAndReturn(game);
        require(game.player.feetPosition() == glm::vec3(.5f,9,20.5f)
                && game.world.get(startOre) == Block::Air && pickups(game) == 1,
                "death uses the same surviving floor below a mined Start");
    };
    const auto checkCheckpoint = [&](Game& game) {
        position(game, *meta.checkpoint); tick(game);
        mine(game, checkpointOre);
        position(game, {4.5f,9,.5f});
        press(game, &InputActions::respawn);
        require(game.player.feetPosition() == glm::vec3(.5f,9,.5f)
                && game.world.get(checkpointOre) == Block::Air && pickups(game) == 2,
                "manual R keeps the checkpoint and lands below its mined support");
        deathAndReturn(game);
        require(game.player.feetPosition() == glm::vec3(.5f,9,.5f)
                && game.world.get(checkpointOre) == Block::Air && pickups(game) == 2,
                "checkpoint death keeps both mined blocks absent and both unclaimed rewards");
    };
    const auto removeNearbyFloors = [&](Game& game) {
        for (const int centerZ : {0,20})
            for (int z = centerZ - 3; z <= centerZ + 3; ++z)
                for (int x = -3; x <= 3; ++x)
                    for (int y = 0; y < 10; ++y) game.world.set({x,y,z}, Block::Air);
        position(game, {8.5f,9,.5f});
    };
    checkStart(live); checkCheckpoint(live);
    require(live.levelController().checkpointReached(), "lower checkpoint fallback retains the reached checkpoint");
    Game reload(liveOptions);
    require(reload.levelController().start(reload, level), "a room with mined marker supports reloads");
    press(reload, &InputActions::respawn);
    require(reload.player.feetPosition() == glm::vec3(.5f,9,20.5f) && pickups(reload) == 2,
            "manual reset after profile reload finds the surviving Start floor");
    removeNearbyFloors(live);
    press(live, &InputActions::respawn);
    require(!live.levelActive() && live.equipment.lootProgress.size() == 2 && !live.health().dead(),
            "when neither marker has a surviving floor, manual reset safely returns to the village");

    auto dev = liveOptions; dev.dev = true;
    Game builder(dev);
    click(builder, SectionUiWorldBase);
    const auto& entries = builder.sectionEditor().entries();
    const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& value) { return value.path == section; });
    require(entry != entries.end(), "marker fixture appears in the creative library");
    click(builder, SectionUiLibraryEntryBase + static_cast<int>(entry - entries.begin()));
    press(builder, &InputActions::devTest);
    checkStart(builder); checkCheckpoint(builder);
    require(builder.sectionEditor().checkpointReached(), "F8 retains the reached checkpoint after losing its ore support");
    removeNearbyFloors(builder);
    press(builder, &InputActions::respawn);
    require(builder.sectionEditor().screen() == SectionEditorScreen::Build && builder.world.sameContents(room)
            && !builder.health().dead(), "an F8 room without safe respawn ground returns to the preserved authored world");
    require(read(section) == authored, "marker recovery never rewrites the accepted world");
}
} // namespace

int main()
{
    char temporary[] = "/tmp/voxel-mining-game-XXXXXX";
    if (!mkdtemp(temporary)) return 1;
    const std::filesystem::path root(temporary), library = root / "sections";
    int result = 0;
    try {
        World room; buildBlankSection(room);
        for (std::size_t i = 0; i < oreCatalog().size(); ++i) room.set(oreCell(i), oreCatalog()[i].block);
        SectionMetadata metadata; metadata.name = "Ore gallery"; metadata.tested = true;
        const auto section = acceptSection(room, metadata, {}, library);
        LevelDefinition level; level.name = "Mining test"; level.sections = {section.lexically_relative(library)};
        Options options; options.sectionLibrary = library.string(); options.levelLibrary = (root / "levels").string();
        options.progressPath = (root / "progress.vxp").string();
        mappingAndPersistence(options, level, section);
        controls(options, level);
        fullBagAndSaveFailure(options, level, root);
        builderPreservation(options, section);
        minedMarkerRespawns(options, root);
        std::cout << "Mining game tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    std::filesystem::remove_all(root);
    return result;
}
