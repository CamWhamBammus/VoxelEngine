#pragma once

#include <cstdint>
#include <array>
#include <string_view>
#include <glm/vec3.hpp>

namespace voxel {

enum class Block : std::uint8_t { Air, Grass, Dirt, Stone, Sand };

inline constexpr std::array PlaceableBlocks{Block::Grass, Block::Dirt, Block::Stone, Block::Sand};

constexpr bool isValidBlock(Block block) noexcept
{
    return static_cast<std::uint8_t>(block) <= static_cast<std::uint8_t>(Block::Sand);
}

constexpr std::string_view blockName(Block block) noexcept
{
    switch (block) {
    case Block::Air: return "Air";
    case Block::Grass: return "Grass";
    case Block::Dirt: return "Dirt";
    case Block::Stone: return "Stone";
    case Block::Sand: return "Sand";
    }
    return "Unknown";
}

constexpr bool isSolid(Block block) noexcept
{
    return isValidBlock(block) && block != Block::Air;
}

inline glm::vec3 blockColor(Block block) noexcept
{
    switch (block) {
    case Block::Grass: return {0.32f, 0.66f, 0.22f};
    case Block::Dirt: return {0.46f, 0.29f, 0.16f};
    case Block::Stone: return {0.51f, 0.53f, 0.57f};
    case Block::Sand: return {0.85f, 0.77f, 0.50f};
    case Block::Air: return {0.0f, 0.0f, 0.0f};
    }
    return {1.0f, 0.0f, 1.0f};
}

} // namespace voxel
