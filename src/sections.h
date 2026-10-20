#pragma once

#include "hub_assets.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace voxel {
class AssetCollision;

// These IDs are stored in .vxs files. Append entries without renumbering.
enum class SectionWorld { LushCaves, CrystalHollows, EmberDepths };
enum class SectionCategory { Parkour, Puzzle, Exploration, Mining, Treasure };

struct SectionWorldInfo {
    SectionWorld id;
    std::string_view name, description, folder;
};
struct SectionCategoryInfo {
    SectionCategory id;
    std::string_view name, description, folder;
};
struct SectionAssetInfo {
    HubAssetKind kind;
    std::string_view name, group;
};
std::span<const SectionWorldInfo> sectionWorldCatalog();
std::span<const SectionCategoryInfo> sectionCategoryCatalog();
std::span<const SectionAssetInfo> sectionAssetCatalog();

inline constexpr glm::ivec3 SectionInteriorMin{-32, 9, -32};
inline constexpr glm::ivec3 SectionInteriorMax{32, 57, 32};
inline constexpr std::size_t SectionMaxAssets = 4096;
constexpr bool sectionInterior(glm::ivec3 cell)
{
    return cell.x >= SectionInteriorMin.x && cell.x < SectionInteriorMax.x
        && cell.y >= SectionInteriorMin.y && cell.y < SectionInteriorMax.y
        && cell.z >= SectionInteriorMin.z && cell.z < SectionInteriorMax.z;
}

struct SectionMetadata {
    SectionWorld world = SectionWorld::LushCaves;
    SectionCategory category = SectionCategory::Parkour;
    std::string name = "Untitled section";
    // Exactly one Start and one End, at the center of the player's feet.
    // Their original field names are retained for existing .vxs documents.
    glm::vec3 spawn{0.5f, 9.0f, 24.5f};
    glm::vec3 finish{0.5f, 9.0f, -24.5f};
    std::optional<glm::vec3> checkpoint;
    bool tested = false;
};

struct SectionDocument {
    World world;
    SectionMetadata meta;
    // Solid model surfaces use the same collision shapes in tests and expeditions.
    std::vector<HubAsset> assets;
};
struct SectionEntry {
    std::filesystem::path path;
    SectionMetadata meta;
};

// Clears the old scene and makes a closed 64 x 48 x 64 stone building room.
// The shell is outside the editable interior and is protected by the editor.
void buildBlankSection(World& world);
// A marker needs full player clearance and support under the player's footprint.
bool sectionMarkerSafe(const World& world, glm::vec3 feet, const AssetCollision* assets = nullptr);

// Version 2 .vxs documents contain metadata, models and their authored chest
// contents, and an entire voxel snapshot. Version 1 chest presets import once as
// fixed contents. Writes are atomic; loading never rewrites a file.
void saveSection(const World& world, const SectionMetadata& meta,
                 std::span<const HubAsset> assets, const std::filesystem::path& path,
                 bool overwrite = false);
SectionDocument loadSection(const std::filesystem::path& path);

// Both helpers create a new numbered entry, preserving earlier work. Keep the
// returned draft path and use saveSection(..., true) for subsequent draft saves.
// Accepting requires a tested section with safe start, checkpoint, and finish.
std::filesystem::path saveSectionDraft(const World& world, const SectionMetadata& meta,
                                      std::span<const HubAsset> assets,
                                      const std::filesystem::path& libraryRoot);
std::filesystem::path acceptSection(const World& world, const SectionMetadata& meta,
                                   std::span<const HubAsset> assets,
                                   const std::filesystem::path& libraryRoot);
// Malformed files are skipped so one corrupt entry cannot hide the library.
std::vector<SectionEntry> listSections(const std::filesystem::path& libraryRoot,
                                     std::optional<SectionWorld> filter = std::nullopt);
std::vector<SectionEntry> listSectionDrafts(const std::filesystem::path& libraryRoot);

} // namespace voxel
