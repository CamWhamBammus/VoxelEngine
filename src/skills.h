#pragma once

#include <span>
#include <string_view>
#include <glm/vec2.hpp>

namespace voxel {

enum class SkillIcon {
    Boots, Chestplate, Feather, Cairn, Scarf, Fern,
    Pickaxe, Lantern, Compass, Ore, Sword, Shield, Leaf, Wave, Root, Eye
};

struct SkillEntry {
    std::string_view name;
    std::string_view description;
    SkillIcon icon;
};

// A display catalog only. These ideas grant no abilities and have no prices.
std::span<const std::string_view> skillTabNames();
std::span<const SkillEntry> skillEntries(int tab);

} // namespace voxel
