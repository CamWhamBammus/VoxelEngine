#include "world.h"

#include <iostream>
#include <limits>

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            ++failures;
        }
    };

    voxel::Chunk chunk;
    for (int y = 0; y < voxel::Chunk::Size; ++y) {
        for (int z = 0; z < voxel::Chunk::Size; ++z) {
            for (int x = 0; x < voxel::Chunk::Size; ++x) {
                check(chunk.get(x, y, z) == voxel::Block::Air, "new chunk must be empty");
                const auto block = static_cast<voxel::Block>(
                    1 + (x + 2 * y + 3 * z) % (voxel::BlockCount - 1));
                check(chunk.set(x, y, z, block), "valid write must succeed");
            }
        }
    }
    for (int y = 0; y < voxel::Chunk::Size; ++y) {
        for (int z = 0; z < voxel::Chunk::Size; ++z) {
            for (int x = 0; x < voxel::Chunk::Size; ++x) {
                const auto block = static_cast<voxel::Block>(
                    1 + (x + 2 * y + 3 * z) % (voxel::BlockCount - 1));
                check(chunk.get(x, y, z) == block, "voxel coordinates must not alias");
            }
        }
    }
    for (const int coordinate : {-1, voxel::Chunk::Size, std::numeric_limits<int>::min(),
                                  std::numeric_limits<int>::max()}) {
        check(chunk.get(coordinate, 0, 0) == voxel::Block::Air, "out-of-range x read");
        check(chunk.get(0, coordinate, 0) == voxel::Block::Air, "out-of-range y read");
        check(chunk.get(0, 0, coordinate) == voxel::Block::Air, "out-of-range z read");
        check(!chunk.set(coordinate, 0, 0, voxel::Block::Air), "out-of-range x write");
        check(!chunk.set(0, coordinate, 0, voxel::Block::Air), "out-of-range y write");
        check(!chunk.set(0, 0, coordinate, voxel::Block::Air), "out-of-range z write");
    }
    check(chunk.get(0, 0, 0) == voxel::Block::Grass, "invalid writes must preserve storage");
    check(chunk.set(15, 15, 15, voxel::Block::Air), "last voxel must be writable");
    check(chunk.get(15, 15, 15) == voxel::Block::Air, "voxel removal must work");
    check(!voxel::isSolid(voxel::Block::Air), "air must not be solid");
    const std::array expectedMaterials{
        voxel::Block::Air, voxel::Block::Grass, voxel::Block::Dirt, voxel::Block::Stone,
        voxel::Block::Sand, voxel::Block::Limestone, voxel::Block::Slate,
        voxel::Block::CoalOre, voxel::Block::CopperOre, voxel::Block::IronOre,
        voxel::Block::Crystal, voxel::Block::Timber, voxel::Block::Planks,
        voxel::Block::Brick, voxel::Block::Lantern, voxel::Block::Bedrock,
        voxel::Block::Water, voxel::Block::Leaves, voxel::Block::GlowLeaves, voxel::Block::RoofTiles
    };
    check(voxel::BlockCount == expectedMaterials.size(), "block count covers every stored material");
    for (std::size_t value = 0; value < expectedMaterials.size(); ++value) {
        const auto block = expectedMaterials[value];
        check(static_cast<std::size_t>(block) == value, "material IDs must remain save-compatible");
        check(voxel::isValidBlock(block), "every declared material is valid");
        check(voxel::isSolid(block) == (block != voxel::Block::Air && block != voxel::Block::Water),
              "air and water have no collision");
        check(voxel::isRenderable(block) == (block != voxel::Block::Air), "every material except air renders");
        check(!voxel::blockName(block).empty() && voxel::blockName(block) != "Unknown",
              "every declared material has a display name");
        const auto color = voxel::blockColor(block);
        check(color.x >= 0 && color.x <= 1 && color.y >= 0 && color.y <= 1
              && color.z >= 0 && color.z <= 1, "material color must be normalized");
        check(chunk.set(15, 15, 15, block) && chunk.get(15, 15, 15) == block,
              "every declared material including bedrock survives storage");
    }
    for (unsigned int value = voxel::BlockCount; value < 256; ++value) {
        const auto invalid = static_cast<voxel::Block>(value);
        check(!voxel::isValidBlock(invalid) && !voxel::isSolid(invalid) && !voxel::isRenderable(invalid), "unknown block IDs must not be solid");
        check(!chunk.set(0, 0, 0, invalid), "unknown block IDs must not enter storage");
        check(voxel::blockName(invalid) == "Unknown", "unknown block names must be safe");
    }
    check(chunk.get(0, 0, 0) == voxel::Block::Grass, "invalid material writes preserve contents");
    std::array<bool, voxel::BlockCount> inPalette{};
    for (const auto block : voxel::PlaceableBlocks) {
        check(voxel::isValidBlock(block) && voxel::isRenderable(block), "palette contains only valid visible blocks");
        check(voxel::blockName(block) != "Unknown", "palette has display names");
        const auto value = static_cast<std::size_t>(block);
        check(!inPalette[value], "palette contains no duplicate materials");
        inPalette[value] = true;
    }
    for (const auto block : expectedMaterials) {
        check(inPalette[static_cast<std::size_t>(block)]
                  == (block != voxel::Block::Air && block != voxel::Block::Bedrock),
              "all materials except air and boundary bedrock can be placed");
    }
    return failures == 0 ? 0 : 1;
}
