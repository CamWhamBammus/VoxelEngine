#pragma once

#include "sections.h"

namespace voxel {

// A starting canvas, baked into ordinary editable blocks and assets when built.
// Choosing another preset never changes an existing section or its saved data.
enum class SectionPreset { WorldTheme, PlainStone };

std::string_view sectionPresetName(SectionWorld world, SectionPreset preset);
std::string_view sectionPresetDescription(SectionWorld world, SectionPreset preset);

// Keeps the blank/cave templates' protected shell and safe marker positions.
// Cave geology extends through the fill so newly carved rooms keep the theme.
// Cave presets include no lamps or glowing materials; the author places lights.
void buildSectionPreset(World& world, std::vector<HubAsset>& assets,
                        SectionWorld theme, bool cave, SectionPreset preset);

} // namespace voxel
