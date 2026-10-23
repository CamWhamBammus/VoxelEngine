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
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void tick(Game& game, int frames = 1, float seconds = .1f)
{
    InputActions input; input.active = true;
    for (int i = 0; i < frames; ++i) game.update(input, seconds);
}
void press(Game& game, bool InputActions::*key)
{
    InputActions input; input.active = true; input.*key = true; game.update(input, .1f);
}
void click(Game& game, int id)
{
    InputActions input; input.active = true; input.devClick = id; game.update(input, .1f);
}
void position(Game& game, glm::vec3 feet)
{
    require(game.player.setFeetPosition(game.world, feet), "fixture player position must fit");
    game.camera.setPosition(game.player.eyePosition());
}
void aim(Game& game, glm::vec3 target)
{
    const auto direction = glm::normalize(target - game.camera.position()), old = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                       glm::degrees(std::asin(direction.y)-std::asin(old.y)));
}
std::string read(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
std::size_t pickups(const Game& game)
{
    return std::count_if(game.sectionSceneAssets().begin(), game.sectionSceneAssets().end(),
        [](const HubAsset& asset) { return isItemPickup(asset.kind); });
}
void respawn(Game& game)
{
    for (int i = 0; i < 20 && game.health().dead(); ++i) tick(game);
    require(!game.health().dead(), "death completes and returns control");
}
void activate(Game& game)
{
    position(game, {5.5f,9,.5f}); tick(game);
    require(game.encounters().active(), "walking near the spawner activates it without interaction");
    for (int i = 0; i < 30 && game.encounters().monsters().empty(); ++i) tick(game);
    require(game.encounters().monsters().size() == 1, "one authored skeleton spawns into the section");
}
void attackTarget(Game& game, int selectedSlot)
{
    require(!game.encounters().monsters().empty(), "an enemy remains to target");
    const auto target = game.encounters().monsters().front().position;
    bool placed = false;
    for (const auto offset : {glm::vec3(0,0,1.7f),glm::vec3(1.7f,0,0),glm::vec3(0,0,-1.7f),glm::vec3(-1.7f,0,0)})
        if (game.player.setFeetPosition(game.world,target+offset)) { placed = true; break; }
    require(placed, "fixture has space in sword reach of the skeleton");
    game.camera.setPosition(game.player.eyePosition());
    aim(game,target+glm::vec3(0,.9f,0));
    InputActions input; input.active = input.attack = true; input.selectedTool = selectedSlot;
    game.update(input,.1f);
}
void defeat(Game& game)
{
    for (int i = 0; i < 80 && game.encounters().remaining(); ++i) {
        if (game.encounters().monsters().empty()) tick(game);
        else attackTarget(game,3);
        require(!game.health().dead(), "the controlled sword fight should finish before player death");
    }
    require(game.encounters().remaining() == 0 && game.encounters().monsters().empty(),
            "sword attacks defeat all enemies and clear the encounter");
}
void collect(Game& game)
{
    tick(game,12); // Let the reward emission finish before walking over drops.
    for (int i = 0; i < 10 && pickups(game); ++i) {
        const auto scene = game.sectionSceneAssets();
        const auto found = std::find_if(scene.begin(),scene.end(),[](const auto& asset) { return isItemPickup(asset.kind); });
        require(found != scene.end(), "visible reward source exists");
        const auto feet = found->position;
        position(game,feet); tick(game,10);
    }
    require(pickups(game) == 0, "walking over every reward collects it into inventory");
}
glm::vec3 dropPosition(const Game& game, ItemId item)
{
    const auto scene = game.sectionSceneAssets();
    const auto found = std::find_if(scene.begin(),scene.end(),[&](const auto& asset) {
        return isItemPickup(asset.kind) && itemPickupStack(asset).item == item;
    });
    require(found != scene.end(), "fixture reward item remains visible");
    return found->position;
}

void playerHitChecks(const Options& options, const LevelDefinition& level, const SectionMetadata& meta)
{
    Options temporary = options; temporary.progressPath.clear();
    Game game(temporary);
    require(game.levelController().start(game,level), "player-hit fixture starts with a fresh encounter");
    activate(game);
    const auto enemy = game.encounters().monsters().front().position;
    bool placed = false;
    for (const auto offset : {glm::vec3(0,0,1.6f),glm::vec3(1.6f,0,0),glm::vec3(0,0,-1.6f),glm::vec3(-1.6f,0,0)})
        if (game.player.setFeetPosition(game.world,enemy+offset)) { placed = true; break; }
    require(placed, "player-hit fixture has room within the skeleton's reach");
    game.camera.setPosition(game.player.eyePosition());
    aim(game,enemy+glm::vec3(0,.9f,0));
    const auto aimBefore = game.camera.forward();
    auto beforeImpact = game.player.feetPosition();
    for (int i = 0; i < 180 && game.health().current() == Health::Maximum; ++i) {
        beforeImpact = game.player.feetPosition();
        tick(game,1,1.0f/60);
    }
    require(game.health().current() == Health::Maximum-16 && game.health().damageFlash() > .99f,
            "a skeleton's real slash damages the player and starts immediate visible hit feedback");
    const auto direction = game.playerHitDirection();
    auto away = beforeImpact-game.encounters().monsters().front().position; away.y = 0;
    require(std::abs(glm::length(direction)-1) < .0001f && direction.y == 0
            && glm::dot(direction,glm::normalize(away)) > .99f,
            "player feedback points horizontally away from the skeleton that landed the hit");
    require(game.viewCamera().position() == game.camera.position()
            && glm::length(game.viewCamera().forward()-aimBefore) > .001f
            && game.camera.forward() == aimBefore,
            "a real incoming hit kicks only the rendered view without moving its origin or changing weapon aim");
    const auto firstStep = game.player.feetPosition();
    tick(game,3,1.0f/60);
    require(glm::dot(game.player.feetPosition()-firstStep,direction) > .01f
            && glm::dot(game.player.feetPosition()-beforeImpact,direction) < .4f,
            "knockback continues smoothly over following frames instead of teleporting the whole distance");

    const auto pausedFeet = game.player.feetPosition();
    const auto pausedFlash = game.health().damageFlash();
    const auto pausedView = game.viewCamera().forward();
    InputActions inactive;
    for (int i = 0; i < 10; ++i) game.update(inactive,.1f);
    require(game.player.feetPosition() == pausedFeet && game.health().damageFlash() == pausedFlash
            && game.playerHitDirection() == direction && game.viewCamera().forward() == pausedView,
            "losing focus pauses the player's shove, hit flash, and view kick together");
    press(game,&InputActions::inventory);
    const auto menuFeet = game.player.feetPosition();
    const auto menuFlash = game.health().damageFlash();
    tick(game,12);
    require(game.inventoryOpen() && game.player.feetPosition() == menuFeet
            && game.health().damageFlash() == menuFlash && game.playerHitDirection() == direction
            && game.viewCamera().forward() == game.camera.forward(),
            "inventory pauses player-hit feedback and uses a steady view while the menu is open");
    press(game,&InputActions::closeMenu);
    float previousFlash = game.health().damageFlash();
    for (int i = 0; i < 24; ++i) {
        tick(game,1,1.0f/60);
        require(game.health().current() == Health::Maximum-16 && game.health().damageFlash() <= previousFlash,
                "one enemy attack cannot repeatedly damage the player or restart its fading feedback");
        previousFlash = game.health().damageFlash();
    }
    require(std::abs(glm::dot(game.player.feetPosition()-beforeImpact,direction)-.4f) < .01f
            && game.health().damageFlash() == 0 && game.viewCamera().forward() == aimBefore
            && game.camera.forward() == aimBefore,
            "the small shove finishes and the view returns exactly to the unchanged aiming direction");

    require(game.damagePlayer(1,direction), "fixture can start fresh feedback before restoring health");
    game.restoreHealth();
    require(game.health().current() == Health::Maximum && game.health().damageFlash() == 0
            && game.playerHitDirection() == glm::vec3(0) && game.viewCamera().forward() == game.camera.forward(),
            "restoring health clears transient player hit feedback");
    require(game.damagePlayer(100,direction), "player-hit fixture can die with an active hit direction");
    respawn(game);
    const auto respawnFeet = game.player.feetPosition();
    tick(game,4);
    require(respawnFeet == meta.spawn && game.player.feetPosition() == meta.spawn
            && game.health().damageFlash() == 0 && game.playerHitDirection() == glm::vec3(0),
            "respawning clears the old hit direction and cannot carry a shove into the section entrance");
    require(game.damagePlayer(1,direction), "fixture can take damage just before changing sections");
    auto safeLevel = level; safeLevel.sections.erase(safeLevel.sections.begin());
    require(game.levelController().start(game,safeLevel), "the safe section loads after the damaged encounter");
    tick(game,4);
    require(game.health().damageFlash() == 0 && game.playerHitDirection() == glm::vec3(0)
            && game.player.feetPosition() == meta.spawn && game.viewCamera().forward() == game.camera.forward(),
            "loading a different section drops the previous section's hit flash, view kick, and knockback");
}

void auditChecks(const Options& options, const LevelDefinition& level, const World& room,
                 const SectionMetadata& meta, const HubAsset& spawner, const std::filesystem::path& root)
{
    const auto record = level.sections[0].generic_string()+"/asset-0";
    Options failureOptions = options;
    failureOptions.progressPath = (root/"unavailable-folder"/"progress.vxp").string();
    Game failure(failureOptions);
    require(failure.levelController().start(failure,level), "failed-save fixture starts");
    activate(failure);
    const auto beforeMenu = failure.encounters().monsters().front();
    const auto hpBeforeMenu = failure.health().current();
    press(failure,&InputActions::inventory); tick(failure,12);
    require(failure.inventoryOpen() && failure.encounters().monsters().front().position == beforeMenu.position
            && failure.encounters().monsters().front().seconds == beforeMenu.seconds && failure.health().current() == hpBeforeMenu,
            "the inventory menu freezes active enemies and their attack timers");
    press(failure,&InputActions::closeMenu);
    for (int i = 0; i < 80 && failure.encounters().pendingCompletions().empty(); ++i)
        attackTarget(failure,3);
    require(failure.encounters().monsters().empty() && failure.encounters().remaining() == 1
            && !failure.encounters().pendingCompletions().empty() && pickups(failure) == 0
            && !failure.equipment.lootProgress.contains(record),
            "failed completion saves keep the encounter pending and hide rewards without altering memory progress");
    position(failure,meta.finish); tick(failure,15);
    require(failure.levelController().sectionIndex() == 0, "a failed durable clear cannot open the End");
    std::filesystem::create_directories(std::filesystem::path(failureOptions.progressPath).parent_path());
    position(failure,meta.spawn); tick(failure,15);
    require(failure.encounters().remaining() == 0 && pickups(failure) == 2
            && loadEquipment(failureOptions.progressPath).lootProgress.at(record).unlocked,
            "completion retries after storage recovers and releases rewards only after a successful write");

    Options partialOptions = options; partialOptions.progressPath = (root/"partial.vxp").string();
    Game partial(partialOptions);
    for (auto& slot : partial.equipment.inventory.slots) slot = {ItemId::Coal,Inventory::StackLimit};
    partial.equipment.inventory.slots.back() = {ItemId::Ruby,95};
    partial.equipment.lootProgress[record].unlocked = true;
    require(partial.levelController().start(partial,level), "partial-inventory fixture starts cleared");
    position(partial,dropPosition(partial,ItemId::Ruby)); tick(partial,15);
    require(partial.equipment.inventory.count(ItemId::Ruby) == 99
            && partial.equipment.lootProgress.at(record+"/reward-0").taken[0] == 4 && pickups(partial) == 2,
            "a nearly full bag takes only four rubies and leaves the other three plus the key in the room");
    const auto partialProfile = read(partialOptions.progressPath);
    tick(partial,20);
    require(read(partialOptions.progressPath) == partialProfile && partial.equipment.inventory.count(ItemId::Ruby) == 99,
            "a full bag does not repeatedly save or duplicate a partial reward");
    Game partialReload(partialOptions);
    require(partialReload.levelController().start(partialReload,level), "partial rewards survive a profile reload");
    require(partialReload.equipment.inventory.remove(ItemId::Coal,99), "fixture makes exactly one bag slot available");
    position(partialReload,dropPosition(partialReload,ItemId::Ruby)); tick(partialReload,15);
    require(partialReload.equipment.inventory.count(ItemId::Ruby) == 102
            && partialReload.equipment.lootProgress.at(record+"/reward-0").taken[0] == 7 && pickups(partialReload) == 1,
            "after making room, only the three remaining rubies can be claimed");

    const auto library = std::filesystem::path(options.sectionLibrary);
    auto keyMeta = meta; keyMeta.name = "Key reward chamber";
    auto keySpawner = spawner; keySpawner.chestItems = {{ItemId::Key,1}};
    const HubAsset locked{HubAssetKind::IronboundChest,{12.5f,9,.5f},0,0,{{ItemId::GoldOre,2}}};
    const std::array keyAssets{keySpawner,locked};
    const auto keyPath = acceptSection(room,keyMeta,keyAssets,library);
    LevelDefinition keyLevel; keyLevel.name = "Reward key"; keyLevel.sections = {keyPath.lexically_relative(library)};
    const auto keyRecord = keyLevel.sections[0].generic_string()+"/asset-0";
    Options keyOptions = options; keyOptions.progressPath = (root/"spent-key.vxp").string();
    Game keyGame(keyOptions); keyGame.equipment.lootProgress[keyRecord].unlocked = true;
    require(keyGame.levelController().start(keyGame,keyLevel), "reward-key fixture starts cleared");
    collect(keyGame);
    position(keyGame,locked.position+glm::vec3(0,0,3));
    aim(keyGame,locked.position+glm::vec3(0,.6f,0)); press(keyGame,&InputActions::interact);
    require(keyGame.chestOpen() && keyGame.equipment.inventory.count(ItemId::Key) == 0
            && keyGame.equipment.lootProgress.at(keyRecord+"/reward-0").taken[0] == 1,
            "spending a reward key permanently claims its exact spawner source");
    press(keyGame,&InputActions::closeMenu);
    require(keyGame.levelController().start(keyGame,keyLevel) && pickups(keyGame) == 0,
            "a spent reward key cannot be collected again on later visits");

    auto visibilityMeta = meta; visibilityMeta.name = "Claimed pickup before spawner";
    const std::array visibilityAssets{
        HubAsset{HubAssetKind::ItemPickup,{-12.5f,9,.5f},0,0,{}, {ItemId::Ruby,1}},
        spawner,
        HubAsset{HubAssetKind::TimberPillar,{3.5f,9.5f,.5f}}
    };
    const auto visibilityPath = acceptSection(room,visibilityMeta,visibilityAssets,library);
    LevelDefinition visibilityLevel; visibilityLevel.name = "Collision identity";
    visibilityLevel.sections = {visibilityPath.lexically_relative(library)};
    Options visibilityOptions = options; visibilityOptions.progressPath.clear();
    Game visibility(visibilityOptions);
    visibility.equipment.lootProgress[visibilityLevel.sections[0].generic_string()+"/asset-0"].taken[0] = 1;
    require(visibility.levelController().start(visibility,visibilityLevel), "claimed-pickup visibility fixture starts");
    position(visibility,{5.5f,9,.5f}); tick(visibility,10);
    require(!visibility.encounters().active() && visibility.encounters().monsters().empty(),
            "removing a claimed pickup cannot shift collider IDs and let spawners see through another model");
    position(visibility,{.5f,9,5.5f}); tick(visibility,10);
    require(visibility.encounters().active() && !visibility.encounters().monsters().empty(),
            "the same spawner activates from its unobstructed side after a prior pickup was removed");
}
}

int main()
{
    using namespace voxel;
    char temporary[] = "/tmp/voxel-encounter-game-XXXXXX";
    if (!mkdtemp(temporary)) return 1;
    const std::filesystem::path root(temporary), library = root / "sections";
    int result = 0;
    try {
        World room; buildBlankSection(room);
        SectionMetadata first;
        first.name = "Trial chamber"; first.tested = true;
        first.spawn = {.5f,9,20.5f}; first.finish = {.5f,9,-20.5f};
        HubAsset spawner{HubAssetKind::TrialSpawner,{.5f,9,.5f},0,0,{{ItemId::Ruby,7},{ItemId::Key,1}}};
        spawner.spawner.counts = {1,0,0};
        const auto firstPath = acceptSection(room,first,std::span(&spawner,1),library);
        SectionMetadata second = first; second.name = "Safe chamber";
        const auto secondPath = acceptSection(room,second,{},library);
        const auto sectionBytes = read(firstPath);
        LevelDefinition level; level.name = "Encounter test";
        level.sections = {firstPath.lexically_relative(library), secondPath.lexically_relative(library)};
        const auto record = level.sections[0].generic_string() + "/asset-0";
        Options options; options.sectionLibrary = library.string();
        options.levelLibrary = (root/"levels").string(); options.progressPath = (root/"progress.vxp").string();
        Game game(options);
        require(game.levelController().start(game,level), "authored encounter level starts");
        require(game.encounters().remaining() == 1 && !game.encounters().active() && game.monsters().empty(),
                "distant spawner waits for proximity and does not pre-spawn enemies");
        position(game,first.finish); tick(game,8);
        require(game.levelController().sectionIndex() == 0 && game.levelController().screen() == LevelScreen::Play,
                "walking around an unfinished spawner cannot bypass the section End");
        activate(game);
        require(std::none_of(game.encounters().healthBars().begin(),game.encounters().healthBars().end(),
            [](const auto& bar) { return bar.visibleSeconds > 0; }), "undamaged enemies do not show health bars");
        attackTarget(game,0);
        require(std::none_of(game.encounters().healthBars().begin(),game.encounters().healthBars().end(),
            [](const auto& bar) { return bar.current < bar.maximum; }), "a pickaxe cannot invoke sword damage");
        require(game.encounters().hits().empty() && game.encounters().monsters().front().hitReaction == 0,
                "a pickaxe produces no impact particles or hurt flash");
        aim(game,game.camera.position()-game.camera.forward()*4.0f);
        InputActions miss; miss.active = miss.attack = true; miss.selectedTool = 3;
        game.update(miss,.1f);
        require(game.encounters().hits().empty() && game.encounters().monsters().front().hitReaction == 0,
                "swinging the sword away from the target produces no damage feedback");
        tick(game,5);
        attackTarget(game,3);
        require(game.equipment.selectedTool() == Tool::Sword && std::any_of(game.encounters().healthBars().begin(),
            game.encounters().healthBars().end(),[](const auto& bar) { return bar.current < bar.maximum && bar.visibleSeconds > 0; }),
            "the fourth tool damages the targeted mob and reveals its health bar");
        require(game.encounters().hits().size() == 1 && !game.encounters().hits().front().lethal
                && game.encounters().hits().front().kind == MonsterKind::Skeleton
                && game.encounters().monsters().front().hitReaction > 0,
                "a real sword hit emits one impact and makes the surviving skeleton flash");
        const auto impactAge = game.encounters().hits().front().age;
        const auto reaction = game.encounters().monsters().front().hitReaction;
        const auto hurtPosition = game.encounters().monsters().front().position;
        InputActions inactive;
        for (int i = 0; i < 10; ++i) game.update(inactive,.1f);
        require(game.encounters().hits().size() == 1 && game.encounters().hits().front().age == impactAge
                && game.encounters().monsters().front().hitReaction == reaction
                && game.encounters().monsters().front().position == hurtPosition,
                "losing focus freezes damage particles, hurt flash, and knockback");
        press(game,&InputActions::inventory); tick(game,10);
        require(game.inventoryOpen() && game.encounters().hits().size() == 1
                && game.encounters().hits().front().age == impactAge
                && game.encounters().monsters().front().hitReaction == reaction
                && game.encounters().monsters().front().position == hurtPosition,
                "the inventory pauses damage feedback alongside enemy movement");
        press(game,&InputActions::closeMenu);
        require(game.damagePlayer(100), "unfinished encounter can kill the player");
        respawn(game);
        require(game.player.feetPosition() == first.spawn && game.encounters().remaining() == 1
                && game.encounters().monsters().empty() && !game.encounters().active()
                && game.encounters().hits().empty(),
                "respawning discards damaged enemies and their feedback while rearming the unfinished encounter");
        activate(game); defeat(game);
        require(std::any_of(game.encounters().hits().begin(),game.encounters().hits().end(),
            [](const auto& hit) { return hit.lethal; }), "a killing sword hit keeps its impact visible after the mob disappears");
        require(game.equipment.lootProgress.at(record).unlocked && pickups(game) == 2,
                "completing persists the encounter and releases both configured reward stacks");
        require(std::filesystem::is_regular_file(options.progressPath), "encounter completion writes player progress");
        require(read(firstPath) == sectionBytes, "playing never rewrites the authored section");
        position(game,first.finish); tick(game,5);
        require(game.levelController().sectionIndex() == 1 && game.encounters().hits().empty()
                && game.equipment.lootProgress.at(record).unlocked,
                "entering the next section clears transient impacts without losing the permanent spawner clear");

        Game restored(options);
        require(restored.levelController().start(restored,level), "cleared level can be loaded again");
        position(restored,{5.5f,9,.5f}); tick(restored,15);
        require(restored.encounters().remaining() == 0 && restored.encounters().monsters().empty() && pickups(restored) == 2,
                "reloading keeps the spawner cleared and restores unclaimed rewards");
        require(restored.encounters().hits().empty(), "loading durable progress does not restore transient damage feedback");
        require(restored.damagePlayer(100), "death can occur after encounter completion");
        respawn(restored);
        require(restored.encounters().remaining() == 0 && pickups(restored) == 2,
                "death does not rearm a cleared encounter or remove its unclaimed rewards");
        collect(restored);
        require(restored.equipment.inventory.count(ItemId::Ruby) == 7 && restored.equipment.inventory.count(ItemId::Key) == 1,
                "rewards enter inventory with exact authored quantities");
        tick(restored,15);
        require(restored.equipment.inventory.count(ItemId::Ruby) == 7 && pickups(restored) == 0,
                "remaining near the emptied spawner cannot duplicate reward claims");
        position(restored,first.finish); tick(restored,10);
        require(restored.levelController().sectionIndex() == 1 && restored.equipment.inventory.count(ItemId::Ruby) == 7
                && restored.equipment.inventory.count(ItemId::Key) == 0,
                "clearing opens the End and unused reward keys cannot leave their section");
        require(restored.levelController().start(restored,level), "the expedition can be revisited after leaving");
        require(restored.encounters().remaining() == 0 && pickups(restored) == 1,
                "spent ordinary loot stays gone while an unused section key becomes available again");
        const auto remaining = std::find_if(restored.sectionSceneAssets().begin(),restored.sectionSceneAssets().end(),
            [](const auto& asset) { return isItemPickup(asset.kind); });
        require(remaining != restored.sectionSceneAssets().end() && itemPickupStack(*remaining).item == ItemId::Key,
                "the only respawned reward is the unspent section key");

        const auto profile = read(options.progressPath);
        Options dev = options; dev.dev = true;
        Game builder(dev);
        const auto equipmentBefore = builder.equipment;
        click(builder,SectionUiWorldBase);
        const auto& entries = builder.sectionEditor().entries();
        const auto entry = std::find_if(entries.begin(),entries.end(),[&](const auto& value) { return value.path == firstPath; });
        require(entry != entries.end(), "authored spawner fixture appears in the section library");
        click(builder,SectionUiLibraryEntryBase+static_cast<int>(entry-entries.begin()));
        press(builder,&InputActions::devTest);
        require(builder.encounters().remaining() == 1, "F8 starts fresh encounters despite permanent clear progress");
        position(builder,first.finish); tick(builder,6);
        require(!builder.sectionEditor().testComplete(), "F8 also requires clearing encounters before passing End");
        activate(builder); defeat(builder); collect(builder);
        require(builder.equipment.inventory.count(ItemId::Ruby) == equipmentBefore.inventory.count(ItemId::Ruby)+7,
                "playtest loot is available in the temporary inventory");
        position(builder,first.finish); tick(builder,2);
        require(builder.sectionEditor().testComplete(), "F8 can pass after the authored encounter is defeated");
        press(builder,&InputActions::devTest);
        require(builder.equipment == equipmentBefore && read(options.progressPath) == profile && read(firstPath) == sectionBytes,
                "leaving F8 restores equipment and never modifies real progress or accepted sections");
        playerHitChecks(options,level,first);
        auditChecks(options,level,room,first,spawner,root);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    std::filesystem::remove_all(root);
    return result;
}
