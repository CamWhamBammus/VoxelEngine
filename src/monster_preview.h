#pragma once

#include "monsters.h"
#include "hub_assets.h"
#include <array>

namespace voxel {

// A small animation workbench. It owns no save files or combat state.
class MonsterPreview {
public:
    void build(World& world);
    void update(float seconds);
    void select(MonsterAnimation animation);
    void automatic();
    bool isAutomatic() const { return automatic_; }
    MonsterAnimation animation() const { return monsters_[0].animation; }
    std::span<const MonsterInstance> monsters() const { return monsters_; }
    std::span<const HubAsset> scenery() const { return scenery_; }
    static constexpr glm::vec3 Spawn{0,9,7.5f};

private:
    std::array<MonsterInstance,3> monsters_{};
    std::vector<HubAsset> scenery_;
    float elapsed_ = 0;
    bool automatic_ = true;
};

} // namespace voxel
