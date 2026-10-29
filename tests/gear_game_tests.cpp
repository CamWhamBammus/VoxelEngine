#include "game.h"
#include "input.h"
#include "scene.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;
void require(bool pass, const char* message)
{
    if (!pass) throw std::runtime_error(message);
}
void press(Game& game, bool InputActions::*key)
{
    InputActions input; input.active = true; input.*key = true;
    game.update(input, .1f);
}
void position(Game& game, glm::vec3 feet)
{
    require(game.player.setFeetPosition(game.world, feet), "fixture position must fit");
    game.camera.setPosition(game.player.eyePosition());
}
void aim(Game& game, glm::vec3 point)
{
    const auto direction = glm::normalize(point - game.camera.position());
    const auto old = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(old.z, old.x)),
                       glm::degrees(std::asin(direction.y) - std::asin(old.y)));
}
void shopping(const std::filesystem::path& root)
{
    Options options; options.progressPath = (root / "gear.vxp").string();
    Game game(options);
    require(!game.armoryOpen() && !game.nearArmory(), "village arrival does not reach the armory");
    const auto starter = game.equipment;
    press(game, &InputActions::upgrade);
    require(game.equipment == starter, "upgrade input cannot buy gear away from a workshop");
    position(game, sceneArmoryApproach());
    require(game.nearArmory() && !game.nearBlacksmith(), "the armory has its own reachable service");
    const auto blockingCell = glm::ivec3(glm::floor(glm::mix(game.player.eyePosition(), sceneArmoryStation(), .5f)));
    const auto before = game.world.get(blockingCell);
    game.world.set(blockingCell, Block::Stone);
    require(!game.nearArmory(), "a wall blocks armory interaction");
    game.world.set(blockingCell, before);
    press(game, &InputActions::interact);
    require(game.armoryOpen() && game.menuOpen() && !game.smithOpen()
            && game.upgradeSelection() == GearSlot::Helmet, "E opens the armory with helmet selected");
    game.equipment.inventory = {};
    game.equipment.inventory.add(ItemId::Coal, 40);
    const auto standing = game.player.feetPosition();
    for (int row = 0; row < 4; ++row) {
        InputActions buy; buy.active = buy.upgrade = buy.jump = true;
        buy.movement = {1,1}; buy.selectedUpgrade = row; buy.selectedTool = 3;
        game.update(buy, .1f);
        require(game.equipment.armorTiers[row] == 1
                && game.equipment.inventory.count(ItemId::Coal) == 30 - row * 10,
                "each armor slot crafts and equips only its own piece for ten coal");
        require(game.player.feetPosition() == standing && game.equipment.selectedSlot == 0,
                "shopping freezes movement and hotbar selection");
    }
    require(game.equipment.inventory.slots.size() == 20 && armorProtection(game.equipment) > 0,
            "armor protects the player without occupying any bag slots");
    require(loadEquipment(options.progressPath) == game.equipment, "armor and costs survive a profile reload");
    const auto crafted = game.equipment;
    press(game, &InputActions::upgrade);
    require(game.equipment == crafted, "insufficient ore never changes armor or inventory");
    InputActions invalid; invalid.active = true; invalid.selectedUpgrade = 99;
    game.update(invalid);
    require(game.upgradeSelection() == GearSlot::Boots, "invalid armor rows cannot select a hidden tool");
    InputActions next; next.active = true; next.upgradeStep = 1;
    game.update(next);
    require(game.upgradeSelection() == GearSlot::Helmet, "down wraps to the first armor row");
    game.equipment.inventory.add(ItemId::Coal, 10);
    press(game, &InputActions::upgrade);
    require(game.equipment.armorTiers[0] == 2 && game.equipment.armorTiers[1] == 1,
            "upgrading a helmet leaves the other armor pieces equipped");
    press(game, &InputActions::closeMenu);
    require(!game.armoryOpen(), "Escape closes the armor menu");
    press(game, &InputActions::inventory);
    require(game.inventoryOpen(), "equipped armor can be inspected in the ordinary inventory");
    press(game, &InputActions::closeMenu);

    position(game, sceneBlacksmithApproach());
    press(game, &InputActions::interact);
    require(game.smithOpen() && !game.armoryOpen(), "tools remain at their separate blacksmith");
    game.equipment.inventory.add(ItemId::Coal, 10);
    InputActions sword; sword.active = sword.upgrade = true; sword.selectedUpgrade = 1;
    game.update(sword);
    require(game.equipment.swordTier == 2 && game.equipment.pickaxeTier == 1,
            "blacksmith sword upgrade does not upgrade the pickaxe");
    const auto saved = loadEquipment(options.progressPath);
    require(saved == game.equipment, "tool purchases preserve all equipped armor in the same profile");
    press(game, &InputActions::closeMenu);
    position(game, sceneArmoryApproach()); press(game, &InputActions::interact);
    press(game, &InputActions::respawn);
    require(!game.armoryOpen() && game.equipment == saved, "returning to the village closes the armory without losing gear");

    options.progressPath = (root / "missing" / "gear.vxp").string();
    Game failure(options);
    position(failure, sceneArmoryApproach()); press(failure, &InputActions::interact);
    const auto unchanged = failure.equipment;
    press(failure, &InputActions::upgrade);
    require(failure.equipment == unchanged && failure.status.starts_with("Upgrade not saved"),
            "failed armor saves charge nothing and equip nothing");
}

void combat()
{
    Options options;
    Game game(options);
    game.player.setAssetCollision({});
    buildBlankSection(game.world);
    HubAsset trial{HubAssetKind::TrialSpawner, {.5f,9,.5f}};
    trial.spawner.counts = {1,0,0};
    const auto start = [&] {
        game.restoreHealth();
        position(game, {5.5f,9,.5f});
        game.beginSectionLoot(std::span(&trial,1), false, "gear-combat");
        InputActions tick; tick.active = true;
        for (int i = 0; i < 30 && game.monsters().empty(); ++i) game.update(tick,.1f);
        require(game.monsters().size() == 1, "one skeleton spawns for equipment combat checks");
        const auto target = game.monsters()[0].position;
        position(game, target + glm::vec3(0,0,1.6f));
        aim(game, target + glm::vec3(0,.9f,0));
    };
    const auto firstHit = [&](int tier) {
        game.equipment.armorTiers.fill(tier);
        start();
        InputActions tick; tick.active = true;
        for (int i = 0; i < 150 && game.health().current() == Health::Maximum; ++i) game.update(tick,.1f);
        require(game.health().current() < Health::Maximum, "skeleton lands an actual attack");
        return Health::Maximum - game.health().current();
    };
    const int bare = firstHit(0), armored = firstHit(GearTierCount);
    require(armored < bare && armored == armoredDamage(game.equipment, bare),
            "equipped armor reduces actual enemy damage by its displayed protection");
    game.restoreHealth();
    require(game.damagePlayer(Health::Maximum) && game.health().dead(),
            "armor cannot cancel a section kill plane or other direct death");

    start();
    game.equipment.swordTier = 1;
    InputActions swing; swing.active = swing.attack = true; swing.selectedTool = 3;
    game.update(swing,.01f);
    require(game.encounters().healthBars().size() == 1
            && game.encounters().healthBars()[0].current == 15,
            "starter sword retains its existing 25-damage strike");
    start();
    game.equipment.swordTier = GearTierCount;
    game.update(swing,.01f);
    require(game.monsters().empty() && game.sectionEncountersCleared(),
            "upgraded sword damage reaches combat and defeats the same skeleton in one hit");
}
} // namespace

int main()
{
    char temporary[] = "/tmp/voxel-gear-game-XXXXXX";
    if (!mkdtemp(temporary)) return 1;
    int result = 0;
    try {
        shopping(temporary);
        combat();
        std::cout << "Gear gameplay passed: armory, independent purchases, saves, armor and sword damage\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    std::filesystem::remove_all(temporary);
    return result;
}
