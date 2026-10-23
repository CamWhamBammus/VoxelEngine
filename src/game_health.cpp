#include "game.h"
#include "input.h"
#include <stdexcept>

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

bool Game::damagePlayer(int amount)
{
    if (!healthVisible() || !health_.damage(amount)) return false;
    if (health_.dead()) {
        closeInventory();
        target.reset();
        status = "YOU DIED | RETURNING TO " + std::string(respawnDestination());
    }
    return true;
}

void Game::restoreHealth()
{
    health_.restore();
    encounters_.resetUnfinished();
    player.consumeFallDistance();
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
