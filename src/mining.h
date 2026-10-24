#pragma once

#include "ores.h"
#include <optional>

namespace voxel {

struct MiningTarget {
    glm::ivec3 position{};
    Block block = Block::Air;
    bool operator==(const MiningTarget&) const = default;
};

// Input and timing only. Game resolves visibility and commits a completed break.
class MiningSession {
public:
    static constexpr float Reach = 4.5f;
    static constexpr float SwingDuration = .48f;
    std::optional<MiningTarget> update(std::optional<MiningTarget> target,
                                      bool held, int pickaxeTier, float seconds);
    float swing() const { return swingRemaining_ / SwingDuration; }
    float progress() const { return progress_; }
    std::optional<MiningTarget> target() const { return target_; }
    bool blocked() const { return blocked_; }
    void cancelTarget();
    void reset();

private:
    std::optional<MiningTarget> target_;
    float worked_ = 0;
    float progress_ = 0;
    float swingRemaining_ = 0;
    bool blocked_ = false;
};

} // namespace voxel
