#include "loot.h"
#include "editing.h"
#include "world.h"

#include <bit>

namespace voxel {
void LootSession::begin(std::span<const HubAsset> assets, std::uint32_t seed)
{
    clear();
    for (const auto& asset : assets) {
        if (asset.kind != HubAssetKind::LootChest) continue;
        auto roll = seed;
        for (int axis = 0; axis < 3; ++axis)
            roll = (roll ^ std::bit_cast<std::uint32_t>(asset.position[axis])) * 16777619u;
        Chest chest{asset.variation, {}, chestLoot(asset.variation, roll)};
        chest.collision.rebuild(std::span(&asset, 1));
        chests_.push_back(std::move(chest));
    }
    if (!chests_.empty()) obstacles_.rebuild(assets);
}

void LootSession::clear()
{
    chests_.clear();
    obstacles_.clear();
}

std::optional<std::size_t> LootSession::target(const World& world, glm::vec3 eye, glm::vec3 direction) const
{
    constexpr float Reach = 4.0f;
    if (chests_.empty()) return {};
    const auto surface = obstacles_.raycast(eye, direction, Reach);
    if (!surface) return {};
    const auto block = raycast(eye, direction, Reach, [&](glm::ivec3 cell) {
        const auto material = world.get(cell);
        return isSolid(material) ? material : Block::Air;
    });
    if (block && block->distance < surface->distance - .001f) return {};
    for (std::size_t i = 0; i < chests_.size(); ++i) {
        const auto chest = chests_[i].collision.raycast(eye, direction, Reach);
        if (chest && chest->distance <= surface->distance + .001f) return i;
    }
    return {};
}

std::string_view LootSession::name(std::size_t index) const
{
    return chestLootName(chests_.at(index).variation);
}
} // namespace voxel
