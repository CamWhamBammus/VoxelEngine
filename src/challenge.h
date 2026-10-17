#pragma once

#include "world.h"

#include <array>

namespace voxel {

enum class ChallengeState { Playing, Won, Lost };
enum class Interaction { None, Collected, NeedCrystals, Won, Finished };

struct Crystal {
    glm::vec3 position{0.0f};
    bool collected = false;
};

// Collect all three crystals, then interact with camp to finish the trail.
// Positions are marker centers; interactions measure distance from the eyes.
class Challenge {
public:
    static constexpr float InteractionReach = 3.0f;
    static constexpr float TimeLimit = 300.0f;
    static constexpr float FallPenalty = 20.0f;

    void reset(const World& world, glm::vec3 camp);
    Interaction interact(glm::vec3 eye);
    // Consume elapsed game time; releasing the mouse can pause the countdown.
    void update(float seconds, bool paused = false);
    void penalizeFall();

    const std::array<Crystal, 3>& crystals() const { return crystals_; }
    glm::vec3 camp() const { return camp_; }
    int collectedCount() const;
    ChallengeState state() const { return state_; }
    float timeRemaining() const { return timeRemaining_; }

private:
    std::array<Crystal, 3> crystals_{};
    glm::vec3 camp_{0.0f};
    ChallengeState state_ = ChallengeState::Playing;
    float timeRemaining_ = TimeLimit;
};

} // namespace voxel
