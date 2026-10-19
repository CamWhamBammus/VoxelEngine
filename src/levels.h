#pragma once

#include "sections.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace voxel {

inline constexpr std::size_t LevelMaxSections = 64;
inline constexpr int LevelMaxNumber = 999;

struct LevelDefinition {
    SectionWorld world = SectionWorld::LushCaves;
    int number = 1;
    std::string name = "Untitled level";
    // Paths are relative to the section library. Each numbered accepted file
    // pins a revision; accepting a later edit never changes an existing level.
    // Order and duplicates are intentional: a section can be used more than once.
    std::vector<std::filesystem::path> sections;
};

struct LevelEntry {
    std::filesystem::path path;
    LevelDefinition definition;
};

// Validate every referenced accepted section, one at a time. A published level
// must contain 1 to 64 tested sections from its world with safe markers.
void validateLevel(const LevelDefinition& definition, const std::filesystem::path& sectionRoot);
SectionDocument resolveLevelSection(const LevelDefinition& definition, std::size_t index,
                                    const std::filesystem::path& sectionRoot);

// Saves levels/<world>/level-NNN.vlevel. Validation finishes before writing, and
// a claimed number cannot be replaced unless overwrite is explicitly requested.
std::filesystem::path saveLevel(const LevelDefinition& definition,
                                const std::filesystem::path& sectionRoot,
                                const std::filesystem::path& levelRoot,
                                bool overwrite = false);
LevelDefinition loadLevel(const std::filesystem::path& path);
// Listing checks manifests only; resolve/validate before play to detect missing
// section files. Corrupt or misplaced manifests do not hide healthy levels.
std::vector<LevelEntry> listLevels(const std::filesystem::path& levelRoot,
                                  std::optional<SectionWorld> filter = std::nullopt);

// One independent autosave per world, under levels/drafts/<world>.vlevel.
// Drafts can have an empty sequence and never replace published levels.
// Reference syntax is checked, but missing section files can be kept in a draft
// while the author repairs it. Full content validation happens before publication.
std::filesystem::path saveLevelDraft(const LevelDefinition& definition,
                                     const std::filesystem::path& sectionRoot,
                                     const std::filesystem::path& levelRoot);
std::optional<LevelDefinition> loadLevelDraft(const std::filesystem::path& levelRoot,
                                             SectionWorld world);

} // namespace voxel
