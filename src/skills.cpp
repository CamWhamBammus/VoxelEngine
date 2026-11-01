#include "skills.h"

#include <array>

namespace voxel {
namespace {
constexpr std::array<std::string_view,4> Tabs{"Armor", "Tools", "Combat", "Exploration"};
constexpr std::array<std::array<SkillEntry,6>,4> Entries{{
    {{{"Rootbound Leap", "A stronger jump from your boots.", SkillIcon::Boots},
      {"Stoneguard", "Stand firm behind your armor.", SkillIcon::Chestplate},
      {"Soft Landing", "Touch down with a lighter step.", SkillIcon::Feather},
      {"Sure Footing", "Keep your balance on rough ground.", SkillIcon::Cairn},
      {"Second Wind", "Catch your breath after a close call.", SkillIcon::Scarf},
      {"Deep Breath", "Explore beneath the water longer.", SkillIcon::Fern}}},
    {{{"Deep Strike", "Put more force into your pickaxe.", SkillIcon::Pickaxe},
      {"Guiding Light", "Carry a wider pool of lantern light.", SkillIcon::Lantern},
      {"Trail Sense", "Let your compass reveal a way ahead.", SkillIcon::Compass},
      {"Ore Whisper", "Notice hidden mineral seams nearby.", SkillIcon::Ore},
      {"Root Reader", "Find clues in the roots around you.", SkillIcon::Root},
      {"Careful Hands", "Recover delicate finds intact.", SkillIcon::Leaf}}},
    {{{"Steady Guard", "Meet a blow with a steady defense.", SkillIcon::Shield},
      {"Heavy Swing", "Commit to a stronger sword strike.", SkillIcon::Sword},
      {"Quickstep", "Move aside when danger closes in.", SkillIcon::Boots},
      {"Riposte", "Answer a well-timed defense.", SkillIcon::Sword},
      {"Clear Sight", "Read an enemy before it strikes.", SkillIcon::Eye},
      {"Last Stand", "Hold your ground when cornered.", SkillIcon::Chestplate}}},
    {{{"Hidden Paths", "Spot passages concealed by growth.", SkillIcon::Fern},
      {"Water Strider", "Travel more easily through water.", SkillIcon::Wave},
      {"Wayfinder", "Recognize signs left by explorers.", SkillIcon::Compass},
      {"Canopy Reach", "Discover routes among the roots.", SkillIcon::Root},
      {"Quiet Step", "Move softly through the cave.", SkillIcon::Feather},
      {"Keen Eye", "Notice treasures others overlook.", SkillIcon::Eye}}}
}};
}

std::span<const std::string_view> skillTabNames() { return Tabs; }
std::span<const SkillEntry> skillEntries(int tab)
{
    return tab >= 0 && tab < int(Entries.size()) ? std::span<const SkillEntry>(Entries[tab])
                                               : std::span<const SkillEntry>{};
}

} // namespace voxel
