#include "damage_feedback_smoke.h"
#include "camera.h"
#include "graphics/renderer.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {

using Pixels = std::vector<unsigned char>;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

std::size_t changed(const Pixels& a, const Pixels& b)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < a.size(); i += 4)
        count += std::abs(int(a[i]) - int(b[i]))
            + std::abs(int(a[i + 1]) - int(b[i + 1]))
            + std::abs(int(a[i + 2]) - int(b[i + 2])) > 18;
    return count;
}

} // namespace

void verifyDamageFeedback()
{
    constexpr int Width = 640, Height = 360;
    World world;
    for (int z = -7; z <= 8; ++z) for (int x = -8; x <= 8; ++x)
        world.set({x, 38, z}, Block::MossStone);
    for (int y = 39; y <= 46; ++y) for (int x = -8; x <= 8; ++x)
        world.set({x, y, -7}, Block::Stone);
    world.set({3, 41, 3}, Block::Lantern);
    Camera camera;
    camera.setPosition({.5f, 40.6f, 5.5f});
    camera.rotate(35, 25);
    WorldRenderer renderer;
    const auto draw = [&](std::span<const MonsterInstance> mobs,
                          std::span<const EncounterHit> hits) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, Width, Height);
        glDepthMask(GL_TRUE); glDepthRange(0, 1);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        renderer.render(world, camera, Width, Height, false, true, 1, false, false,
                        {}, {}, mobs, {}, 0, hits);
        Pixels pixels(static_cast<std::size_t>(Width) * Height * 4);
        glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        require(glGetError() == GL_NO_ERROR, "Damage feedback generated an OpenGL error");
        return pixels;
    };

    for (const auto kind : {MonsterKind::Skeleton, MonsterKind::Wizard, MonsterKind::Slime}) {
        MonsterInstance mob;
        mob.kind = kind; mob.position = {.5f, 39, .5f}; mob.seconds = .3f;
        mob.hitDirection = {0, 0, -1};
        const auto idle = draw(std::span(&mob, 1), {});
        const auto rebuilds = renderer.sceneRebuildCount();
        mob.hitReaction = .85f;
        const auto hurt = draw(std::span(&mob, 1), {});
        require(changed(idle, hurt) > 80, "Hurt mob lacks a visible flash and recoil");
        mob.hitReaction = 0;
        require(changed(idle, draw(std::span(&mob, 1), {})) == 0,
                "Hit flash leaks into recovered creatures or the held sword");

        EncounterHit hit;
        hit.position = mob.position + glm::vec3(0, .8f, .72f);
        hit.direction = {0, 0, -1}; hit.kind = kind; hit.age = .04f;
        const auto impact = draw(std::span(&mob, 1), std::span(&hit, 1));
        require(changed(idle, impact) > 12, "Sword impact lacks visible 3D debris");
        hit.age = .21f;
        require(changed(impact, draw(std::span(&mob, 1), std::span(&hit, 1))) > 12,
                "Impact debris does not move and shrink during its lifetime");
        hit.age = EncounterHit::Lifetime;
        require(changed(idle, draw(std::span(&mob, 1), std::span(&hit, 1))) == 0,
                "Expired impact debris remains visible");

        const auto empty = draw({}, {});
        hit.lethal = true; hit.age = .04f;
        require(changed(empty, draw({}, std::span(&hit, 1))) > 12,
                "Finishing hit vanishes along with its defeated mob");
        require(renderer.sceneRebuildCount() == rebuilds,
                "Transient damage effects rebuild static scenery");
    }

    // Impact geometry obeys scene depth, even when the enemy has disappeared.
    for (int y = 39; y <= 46; ++y) for (int x = -8; x <= 8; ++x)
        world.set({x, y, 3}, Block::Stone);
    EncounterHit behindWall;
    behindWall.position = {.5f, 40, .5f}; behindWall.age = .08f;
    const auto wall = draw({}, {});
    require(changed(wall, draw({}, std::span(&behindWall, 1))) == 0,
            "Impact debris is visible through a solid wall");
    std::cout << "Damage feedback passed: three hit reactions, moving impacts, lethal hits, expiry, depth and cache reuse\n";
}

} // namespace voxel
