#include "loot.h"
#include "editing.h"
#include "world.h"

#include <algorithm>
#include <stdexcept>

namespace voxel {
void LootSession::begin(std::span<const HubAsset> assets, std::string_view section,
                        const std::map<std::string, LootProgress>& progress)
{
    clear();
    authoredAssets_.assign(assets.begin(), assets.end());
    for (std::size_t i = 0; i < assets.size(); ++i) {
        const auto& asset = assets[i];
        if (!validChestItems(asset)) throw std::invalid_argument("Invalid authored chest contents");
        if (!isChestAsset(asset.kind) && !isItemPickup(asset.kind)) continue;
        const auto key = std::string(section.empty() ? "playtest" : section) + "/asset-" + std::to_string(i);
        const auto saved = progress.find(key);
        Chest chest{asset.kind, i, key, asset.kind == HubAssetKind::IronboundChest
            && (saved == progress.end() || !saved->second.unlocked), {},
            isItemPickup(asset.kind) ? std::vector<ItemStack>{itemPickupStack(asset)} : asset.chestItems};
        if (saved != progress.end())
            for (std::size_t slot = 0; slot < chest.contents.size(); ++slot)
                chest.contents[slot].count = std::max(0, chest.contents[slot].count - saved->second.taken[slot]);
        chest.collision.rebuild(std::span(&asset, 1));
        chests_.push_back(std::move(chest));
    }
    refreshAssets();
}

void LootSession::clear()
{
    chests_.clear();
    authoredAssets_.clear();
    sceneAssets_.clear();
    obstacles_.reset();
}

void LootSession::refreshAssets()
{
    auto visible = authoredAssets_;
    // Reverse placement order preserves document indices while removing pickups.
    for (auto i = chests_.rbegin(); i != chests_.rend(); ++i)
        if (isItemPickup(i->kind) && i->contents[0].count == 0)
            visible.erase(visible.begin() + static_cast<std::ptrdiff_t>(i->assetIndex));
    if (!obstacles_ || visible != sceneAssets_) {
        sceneAssets_ = std::move(visible);
        obstacles_ = std::make_shared<AssetCollision>();
        obstacles_->rebuild(sceneAssets_);
    }
}

std::optional<std::size_t> LootSession::target(const World& world, glm::vec3 eye, glm::vec3 direction) const
{
    constexpr float Reach = 4.0f;
    if (chests_.empty() || !obstacles_) return {};
    const auto surface = obstacles_->raycast(eye, direction, Reach);
    if (!surface) return {};
    const auto block = raycast(eye, direction, Reach, [&](glm::ivec3 cell) {
        const auto material = world.get(cell);
        return isSolid(material) ? material : Block::Air;
    });
    if (block && block->distance < surface->distance - .001f) return {};
    for (std::size_t i = 0; i < chests_.size(); ++i) {
        if (pickup(i) && chests_[i].contents[0].count == 0) continue;
        const auto chest = chests_[i].collision.raycast(eye, direction, Reach);
        if (chest && chest->distance <= surface->distance + .001f) return i;
    }
    return {};
}

std::string_view LootSession::name(std::size_t index) const
{
    return pickup(index) ? itemInfo(chests_.at(index).contents[0].item).name : chestAssetName(chests_.at(index).kind);
}
} // namespace voxel
