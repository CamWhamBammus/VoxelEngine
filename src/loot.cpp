#include "loot.h"
#include "editing.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace voxel {
namespace {
glm::vec3 pickupCenter(glm::vec3 anchor) { return anchor + PickupVisualCenter; }

bool clearPath(const World& world, const AssetCollision& obstacles, glm::vec3 from, glm::vec3 to)
{
    const auto offset = to - from;
    const float distance = glm::length(offset);
    if (distance < .001f) return !isSolid(world.get(glm::ivec3(glm::floor(to))));
    const auto direction = offset / distance;
    const auto block = raycast(from, direction, distance, [&](glm::ivec3 cell) {
        const auto material = world.get(cell);
        return isSolid(material) ? material : Block::Air;
    });
    if (block && block->distance < distance - .001f) return false;
    const auto model = obstacles.raycast(from, direction, distance);
    return !model || model->distance >= distance - .001f;
}

std::optional<float> pickupRay(glm::vec3 center, glm::vec3 eye, glm::vec3 direction)
{
    constexpr float Radius = .3f;
    const auto offset = eye - center;
    const float along = glm::dot(offset, direction);
    const float squared = glm::dot(offset, offset) - Radius * Radius;
    const float discriminant = along * along - squared;
    if (discriminant < 0 || (squared > 0 && along > 0)) return {};
    return std::max(0.0f, -along - std::sqrt(discriminant));
}
} // namespace

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
        Chest chest{asset.kind, i, asset.position, key, asset.kind == HubAssetKind::IronboundChest
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
    scenePickupOffsets_.clear();
    obstacles_.reset();
}

void LootSession::refreshAssets()
{
    sceneAssets_.clear();
    scenePickupOffsets_.clear();
    std::size_t reward = 0;
    for (std::size_t i = 0; i < authoredAssets_.size(); ++i) {
        glm::vec3 offset{};
        if (reward < chests_.size() && chests_[reward].assetIndex == i) {
            auto& chest = chests_[reward++];
            chest.sceneIndex = sceneAssets_.size();
            if (isItemPickup(chest.kind) && chest.contents[0].count == 0) continue;
            offset = chest.pickupOffset;
        }
        sceneAssets_.push_back(authoredAssets_[i]);
        scenePickupOffsets_.push_back(offset);
    }
    // Pickup visibility changes without changing any physical scenery.
    if (!obstacles_) {
        obstacles_ = std::make_shared<AssetCollision>();
        obstacles_->rebuild(sceneAssets_);
    }
}

std::optional<std::size_t> LootSession::target(const World& world, glm::vec3 eye, glm::vec3 direction) const
{
    constexpr float Reach = 4.0f;
    if (chests_.empty() || !obstacles_) return {};
    const float length = glm::length(direction);
    if (!std::isfinite(length) || length < .001f) return {};
    direction /= length;
    const auto surface = obstacles_->raycast(eye, direction, Reach);
    float nearest = surface ? surface->distance + .001f : Reach;
    const auto block = raycast(eye, direction, Reach, [&](glm::ivec3 cell) {
        const auto material = world.get(cell);
        return isSolid(material) ? material : Block::Air;
    });
    if (block) nearest = std::min(nearest, block->distance);
    std::optional<std::size_t> result;
    for (std::size_t i = 0; i < chests_.size(); ++i) {
        if (pickup(i) && chests_[i].contents[0].count == 0) continue;
        if (pickup(i)) {
            const auto hit = pickupRay(pickupCenter(chests_[i].position) + chests_[i].pickupOffset, eye, direction);
            if (hit && *hit < nearest) { nearest = *hit; result = i; }
        } else {
            const auto chest = chests_[i].collision.raycast(eye, direction, Reach);
            if (chest && chest->distance <= nearest) { nearest = chest->distance; result = i; }
        }
    }
    return result;
}

bool LootSession::canCollectPickup(std::size_t index, const World& world, glm::vec3 feet) const
{
    if (!pickup(index) || chests_[index].contents[0].count == 0 || !obstacles_) return false;
    const auto anchor = chests_[index].position;
    const auto delta = feet - anchor;
    if (std::hypot(delta.x, delta.z) > 1.5f || std::abs(delta.y) > .8f) return false;
    // Check from the lower body, so a floor, low wall or solid model between
    // the player and a drop blocks collection even when it is visible above it.
    const auto body = feet + glm::vec3(0, .45f, 0);
    return clearPath(world, *obstacles_, body, pickupCenter(anchor))
        && clearPath(world, *obstacles_, body, pickupCenter(anchor) + chests_[index].pickupOffset);
}

bool LootSession::pullPickup(std::size_t index, glm::vec3 feet, float deltaSeconds)
{
    if (!pickup(index) || chests_[index].contents[0].count == 0
        || !std::isfinite(deltaSeconds) || deltaSeconds <= 0) return false;
    auto& chest = chests_[index];
    const float seconds = std::min(deltaSeconds, .1f);
    const auto goal = feet + glm::vec3(0, .65f, 0) - pickupCenter(chest.position);
    const auto delta = goal - chest.pickupOffset;
    const float distance = glm::length(delta);
    if (distance > .001f)
        chest.pickupOffset += delta * std::min(1.0f, seconds * 8.0f / distance);
    chest.pullSeconds += seconds;
    scenePickupOffsets_[chest.sceneIndex] = chest.pickupOffset;
    return chest.pullSeconds >= .12f && glm::length(goal - chest.pickupOffset) <= .12f;
}

void LootSession::stopPickupPull(std::size_t index)
{
    if (!pickup(index)) return;
    auto& chest = chests_[index];
    chest.pickupOffset = {};
    chest.pullSeconds = 0;
    if (chest.contents[0].count > 0) scenePickupOffsets_[chest.sceneIndex] = {};
}

std::string_view LootSession::name(std::size_t index) const
{
    return pickup(index) ? itemInfo(chests_.at(index).contents[0].item).name : chestAssetName(chests_.at(index).kind);
}
} // namespace voxel
