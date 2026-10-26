#include "spawner_chains.h"

#include <map>
#include <set>
#include <stdexcept>

namespace voxel {
namespace {
std::set<std::uint32_t> identities(std::span<const HubAsset> assets)
{
    std::set<std::uint32_t> result;
    for (const auto& asset : assets)
        if (isSpawnerAsset(asset.kind) && asset.spawner.id) result.insert(asset.spawner.id);
    return result;
}

std::uint32_t allocateId(std::set<std::uint32_t>& used)
{
    // Reuse holes rather than overflowing after an imported UINT32_MAX ID.
    std::uint32_t id = 1;
    while (used.contains(id))
        if (++id == 0) throw std::runtime_error("No spawner identities available");
    used.insert(id);
    return id;
}
}

std::optional<std::size_t> spawnerById(std::span<const HubAsset> assets, std::uint32_t id)
{
    if (!id) return std::nullopt;
    for (std::size_t i = 0; i < assets.size(); ++i)
        if (isSpawnerAsset(assets[i].kind) && assets[i].spawner.id == id) return i;
    return std::nullopt;
}

std::string validateSpawnerChains(std::span<const HubAsset> assets)
{
    std::map<std::uint32_t, std::uint32_t> next;
    for (const auto& asset : assets) {
        if (!isSpawnerAsset(asset.kind)) {
            if (asset.spawner.id || asset.spawner.next) return "Only trial spawners can have links";
            continue;
        }
        const auto& settings = asset.spawner;
        if (!settings.id) {
            if (settings.next) return "Linked spawners need an identity";
            continue;
        }
        if (!next.emplace(settings.id, settings.next).second) return "Duplicate spawner identity";
    }
    std::set<std::uint32_t> destinations;
    for (const auto& [id, target] : next) {
        if (!target) continue;
        if (id == target) return "A spawner cannot link to itself";
        if (!next.contains(target)) return "The next spawner must be in this section";
        if (!destinations.insert(target).second) return "That spawner already follows another spawner";
    }
    // Visit each identity once, including long chains imported from a file.
    std::set<std::uint32_t> done;
    for (const auto& [id, unused] : next) {
        (void)unused;
        std::set<std::uint32_t> path;
        auto cursor = id;
        while (cursor && !done.contains(cursor)) {
            if (!path.insert(cursor).second) return "Spawner links cannot form a loop";
            cursor = next.at(cursor);
        }
        done.insert(path.begin(), path.end());
    }
    return {};
}

std::string linkSpawners(std::vector<HubAsset>& assets, std::size_t source,
                         std::optional<std::size_t> target)
{
    if (source >= assets.size() || !isSpawnerAsset(assets[source].kind)
        || (target && (*target >= assets.size() || !isSpawnerAsset(assets[*target].kind))))
        return "Choose a trial spawner";
    if (target == source) return "A spawner cannot link to itself";
    auto changed = assets;
    auto used = identities(changed);
    auto& from = changed[source].spawner;
    from.next = 0;
    if (target) {
        auto& to = changed[*target].spawner;
        if (!from.id) from.id = allocateId(used);
        if (!to.id) to.id = allocateId(used);
        from.next = to.id;
    }
    if (const auto error = validateSpawnerChains(changed); !error.empty()) return error;
    assets = std::move(changed);
    return {};
}

void pruneSpawnerLinks(std::vector<HubAsset>& assets)
{
    const auto present = identities(assets);
    for (auto& asset : assets)
        if (isSpawnerAsset(asset.kind) && asset.spawner.next && !present.contains(asset.spawner.next))
            asset.spawner.next = 0;
}

void remapCopiedSpawners(std::vector<HubAsset>& copies, std::span<const HubAsset> destination)
{
    pruneSpawnerLinks(copies);
    auto used = identities(destination);
    std::map<std::uint32_t, std::uint32_t> remap;
    for (auto& asset : copies) {
        if (!isSpawnerAsset(asset.kind) || !asset.spawner.id) continue;
        const auto old = asset.spawner.id;
        asset.spawner.id = allocateId(used);
        remap.emplace(old, asset.spawner.id);
    }
    for (auto& asset : copies)
        if (isSpawnerAsset(asset.kind) && asset.spawner.next)
            asset.spawner.next = remap.at(asset.spawner.next);
}

} // namespace voxel
