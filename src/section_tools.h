#pragma once

#include "sections.h"

namespace voxel {

// Both corners are included. Call normalizedSectionBox for user-picked corners.
struct SectionBox { glm::ivec3 min, max; };
SectionBox normalizedSectionBox(glm::ivec3 a, glm::ivec3 b);
bool validSectionBox(const SectionBox& box);

enum class SectionFillMode { Solid, Hollow, Erase };
struct SectionBlockEdit {
    glm::ivec3 position;
    Block before, after;
};
struct SectionClipboard {
    glm::ivec3 size{0};
    // x + size.x * (y + size.y * z), including air. Model origins are relative
    // to the selection's minimum corner. Start/end/checkpoint are never copied.
    std::vector<Block> blocks;
    std::vector<HubAsset> assets;
    bool empty() const;
};
struct SectionEditPlan {
    std::vector<SectionBlockEdit> blocks;
    std::vector<HubAsset> assets; // The complete resulting model list.
    std::string error;
};

SectionClipboard copySectionBox(const World& world, std::span<const HubAsset> assets,
                                const SectionBox& box);
glm::ivec3 rotatedSectionSize(const SectionClipboard& clipboard, int quarterTurns);

// Planning never modifies the world. Failure produces no block edits and keeps
// the original models, so a command can be applied/undone as one atomic edit.
// Hollow builds the boundary only, preserving the selection's existing interior.
SectionEditPlan planSectionFill(const World& world, std::span<const HubAsset> assets,
                               const SectionBox& box, Block material, SectionFillMode mode,
                               glm::vec3 cameraPosition);
// Positive turns match HubAsset::facing: local +Z rotates toward +X.
// includeAir replaces the whole destination (including its anchored models).
SectionEditPlan planSectionPaste(const World& world, std::span<const HubAsset> assets,
                                const SectionClipboard& clipboard, glm::ivec3 origin,
                                int quarterTurns, bool includeAir, glm::vec3 cameraPosition);

} // namespace voxel
