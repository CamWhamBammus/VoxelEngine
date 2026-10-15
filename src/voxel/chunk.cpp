#include "voxel/chunk.h"

namespace voxel {

Block Chunk::get(int x, int y, int z) const noexcept
{
    return inBounds(x, y, z) ? blocks_[index(x, y, z)] : Block::Air;
}

bool Chunk::set(int x, int y, int z, Block block) noexcept
{
    if (!inBounds(x, y, z) || !isValidBlock(block)) {
        return false;
    }
    blocks_[index(x, y, z)] = block;
    return true;
}

} // namespace voxel
