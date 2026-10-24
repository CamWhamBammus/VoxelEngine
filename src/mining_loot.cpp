#include "loot.h"
#include "editing.h"
#include "ores.h"
#include "player.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace voxel {
namespace {
struct MinedOre {
    glm::ivec3 cell{};
    Block block = Block::Air;
    bool collected = false;
};

std::optional<MinedOre> parseOre(std::string_view record)
{
    int values[4]{};
    for (int i = 0; i < 4; ++i) {
        const auto separator = record.find(',');
        const auto token = record.substr(0, separator);
        const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), values[i]);
        if (error != std::errc{} || end != token.data() + token.size()) return {};
        if (i == 3) {
            if (separator != std::string_view::npos) return {};
        } else {
            if (separator == std::string_view::npos) return {};
            record.remove_prefix(separator + 1);
        }
    }
    if (values[3] < 0 || values[3] >= static_cast<int>(BlockCount)) return {};
    MinedOre ore{{values[0], values[1], values[2]}, static_cast<Block>(values[3])};
    if (!World::contains(ore.cell) || !oreInfo(ore.block)) return {};
    return ore;
}

bool clearSourcePath(const World& world, const AssetCollision& collision, glm::vec3 from, glm::vec3 to)
{
    const auto offset = to - from;
    const float distance = glm::length(offset);
    if (distance < .001f) return true;
    const auto direction = offset / distance;
    const auto voxel = raycast(from, direction, distance, [&](glm::ivec3 p) {
        return isSolid(world.get(p)) ? world.get(p) : Block::Air;
    });
    if (voxel && voxel->distance < distance - .001f) return false;
    const auto asset = collision.raycast(from, direction, distance);
    return !asset || asset->distance >= distance - .001f;
}

glm::vec3 oreLanding(const World& world, std::shared_ptr<const AssetCollision> collision, glm::ivec3 cell)
{
    const auto source = glm::vec3(cell) + .5f;
    Player probe;
    probe.setAssetCollision(collision);
    // Ceiling ore falls all the way to the floor. For ore set into a wall,
    // also inspect its exposed sides instead of leaving an item in the recess.
    for (int attempt = 0; attempt < 65; ++attempt) {
        const float angle = float((attempt - 1) % 16) * 6.2831853f / 16;
        constexpr float radii[]{1.0f, 1.75f, 2.5f, 3.5f};
        const float radius = attempt == 0 ? 0 : radii[(attempt - 1) / 16];
        const auto start = source + glm::vec3(std::cos(angle) * radius, 0, std::sin(angle) * radius);
        if (isSolid(world.get(glm::ivec3(glm::floor(start))))
            || !clearSourcePath(world, *collision, source, start)) continue;
        const float reach = start.y + 1;
        auto distance = reach;
        const auto voxel = raycast(start, {0,-1,0}, reach, [&](glm::ivec3 p) {
            return isSolid(world.get(p)) ? world.get(p) : Block::Air;
        });
        if (voxel) distance = voxel->distance;
        const auto asset = collision->raycast(start, {0,-1,0}, reach);
        if (asset) distance = std::min(distance, asset->distance);
        if (distance >= reach) continue;
        const auto surface = start - glm::vec3(0, distance, 0);
        // Uneven models can rise beside the center ray. Start above the local
        // surface and let the full body sweep find the actual supporting top.
        for (const float lift : {.08f, .002f, .25f, .5f}) {
            if (!probe.setFeetPosition(world, surface + glm::vec3(0, lift, 0))) continue;
            // A voxel stair ray hits its bounding cell; settle on its tread.
            probe.move(world, {0,-1,0});
            if (probe.grounded()) return probe.feetPosition();
        }
    }
    // Malformed rooms without a supported landing keep the item in the open
    // mined cell, so the reward is never silently discarded.
    return glm::vec3(cell) + glm::vec3(.5f, 0, .5f);
}
} // namespace

std::string LootSession::oreRecord(glm::ivec3 cell, Block block) const
{
    if (!World::contains(cell) || !oreInfo(block)) return {};
    return (section_.empty() ? "playtest" : section_) + "/ore-"
        + std::to_string(cell.x) + ',' + std::to_string(cell.y) + ',' + std::to_string(cell.z)
        + ',' + std::to_string(static_cast<int>(block));
}

void LootSession::appendOreDrop(const World& world, glm::ivec3 cell, Block block, bool animate)
{
    const auto landing = oreLanding(world, obstacles_, cell);
    HubAsset drop{HubAssetKind::ItemPickup, landing};
    drop.pickupItem = {oreInfo(block)->item, 1};
    Chest chest{drop.kind, authoredAssets_.size(), landing, oreRecord(cell, block), false, {}, {drop.pickupItem}};
    chest.oreSource = cell;
    if (animate) {
        chest.launchOffset = glm::vec3(cell) + .5f - (landing + PickupVisualCenter);
        chest.launchSeconds = 0;
        chest.pickupOffset = chest.launchOffset;
    }
    chests_.push_back(std::move(chest));
    authoredAssets_.push_back(std::move(drop));
}

void LootSession::settleOreDrops(const World& world)
{
    for (auto& chest : chests_) {
        if (!chest.oreSource || chest.contents[0].count == 0) continue;
        const auto landing = oreLanding(world, obstacles_, *chest.oreSource);
        if (glm::length(landing - chest.position) < .01f) continue;
        // Mining a lower ore can remove the support of an earlier drop.
        // Keep its current visible position, then animate to its new floor.
        chest.launchOffset = chest.position + chest.pickupOffset - landing;
        chest.position = landing;
        authoredAssets_[chest.assetIndex].position = landing;
        chest.launchSeconds = 0;
        chest.pickupOffset = chest.launchOffset;
        chest.pullSeconds = 0;
    }
}

void LootSession::releaseOre(World& world, glm::ivec3 cell, Block block)
{
    const auto record = oreRecord(cell, block);
    if (record.empty() || minedOreRecords_.contains(record)
        || (world.get(cell) != block && world.get(cell) != Block::Air)) return;
    if (!obstacles_) refreshAssets();
    world.set(cell, Block::Air);
    minedOreRecords_.insert(record);
    settleOreDrops(world);
    appendOreDrop(world, cell, block, true);
    refreshAssets();
}

void LootSession::restoreMinedOres(World& world, std::string_view section,
                                  const std::map<std::string, LootProgress>& progress)
{
    section_ = section.empty() ? "playtest" : section;
    const auto prefix = section_ + "/ore-";
    std::vector<MinedOre> mined;
    for (const auto& [record, saved] : progress) {
        if (!saved.unlocked || !record.starts_with(prefix) || minedOreRecords_.contains(record)) continue;
        auto ore = parseOre(std::string_view(record).substr(prefix.size()));
        if (!ore || record != oreRecord(ore->cell, ore->block)) continue;
        minedOreRecords_.insert(record);
        // An edited section can put a different material at an old coordinate.
        // Progress for the original block must not remove that replacement.
        if (world.get(ore->cell) != ore->block && world.get(ore->cell) != Block::Air) continue;
        ore->collected = saved.taken[0] > 0;
        mined.push_back(*ore);
    }
    if (mined.empty()) return;
    if (!obstacles_) refreshAssets();
    // Remove every saved voxel before choosing drop floors: adjacent or stacked
    // mined blocks must not become temporary supports during restoration.
    for (const auto& ore : mined) world.set(ore.cell, Block::Air);
    for (const auto& ore : mined)
        if (!ore.collected) appendOreDrop(world, ore.cell, ore.block, false);
    refreshAssets();
}
} // namespace voxel
