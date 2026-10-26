#pragma once

#include "sections.h"

namespace voxel {

// A starting canvas, baked into ordinary editable blocks and assets when built.
// Choosing another preset never changes an existing section or its saved data.
enum class SectionPreset { WorldTheme, PlainStone };
enum class SectionLayout { Parkour, Cave, Arena };

std::string_view sectionPresetName(SectionWorld world, SectionPreset preset);
std::string_view sectionPresetDescription(SectionWorld world, SectionPreset preset);

// All layouts keep the protected shell and safe marker positions.
// Cave geology extends through the fill so newly carved rooms keep the theme.
// Cave presets include no lamps or glowing materials; the author places lights.
// Arena presets include cover, walkable galleries and three linked trial waves.
void buildSectionPreset(World& world, std::vector<HubAsset>& assets,
                        SectionWorld theme, SectionLayout layout, SectionPreset preset);

} // namespace voxel
