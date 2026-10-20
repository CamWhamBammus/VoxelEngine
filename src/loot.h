#pragma once

#include "asset_collision.h"
#include "equipment.h"

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace voxel {
class World;

// Documents describe rewards; the profile remembers permanent claims. This
// session owns what remains while the player is in the room.
class LootSession {
public:
    void begin(std::span<const HubAsset> assets, std::string_view section = {},
               const std::map<std::string, LootProgress>& progress = {});
    void clear();
    std::optional<std::size_t> target(const World& world, glm::vec3 eye, glm::vec3 direction) const;
    std::vector<ItemStack>& contents(std::size_t index) { return chests_.at(index).contents; }
    const std::vector<ItemStack>& contents(std::size_t index) const { return chests_.at(index).contents; }
    std::string_view name(std::size_t index) const;
    const std::string& recordKey(std::size_t index) const { return chests_.at(index).recordKey; }
    bool pickup(std::size_t index) const { return isItemPickup(chests_.at(index).kind); }
    bool locked(std::size_t index) const { return chests_.at(index).locked; }
    void unlock(std::size_t index) { chests_.at(index).locked = false; }
    void refreshAssets();
    const std::vector<HubAsset>& sceneAssets() const { return sceneAssets_; }
    std::shared_ptr<const AssetCollision> collision() const { return obstacles_; }

private:
    struct Chest {
        HubAssetKind kind;
        std::size_t assetIndex;
        std::string recordKey;
        bool locked;
        AssetCollision collision;
        std::vector<ItemStack> contents;
    };
    std::shared_ptr<AssetCollision> obstacles_;
    std::vector<HubAsset> authoredAssets_, sceneAssets_;
    std::vector<Chest> chests_;
};
} // namespace voxel
