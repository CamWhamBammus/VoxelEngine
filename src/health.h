#pragma once

namespace voxel {

// Session state only; inventory and section files never store hit points.
class Health {
public:
    static constexpr int Maximum = 100;
    static constexpr float RespawnDelay = 1.1f;
    int current() const { return current_; }
    bool dead() const { return current_ == 0; }
    float fraction() const { return float(current_) / Maximum; }
    float damageFlash() const { return flash_ / .35f; }
    float respawnProgress() const;
    bool readyToRespawn() const;
    bool damage(int amount);
    void update(float seconds);
    void restore();

private:
    int current_ = Maximum;
    float flash_ = 0;
    float deadSeconds_ = 0;
};

// Short drops and ordinary jumps are safe. Water cancels falls in Player.
int fallDamage(float distance);

} // namespace voxel
