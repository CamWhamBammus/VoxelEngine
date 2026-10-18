#include "game.h"
#include "input.h"
#include "scene.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

namespace {
void look(voxel::Game& game, glm::vec3 direction)
{
    direction = glm::normalize(direction);
    const auto current = game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(current.z, current.x)),
                       glm::degrees(std::asin(direction.y) - std::asin(current.y)));
}
}

int main()
{
    int failures = 0;
    const auto check = [&](bool passed, const char* message) {
        if (!passed) { std::cerr << message << '\n'; ++failures; }
    };
    char name[] = "/tmp/voxel-play-XXXXXX";
    if (!mkdtemp(name)) return 1;
    const std::filesystem::path directory(name);
    try {
        voxel::Options options;
        options.seed = 42;
        options.worldPath = (directory / "world.vxl").string();
        { std::ofstream file(options.worldPath); file << "existing editor save"; }
        voxel::Game game(options);
        check(!game.editorMode() && game.world.seed() == 42, "default mode starts the underground sample");
        check(game.player.grounded() && game.player.feetPosition() == voxel::VillageSpawn,
              "player starts on the village floor, not on the cavern roof");
        check(game.camera.position() == game.player.eyePosition(), "camera follows the player's eyes");
        check(game.blockCount(voxel::Block::Stone) == 24 && game.blockCount(voxel::Block::Planks) == 24
              && game.blockCount(voxel::Block::Lantern) == 8, "sample supplies building and lighting materials");
        const auto spawn = game.player.feetPosition();
        game.update({}, 1000.0f);
        check(game.player.feetPosition() == spawn, "released mouse pauses the player");
        check(game.statusText(0, 0, 0, 0, false, false).find("PAUSED") != std::string::npos,
              "released mouse displays pause instructions");

        voxel::InputActions actions;
        actions.active = true;
        actions.jump = true;
        game.update(actions, 1.0f / 60.0f);
        check(game.player.feetPosition().y > spawn.y && !game.player.grounded(), "space starts a grounded jump");
        actions.jump = false;
        for (int frame = 0; frame < 120; ++frame) game.update(actions, 1.0f / 60.0f);
        check(game.player.grounded() && std::abs(game.player.feetPosition().y - spawn.y) < 0.01f,
              "gravity lands the player back on the village floor");
        actions.movement = {1, 0};
        game.update(actions, 0.1f);
        check(game.player.feetPosition() != spawn, "WASD drives the physical player");
        actions = {};
        actions.active = true;
        actions.save = actions.load = actions.undo = actions.redo = true;
        game.update(actions);
        std::ifstream file(options.worldPath);
        std::string contents;
        std::getline(file, contents);
        check(contents == "existing editor save", "sample controls never overwrite or load editor saves");
        check(!game.hasUnsavedChanges(), "temporary sample edits do not claim to be an editor save");

        // Flat workshop fixtures isolate the ore inventory and placement rules.
        voxel::Game workshop(options);
        for (int z = voxel::World::Min.z; z < voxel::World::MaxExclusive.z; ++z) {
            for (int x = voxel::World::Min.x; x < voxel::World::MaxExclusive.x; ++x) {
                for (int y = 0; y < voxel::World::MaxExclusive.y; ++y) {
                    workshop.world.set({x, y, z}, y == 0 ? voxel::Block::Stone : voxel::Block::Air);
                }
            }
        }
        check(workshop.player.setFeetPosition(workshop.world, {0.5f, 1.0f, 0.5f}), "workshop player fits");
        look(workshop, {1, 0, 0});
        workshop.world.set({3, 2, 0}, voxel::Block::CopperOre);
        actions = {};
        actions.active = true;
        actions.remove = true;
        workshop.update(actions);
        check(workshop.world.get({3, 2, 0}) == voxel::Block::Air && workshop.blockCount(voxel::Block::CopperOre) == 1,
              "mining copper adds exactly one copper ore block to the backpack");
        check(workshop.history.undoCount() == 0, "mining cannot duplicate resources through editor undo");
        workshop.world.set({4, 2, 0}, voxel::Block::Stone);
        actions.remove = false;
        actions.place = true;
        actions.selectedBlock = voxel::Block::CopperOre;
        workshop.update(actions);
        check(workshop.world.get({3, 2, 0}) == voxel::Block::CopperOre && workshop.blockCount(voxel::Block::CopperOre) == 0,
              "placing copper consumes exactly one available ore block");
        workshop.update(actions);
        check(workshop.world.get({2, 2, 0}) == voxel::Block::Air && workshop.blockCount(voxel::Block::CopperOre) == 0,
              "empty inventory cannot create blocks or go negative");
        workshop.world.set({1, 1, 0}, voxel::Block::Stone);
        look(workshop, {0.5f, -0.8660254f, 0});
        actions.selectedBlock = voxel::Block::Planks;
        workshop.update(actions);
        check(workshop.world.get({0, 1, 0}) == voxel::Block::Air && workshop.blockCount(voxel::Block::Planks) == 24
              && workshop.status == "Cannot place: your body is in the destination cell",
              "placing into the player's body is rejected without consuming stock");

        look(workshop, {1, 0, 0});
        workshop.world.set({3, 2, 0}, voxel::Block::Bedrock);
        actions = {};
        actions.active = actions.remove = true;
        workshop.update(actions);
        check(workshop.world.get({3, 2, 0}) == voxel::Block::Bedrock
              && workshop.blockCount(voxel::Block::Bedrock) == 0, "bedrock cannot be mined out of the sample boundary");
        actions.remove = false;
        actions.selectedBlock = voxel::Block::Bedrock;
        workshop.update(actions);
        check(workshop.selectedBlock == voxel::Block::Planks, "bedrock cannot be selected for construction");
        check(workshop.blockCount(static_cast<voxel::Block>(255)) == 0, "invalid materials never index the backpack");

        for (const std::uint32_t seed : {42u, 1337u, 4294967295u}) {
            options.seed = seed;
            voxel::Game expedition(options);
            const auto original = expedition.world;
            // Returning is a relocation, preserving all collected blocks and edits.
            check(expedition.player.setFeetPosition(expedition.world, voxel::VillageSpawn + glm::vec3(0, 1, 0)),
                  "village has vertical space for the return fixture");
            const glm::ivec3 occupiedSpawn(glm::floor(voxel::VillageSpawn));
            expedition.world.set(occupiedSpawn, voxel::Block::Planks);
            const auto edited = expedition.world;
            actions = {};
            actions.active = actions.respawn = true;
            expedition.update(actions, 0.1f);
            check(expedition.player.grounded() && expedition.player.feetPosition().y < voxel::World::MaxExclusive.y
                  && !expedition.player.overlapsBlock(occupiedSpawn), "return finds safe space below the ceiling when spawn is built over");
            check(expedition.world.sameContents(edited) && !expedition.world.sameContents(original)
                  && expedition.blockCount(voxel::Block::Planks) == 24, "return keeps the sample edits and backpack intact");
            actions.respawn = false;
            const auto feet = expedition.player.feetPosition();
            expedition.update(actions, 1000.0f);
            check(expedition.player.feetPosition() == feet, "exploration has no challenge countdown or time limit");
            check(expedition.statusText(0, 0, 0, 0, true, true).find("EXPEDITIONS AND BUILDING SERVICES COMING LATER") != std::string::npos,
                  "hub controls accurately identify the current development scope");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ++failures;
    }
    std::filesystem::remove_all(directory);
    return failures ? 1 : 0;
}
