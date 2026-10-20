#pragma once

#include "asset_collision.h"
#include "items.h"

#include <optional>
#include <span>
#include <vector>

namespace voxel {
class World;

// One set of chest contents per visit to a section. Returning to a checkpoint
// keeps these contents; starting a fresh section creates a new set.
class LootSession {
public:
    void begin(std::span<const HubAsset> assets, std::uint32_t seed);
    void clear();
    std::optional<std::size_t> target(const World& world, glm::vec3 eye, glm::vec3 direction) const;
    std::vector<ItemStack>& contents(std::size_t index) { return chests_.at(index).contents; }
    const std::vector<ItemStack>& contents(std::size_t index) const { return chests_.at(index).contents; }
    std::string_view name(std::size_t index) const;

private:
    struct Chest {
        std::uint32_t variation;
        AssetCollision collision;
        std::vector<ItemStack> contents;
    };
    AssetCollision obstacles_;
    std::vector<Chest> chests_;
};
} // namespace voxel
