#include "game.h"
#include "health.h"
#include "input.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void tick(Game& game, int frames = 1)
{
    InputActions action; action.active = true;
    for (int i = 0; i < frames; ++i) game.update(action, .1f);
}
void waitForRespawn(Game& game)
{
    for (int i = 0; i < 20 && game.health().dead(); ++i) tick(game);
    require(!game.health().dead() && game.health().current() == Health::Maximum,
            "death finishes with a full-health respawn");
}
void press(Game& game, bool InputActions::* key)
{
    InputActions action; action.active = true; action.*key = true; game.update(action);
}
void click(Game& game, int id)
{
    InputActions action; action.active = true; action.devClick = id; game.update(action);
}
void position(Game& game, glm::vec3 feet)
{
    require(game.player.setFeetPosition(game.world, feet), "test player position must fit");
    game.camera.setPosition(game.player.eyePosition());
}
void aim(Game& game, glm::vec3 point)
{
    const auto direction = glm::normalize(point - game.camera.position());
    const auto previous = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(previous.z, previous.x)),
                       glm::degrees(std::asin(direction.y) - std::asin(previous.y)));
}
std::string read(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void unitChecks()
{
    Health health;
    require(health.current() == 100 && health.fraction() == 1 && !health.dead(),
            "health starts full and alive");
    require(!health.damage(0) && !health.damage(-12) && health.current() == 100,
            "zero and negative damage cannot heal or trigger a damage flash");
    require(health.damageFlash() == 0 && health.respawnProgress() == 0 && !health.readyToRespawn(),
            "living players do not have a death timer");
    require(health.damage(25) && health.current() == 75 && health.fraction() == .75f
            && health.damageFlash() == 1, "damage subtracts health and begins the feedback flash");
    health.update(.1f);
    require(health.damageFlash() > 0 && health.damageFlash() < 1, "damage feedback fades over time");
    for (const float duration : {-1.0f, std::numeric_limits<float>::infinity(),
                                 std::numeric_limits<float>::quiet_NaN()}) {
        const auto flash = health.damageFlash();
        health.update(duration);
        require(health.damageFlash() == flash, "invalid frame durations cannot corrupt health feedback");
    }
    require(health.damage(std::numeric_limits<int>::max()) && health.current() == 0 && health.dead(),
            "arbitrarily large damage clamps health to zero without integer overflow");
    health.update(.1f);
    const auto progress = health.respawnProgress();
    require(!health.damage(1) && health.respawnProgress() == progress,
            "additional damage cannot keep restarting the death delay");
    health.update(1000);
    require(health.respawnProgress() > progress && health.respawnProgress() < .25f,
            "a long frame cannot skip the death presentation");
    for (int i = 0; i < 20; ++i) health.update(.1f);
    require(health.readyToRespawn() && health.respawnProgress() == 1 && health.damageFlash() == 0,
            "the death timer finishes and clamps its progress");
    health.restore();
    require(health.current() == 100 && !health.dead() && !health.readyToRespawn()
            && health.damageFlash() == 0 && health.respawnProgress() == 0,
            "restoring health clears death and damage feedback state");
    require(fallDamage(0) == 0 && fallDamage(3) == 0 && fallDamage(4) == 10
            && fallDamage(5) == 20 && fallDamage(13) == 100 && fallDamage(10000) == 100,
            "fall damage tolerates short drops and scales to a lethal long fall");
    require(fallDamage(-5) == 0 && fallDamage(std::numeric_limits<float>::infinity()) == 0
            && fallDamage(std::numeric_limits<float>::quiet_NaN()) == 0,
            "invalid landing distances cannot produce invalid damage");
}

void knockbackChecks(Game& game)
{
    game.world = World{};
    buildBlankSection(game.world);
    game.player.setAssetCollision({});
    const glm::vec3 start(.5f, 9, .5f);
    position(game, start);
    InputActions active; active.active = true;
    require(game.damagePlayer(5, {10, 7, 0}) && game.playerHitDirection() == glm::vec3(1, 0, 0),
            "damage retains a normalized horizontal source direction");
    const float flash = game.health().damageFlash();
    for (int i = 0; i < 5; ++i) game.updateCombat({}, .05f);
    require(game.player.feetPosition() == start && game.health().damageFlash() == flash,
            "inactive combat never advances the knockback or feedback timers");
    float previousDistance = 1;
    for (int i = 0; i < 5; ++i) {
        const auto before = game.player.feetPosition();
        game.updateCombat(active, .05f);
        const float distance = game.player.feetPosition().x - before.x;
        require(distance >= 0 && distance < previousDistance,
                "the short player shove decelerates smoothly across frames");
        previousDistance = distance;
    }
    require(std::abs(game.player.feetPosition().x - start.x - .4f) < .0001f
            && game.player.feetPosition().y == start.y
            && game.camera.position() == game.player.eyePosition(),
            "a free shove travels four tenths of a block and keeps the camera with the body");
    require(game.health().damageFlash() == flash,
            "combat never double-decrements the health feedback timer");
    game.restoreHealth(); position(game, start);
    game.damagePlayer(5, {1, 0, 0});
    for (int i = 0; i < 3; ++i) game.updateCombat(active, .1f);
    require(std::abs(game.player.feetPosition().x - start.x - .4f) < .0001f,
            "the integrated shove distance stays the same at different frame rates");

    game.restoreHealth(); position(game, start);
    for (int y = 9; y <= 11; ++y) game.world.set({1, y, 0}, Block::Stone);
    game.damagePlayer(5, {1, 0, 0});
    for (int i = 0; i < 5; ++i) game.updateCombat(active, .05f);
    require(game.player.feetPosition().x > start.x
            && game.player.feetPosition().x <= 1 - Player::Width / 2 + .0001f,
            "player knockback stops the full body at a voxel wall");
    for (int y = 9; y <= 11; ++y) game.world.set({1, y, 0}, Block::Air);

    game.restoreHealth(); position(game, start);
    const std::array fence{HubAsset{HubAssetKind::TimberFence, {.5f, 9, 1.15f}}};
    auto collision = std::make_shared<AssetCollision>(); collision->rebuild(fence);
    game.player.setAssetCollision(collision);
    require(game.player.setFeetPosition(game.world, start), "fence fixture starts outside its collider");
    game.damagePlayer(5, {0, 0, 1});
    for (int i = 0; i < 5; ++i) game.updateCombat(active, .05f);
    const auto feet = game.player.feetPosition();
    require(feet.z > start.z && feet.z < start.z + .4f
            && !collision->overlaps(feet + glm::vec3(-.3f, .001f, -.3f),
                                    feet + glm::vec3(.3f, Player::Height, .3f)),
            "player knockback also stops on thin decorative fence geometry");
    game.player.setAssetCollision({});

    game.restoreHealth(); position(game, start);
    game.damagePlayer(5, {1, 0, 0});
    game.restoreHealth(); game.updateCombat(active, .1f);
    require(game.player.feetPosition() == start && game.playerHitDirection() == glm::vec3(0)
            && game.health().damageFlash() == 0,
            "restoring health cancels the old room's direction, shove and flash");
    game.damagePlayer(5, {1, 0, 0});
    game.endSectionLoot(); game.updateCombat(active, .1f);
    require(game.player.feetPosition() == start && game.playerHitDirection() == glm::vec3(0)
            && game.health().damageFlash() == 0 && game.health().current() == 95,
            "leaving a section clears damage feedback without healing the player");
    game.restoreHealth();
    require(game.damagePlayer(5) && game.playerHitDirection() == glm::vec3(0),
            "environmental damage has feedback without inventing an attacker");
    game.updateCombat(active, .1f);
    require(game.player.feetPosition() == start && game.health().damageFlash() > 0,
            "environmental damage never causes a sideways shove");
    for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity()}) {
        game.damagePlayer(1, {invalid, 0, 1}); game.updateCombat(active, .1f);
        require(game.playerHitDirection() == glm::vec3(0) && game.player.feetPosition() == start,
                "invalid damage directions cannot corrupt player coordinates");
    }
    game.restoreHealth();
    game.damagePlayer(100, {1, 0, 0}); game.updateCombat(active, .1f);
    require(game.player.feetPosition() == start, "a lethal hit cancels displacement during the death screen");
    game.restoreHealth();
}
}

int main()
{
    char pattern[] = "/tmp/voxel-health-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern), sections = root / "sections";
    int result = 0;
    try {
        unitChecks();
        World room;
        buildBlankSection(room);
        for (int z = 20; z < 25; ++z)
            for (int x = 20; x < 25; ++x)
                for (int y = 9; y <= 12; ++y) room.set({x, y, z}, Block::Water);
        SectionMetadata first;
        first.name = "Checkpoint room"; first.tested = true;
        first.spawn = {.5f, 9, 8.5f}; first.finish = {.5f, 9, -8.5f};
        first.checkpoint = glm::vec3{.5f, 9, .5f};
        const HubAsset chest{HubAssetKind::LootChest, {.5f, 9, 5.5f}, 0, 0, {{ItemId::Ruby, 7}}};
        const auto firstPath = acceptSection(room, first, std::span(&chest, 1), sections);
        SectionMetadata second = first;
        second.name = "No checkpoint room";
        second.spawn = {12.5f, 9, 8.5f}; second.finish = {12.5f, 9, -8.5f}; second.checkpoint.reset();
        const auto secondPath = acceptSection(room, second, {}, sections);
        const auto originalSection = read(firstPath);
        LevelDefinition level;
        level.name = "Health expedition";
        level.sections = {firstPath.lexically_relative(sections), secondPath.lexically_relative(sections)};
        Options options;
        options.sectionLibrary = sections.string(); options.levelLibrary = (root / "levels").string();
        options.progressPath = (root / "progress.vxp").string();
        Game game(options);
        knockbackChecks(game);
        auto& controller = game.levelController();
        require(controller.start(game, level) && game.healthVisible() && game.health().current() == 100,
                "an expedition begins at full health with the health display enabled");
        require(!game.damagePlayer(0) && !game.damagePlayer(-1) && game.health().current() == 100,
                "the gameplay damage API also ignores invalid amounts");

        // A death before reaching the marker must use Start, not the authored
        // checkpoint merely because that checkpoint exists in the document.
        require(game.damagePlayer(100) && game.health().dead(), "lethal damage starts death");
        const auto deadPosition = game.player.feetPosition();
        const auto deadEquipment = game.equipment;
        InputActions paused;
        for (int i = 0; i < 20; ++i) game.update(paused, .1f);
        require(game.health().dead() && game.health().respawnProgress() == 0
                && game.player.feetPosition() == deadPosition,
                "pausing freezes the death timer and player position");
        InputActions controls;
        controls.active = controls.jump = controls.interact = controls.inventory = controls.upgrade = true;
        controls.place = controls.remove = true; controls.movement = {1, 1}; controls.selectedTool = 2;
        const auto deadTime = controller.elapsed();
        game.update(controls, .1f);
        require(game.health().dead() && game.player.feetPosition() == deadPosition
                && game.equipment == deadEquipment && !game.inventoryOpen() && !game.chestOpen()
                && controller.elapsed() == deadTime,
                "death suppresses movement, tools, interactions, inventory and the run timer");
        waitForRespawn(game);
        require(game.player.feetPosition() == first.spawn && !controller.checkpointReached()
                && controller.sectionIndex() == 0 && game.camera.position() == game.player.eyePosition(),
                "death without an activated checkpoint respawns at the current section Start");

        aim(game, chest.position + glm::vec3(0, .65f, 0));
        press(game, &InputActions::interact);
        require(game.chestOpen(), "the respawned player can open the test chest");
        press(game, &InputActions::lootAll);
        require(game.equipment.inventory.count(ItemId::Ruby) == 7, "fixture loot is collected into inventory");
        press(game, &InputActions::closeMenu);
        const auto collected = game.equipment;
        const auto savedProfile = read(options.progressPath);
        position(game, *first.checkpoint); tick(game);
        require(controller.checkpointReached(), "touching the grounded checkpoint activates it");
        require(game.damagePlayer(100), "checkpoint death starts normally");
        waitForRespawn(game);
        require(game.player.feetPosition() == *first.checkpoint && controller.checkpointReached()
                && controller.sectionIndex() == 0 && game.equipment == collected,
                "death respawns at the active checkpoint without losing inventory or section progress");
        require(read(options.progressPath) == savedProfile && read(firstPath) == originalSection,
                "health and checkpoint respawns never rewrite inventory or accepted section files");
        position(game, first.spawn);
        aim(game, chest.position + glm::vec3(0, .65f, 0));
        press(game, &InputActions::interact);
        require(game.chestOpen() && std::all_of(game.chestContents().begin(), game.chestContents().end(),
                [](const auto& stack) { return stack.count == 0; }),
                "dying does not refill already collected chest loot");
        press(game, &InputActions::closeMenu);

        require(game.damagePlayer(25), "nonlethal damage leaves a wounded player");
        position(game, first.finish); tick(game, 5);
        require(controller.sectionIndex() == 1 && controller.screen() == LevelScreen::Play
                && game.health().current() == 75 && !controller.checkpointReached(),
                "normal section transitions carry health and clear the previous checkpoint");
        require(game.damagePlayer(100), "the next section can independently cause death");
        waitForRespawn(game);
        require(game.player.feetPosition() == second.spawn && controller.sectionIndex() == 1,
                "a section without a checkpoint uses its own Start, never a previous section's marker");

        // Actual player physics reports landing damage, while water breaks a fall.
        position(game, {18.5f, 14, 18.5f});
        for (int i = 0; i < 60 && !game.player.grounded(); ++i) tick(game);
        require(game.player.grounded() && game.health().current() == 80
                && game.playerHitDirection() == glm::vec3(0),
                "a five-block dry fall takes twenty health through normal gameplay updates");
        game.restoreHealth();
        position(game, {22.5f, 22, 22.5f}); tick(game, 30);
        require(game.player.inWater() && game.health().current() == 100,
                "landing in a deep pool cancels fall damage");
        position(game, {18.5f, -2, 18.5f}); tick(game);
        require(game.health().dead(), "falling below the section enters the death flow");
        waitForRespawn(game);
        require(game.player.feetPosition() == second.spawn && !game.player.inWater(),
                "a void death returns to a dry safe spawn");
        require(game.damagePlayer(35) && controller.start(game, level) && game.health().current() == 100,
                "starting a fresh expedition restores health");

        // The builder playtest uses the same death rules but leaves the draft,
        // accepted room and saved player profile untouched.
        Options dev = options; dev.dev = true;
        Game builder(dev);
        require(!builder.healthVisible() && !builder.damagePlayer(100),
                "creative authoring does not take damage or show survival health");
        click(builder, SectionUiWorldBase);
        const auto& entries = builder.sectionEditor().entries();
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& value) { return value.path == firstPath; });
        require(entry != entries.end(), "the checkpoint fixture is available in the section library");
        click(builder, SectionUiLibraryEntryBase + static_cast<int>(entry - entries.begin()));
        press(builder, &InputActions::devTest);
        require(builder.sectionEditor().screen() == SectionEditorScreen::Test && builder.healthVisible(),
                "F8 playtests enable health");
        require(builder.damagePlayer(100), "a section playtest can die before its checkpoint");
        waitForRespawn(builder);
        require(builder.player.feetPosition() == first.spawn, "playtest death initially returns to Start");
        position(builder, *first.checkpoint); tick(builder);
        require(builder.sectionEditor().checkpointReached() && builder.damagePlayer(100),
                "playtests activate the same authored checkpoint");
        waitForRespawn(builder);
        require(builder.player.feetPosition() == *first.checkpoint,
                "playtest death returns to the active checkpoint");
        require(builder.damagePlayer(20), "playtest health can be reduced after respawn");
        press(builder, &InputActions::devTest);
        require(!builder.healthVisible() && !builder.damagePlayer(100), "returning to Build disables damage");
        press(builder, &InputActions::devTest);
        require(builder.health().current() == 100 && !builder.sectionEditor().checkpointReached(),
                "a fresh playtest restores health and clears checkpoint activation");
        require(read(firstPath) == originalSection && read(options.progressPath) == savedProfile,
                "playtest deaths never mutate accepted scenes or saved equipment");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result = 1;
    }
    std::filesystem::remove_all(root);
    return result;
}
