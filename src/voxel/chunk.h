#pragma once

#include "voxel/block.h"

#include <array>
#include <cstddef>

namespace voxel {

class Chunk {
public:
    static constexpr int Size = 16;

    static constexpr bool inBounds(int x, int y, int z) noexcept
    {
        return x >= 0 && x < Size && y >= 0 && y < Size && z >= 0 && z < Size;
    }

    Block get(int x, int y, int z) const noexcept;
    bool set(int x, int y, int z, Block block) noexcept;

private:
    static constexpr std::size_t index(int x, int y, int z) noexcept
    {
        return static_cast<std::size_t>(x + Size * (z + Size * y));
    }

    std::array<Block, Size * Size * Size> blocks_{};
};

} // namespace voxel
