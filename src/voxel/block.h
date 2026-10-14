#pragma once

#include <cstdint>
#include <glm/vec3.hpp>

namespace voxel {

enum class Block : std::uint8_t { Air, Grass, Dirt, Stone, Sand };

constexpr bool isSolid(Block block) noexcept
{
    return block != Block::Air;
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
