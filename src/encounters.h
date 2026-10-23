#pragma once

#include "asset_collision.h"
#include "monsters.h"
#include "player.h"

namespace voxel {

struct MobHealthBar {
    glm::vec3 position{0};
    int current = 0;
    int maximum = 0;
    float visibleSeconds = 0;
};

struct EncounterProjectile {
    glm::vec3 position{0};
    glm::vec3 previousPosition{0};
    glm::vec3 velocity{0};
    float remainingSeconds = 0;
};

// One walking section owns one session. Authored placements stay unchanged;
// the caller persists completions and releases their rewards before marking
// them cleared. An unsuccessful save therefore leaves a completion pending.
class EncounterSession {
public:
    static constexpr float ActivationRadius = 8.0f;
    static constexpr std::size_t MaxAlive = 8;
    static constexpr int SwordDamage = 25;
    static constexpr float SwordReach = 3.0f;
    static constexpr float SwordCooldown = .45f;

    void begin(std::span<const HubAsset> assets,
               std::shared_ptr<const AssetCollision> collision,
               std::span<const std::size_t> cleared = {});
    void clear();
    // Returns damage to apply to the player; menus and death pause by not
    // calling update. Holding the sword button repeats at its fixed cooldown.
    int update(const World& world, const Player& player, glm::vec3 aimDirection,
               bool swordAttack, float deltaSeconds);
    void resetUnfinished();
    void markCleared(std::size_t assetIndex);

    bool active() const;
    std::size_t remaining() const;
    float swordSwing() const { return swordCooldown_ / SwordCooldown; }
    std::span<const MonsterInstance> monsters() const { return monsters_; }
    std::span<const MobHealthBar> healthBars() const { return bars_; }
    std::span<const EncounterProjectile> projectiles() const { return projectiles_; }
    std::span<const std::size_t> pendingCompletions() const { return completions_; }

private:
    struct Spawner {
        std::size_t assetIndex;
        glm::vec3 position;
        std::array<int, 3> counts;
        std::array<int, 3> toSpawn;
        float spawnDelay = .45f;
        int nextKind = 0;
        int spawnAttempt = 0;
        bool active = false;
        bool complete = false;
        bool cleared = false;
    };
    struct Mob {
        Player body;
        MonsterInstance model;
        std::size_t spawner;
        int health;
        int maximum;
        float age = 0;
        float healthVisible = 0;
        float cooldown = 1.0f;
        float attackTime = -1;
        bool attackFired = false;
        float hopDelay = .2f;
        glm::vec3 attackDirection{0, 0, 1};
    };

    bool spawnMob(const World& world, const Player& player, std::size_t spawner);
    void swingSword(const World& world, const Player& player, glm::vec3 direction);
    int updateMob(Mob& mob, const World& world, const Player& player, float seconds);
    void refreshViews();
    void checkCompletions();

    std::shared_ptr<const AssetCollision> collision_;
    std::vector<Spawner> spawners_;
    std::vector<Mob> mobs_;
    std::vector<MonsterInstance> monsters_;
    std::vector<MobHealthBar> bars_;
    std::vector<EncounterProjectile> projectiles_;
    std::vector<std::size_t> completions_;
    float swordCooldown_ = 0;
    float hurtCooldown_ = 0;
};

} // namespace voxel
