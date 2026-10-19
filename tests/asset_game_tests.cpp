#include "game.h"
#include "input.h"
#include "scene.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void press(voxel::Game& game, bool voxel::InputActions::* key)
{
    voxel::InputActions action; action.active = true; action.*key = true;
    game.update(action, 1.0f / 60.0f);
}
}

int main()
{
    using namespace voxel;
    char pattern[] = "/tmp/voxel-asset-game-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    const std::filesystem::path root(pattern), sections = root / "sections";
    int result = 0;
    try {
        World room;
        buildBlankSection(room);
        const std::array<HubAsset, 2> platforms{{
            {HubAssetKind::CrateStack, {-8.5f, 12, 4.5f}, 1, 1},
            {HubAssetKind::CrateStack, {-8.5f, 12, .5f}, 3, 1}}};
        SectionMetadata parkour;
        parkour.name = "Crate jump"; parkour.tested = true;
        parkour.spawn = {-8.5f, 12.82f, 4.5f};
        parkour.finish = {-8.5f, 12.82f, .5f};
        const auto first = acceptSection(room, parkour, platforms, sections);
        SectionMetadata empty;
        empty.name = "Empty landing"; empty.tested = true;
        const auto second = acceptSection(room, empty, {}, sections);
        LevelDefinition level;
        level.name = "Model parkour"; level.number = 1;
        level.sections = {first.lexically_relative(sections), second.lexically_relative(sections)};

        Options options; options.sectionLibrary = sections.string();
        options.levelLibrary = (root / "levels").string();
        Game game(options);
        const auto hubSpawn = game.player.feetPosition();
        require(game.player.grounded() && glm::distance(hubSpawn, VillageSpawn) < .15f,
                "hub arrival settles safely onto fitted paving");
        bool landedOnHubModel = false;
        for (const auto& asset : planHubAssets(game.world)) {
            if (asset.kind != HubAssetKind::CrateStack) continue;
            if (!game.player.setFeetPosition(game.world, asset.position + glm::vec3(0, 4, 0))) continue;
            game.player.move(game.world, {0, -4, 0});
            const float expected = asset.position.y + (asset.variation % 2 == 0 ? 1.47f : .82f);
            if (game.player.grounded() && std::abs(game.player.feetPosition().y - expected) < .01f) {
                landedOnHubModel = true; break;
            }
        }
        require(landedOnHubModel, "hub models use the same physical surfaces as authored assets");

        const auto approach = sceneExpeditionApproach();
        // A centimeter drop between fitted slabs must settle before trying
        // the next riser, even when both contacts occur in one horizontal move.
        // Gravity between steps must not be needed to unstick an uphill seam.
        require(game.player.setFeetPosition(game.world,
            glm::vec3(approach.front()) + glm::vec3(.5f,1.2f,.5f)), "mine seam approach is clear");
        game.player.move(game.world, {0,-.2f,0});
        for (int z = approach.front().z-1; z >= approach[approach.size()-2].z; --z) {
            game.player.move(game.world, {0,0,-1});
            require(game.player.grounded() && std::abs(game.player.feetPosition().z-(z+.5f)) < .001f,
                "uphill fitted paving seams settle before the next stair without a gravity recovery frame");
        }

        for (const int direction : {-1, 1}) {
            const auto start = direction < 0 ? approach.front() : approach[approach.size() - 2];
            const auto finish = direction < 0 ? approach[approach.size() - 2] : approach.front();
            require(game.player.setFeetPosition(game.world, glm::vec3(start) + glm::vec3(.5f, 1.2f, .5f)),
                    "decorated mine approach has spawn headroom");
            game.player.move(game.world, {0, -.2f, 0});
            // Exercise real physics frames: descent naturally includes a brief
            // fall between meter-high treads instead of a forced one-meter snap.
            const float destination = finish.z + .5f;
            const int frameLimit = int(std::abs(finish.z-start.z)/4.5f*60.0f) + 120;
            int frame = 0;
            for (; frame < frameLimit
                && (destination-game.player.feetPosition().z)*direction > .04f; ++frame) {
                game.player.update(game.world, {0,1}, {0,0,float(direction)}, false, false, 1.0f/60.0f);
            }
            for (int settling=0; settling<30; ++settling)
                game.player.update(game.world, {}, {0,0,float(direction)}, false, false, 1.0f/60.0f);
            if (frame == frameLimit || !game.player.grounded()) {
                const auto feet = game.player.feetPosition();
                std::cerr << "Mine stair direction=" << direction << " target=" << destination
                    << " feet=" << feet.x << "," << feet.y << "," << feet.z
                    << " grounded=" << game.player.grounded() << '\n';
            }
            require(frame < frameLimit && game.player.grounded()
                && std::abs(game.player.feetPosition().z-destination) < .1f
                && std::abs(game.player.feetPosition().y-(finish.y+1.0f)) < .15f,
                "the real hub mine stairs remain walkable both ways with fitted path and arch collision");
        }

        for (const int fps : {30, 60, 120}) {
            require(game.levelController().start(game, level), "saved model-supported markers load into an expedition");
            require(game.player.grounded() && glm::distance(game.player.feetPosition(), parkour.spawn) < .001f,
                    "level spawn stands on the rotated crate at its fractional top height");
            InputActions jump; jump.active = jump.jump = jump.sprint = true;
            bool airborne = false;
            for (int frame = 0; frame < fps * 2 && game.levelController().screen() == LevelScreen::Play; ++frame) {
                jump.movement = game.player.feetPosition().z > parkour.finish.z + .05f ? glm::vec2(0, 1) : glm::vec2(0);
                game.update(jump, 1.0f / fps);
                airborne |= !game.player.grounded();
            }
            require(airborne && game.levelController().screen() == LevelScreen::Transition,
                    "a real running jump crosses the gap and reaches End on another model at multiple frame rates");
            InputActions idle; idle.active = true;
            for (int i = 0; i < 4; ++i) game.update(idle, .1f);
            require(game.levelController().sectionIndex() == 1 && game.player.grounded()
                    && glm::distance(game.player.feetPosition(), empty.spawn) < .001f,
                    "section transition replaces model collision before spawning in the next room");
            require(game.player.setFeetPosition(game.world, parkour.spawn), "old model location is clear in empty room");
            game.player.move(game.world, {0, -6, 0});
            require(game.player.grounded() && std::abs(game.player.feetPosition().y - 9) < .001f,
                    "no invisible crate remains after changing sections");
        }
        press(game, &InputActions::returnToHub);
        require(!game.levelActive() && game.player.grounded()
                && glm::distance(game.player.feetPosition(), hubSpawn) < .001f,
                "returning to the village restores hub models and the fitted-paving spawn");
        press(game, &InputActions::crossingMenu);
        InputActions crossing; crossing.active = true; crossing.selectedCrossing = 0; crossing.upgrade = true;
        game.update(crossing);
        require(game.rootCrossingActive() && game.player.grounded(),
                "legacy crossing prototypes clear village model collision on entry");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result = 1;
    }
    std::filesystem::remove_all(root);
    return result;
}
