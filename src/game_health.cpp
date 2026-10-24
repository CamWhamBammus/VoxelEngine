#include "game.h"
#include "input.h"
#include <cmath>
#include <stdexcept>
#include <glm/geometric.hpp>

namespace voxel {

bool Game::healthVisible() const
{
    if (editorMode_ || monsterPreviewActive_ || menuOpen()) return false;
    if (devMode_) return sectionEditor_.screen() == SectionEditorScreen::Test;
    if (levelController_.open()) return levelController_.screen() == LevelScreen::Play;
    return true;
}

std::string_view Game::respawnDestination() const
{
    if (devMode_ && sectionEditor_.screen() == SectionEditorScreen::Test)
        return sectionEditor_.checkpointReached() ? "CHECKPOINT" : "SECTION START";
    if (levelController_.active())
        return levelController_.checkpointReached() ? "CHECKPOINT" : "SECTION START";
    if (rootCrossingActive()) return crossingCheckpointReached_ ? "CHECKPOINT" : "SECTION START";
    return "VILLAGE";
}

bool Game::damagePlayer(int amount, glm::vec3 direction)
{
    if (!healthVisible() || !health_.damage(amount)) return false;
    direction.y = 0;
    const float length = glm::length(direction);
    playerHitDirection_ = std::isfinite(length) && length > .001f
        ? direction / length : glm::vec3(0);
    playerKnockbackRemaining_ = health_.dead() || playerHitDirection_ == glm::vec3(0)
        ? 0 : PlayerKnockbackDuration;
    if (health_.dead()) {
        mining_.reset();
        miningMessage_.clear();
        miningMessageSeconds_ = 0;
        closeInventory();
        target.reset();
        status = "YOU DIED | RETURNING TO " + std::string(respawnDestination());
    }
    return true;
}

void Game::restoreHealth()
{
    mining_.reset();
    miningMessage_.clear();
    miningMessageSeconds_ = 0;
    health_.restore();
    resetPlayerHit();
    encounters_.resetUnfinished();
    player.consumeFallDistance();
}

void Game::resetPlayerHit()
{
    playerHitDirection_ = glm::vec3(0);
    playerKnockbackRemaining_ = 0;
    health_.clearDamageFlash();
}

bool Game::checkPlayerDamage(float deathHeight)
{
    const float drop = player.consumeFallDistance();
    if (player.feetPosition().y < deathHeight) damagePlayer(Health::Maximum);
    else damagePlayer(fallDamage(drop));
    return health_.dead();
}

void Game::updateDeath(const InputActions& actions, float deltaSeconds)
{
    if (!actions.active) return;
    health_.update(deltaSeconds);
    if (!health_.readyToRespawn()) return;
    // A respawn only repositions the player. It never reloads the section or
    // restarts its loot session, so collected items cannot be duplicated.
    if (devMode_) sectionEditor_.respawnAfterDeath(*this);
    else if (levelController_.active()) levelController_.respawnAfterDeath(*this);
    else if (rootCrossingActive()) resetRootCrossing(false,true);
    else {
        if (!returnToVillage()) throw std::runtime_error("Village spawn has no room for the player");
        status = "RETURNED TO THE VILLAGE | HEALTH RESTORED";
    }
}

} // namespace voxel
