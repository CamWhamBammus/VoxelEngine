#pragma once

#include "hub_assets.h"
#include <optional>
#include <string>

namespace voxel {

std::optional<std::size_t> spawnerById(std::span<const HubAsset> assets, std::uint32_t id);
// A chain is linear and stays inside one section. An empty string means valid.
std::string validateSpawnerChains(std::span<const HubAsset> assets);
// Changes both endpoint identities and the link atomically, or returns an error.
std::string linkSpawners(std::vector<HubAsset>& assets, std::size_t source,
                         std::optional<std::size_t> target);
// Deleting a target disconnects its predecessor; it never silently skips a fight.
void pruneSpawnerLinks(std::vector<HubAsset>& assets);
// Copied chains are independent. Links outside the copied selection are dropped.
void remapCopiedSpawners(std::vector<HubAsset>& copies, std::span<const HubAsset> destination);

} // namespace voxel
