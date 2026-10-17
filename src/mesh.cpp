#include "mesh.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace voxel {

namespace {

void appendQuad(MeshData& mesh, glm::ivec3 origin, int axis, int sign,
                int layer, int column, int row, int width, int height, Block block)
{
    // Cycling the axes makes cross(u, v) point along the positive face normal.
    const int u = (axis + 1) % 3;
    const int v = (axis + 2) % 3;
    glm::vec3 base(origin);
    base[axis] += static_cast<float>(layer + (sign > 0 ? 1 : 0));
    base[u] += static_cast<float>(column);
    base[v] += static_cast<float>(row);
    glm::vec3 du(0.0f), dv(0.0f), normal(0.0f);
    du[u] = static_cast<float>(width);
    dv[v] = static_cast<float>(height);
    normal[axis] = static_cast<float>(sign);
    std::array<glm::vec3, 4> corners{base, base + du, base + du + dv, base + dv};
    if (sign < 0) {
        std::swap(corners[1], corners[3]);
    }
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const auto corner : corners) {
        mesh.vertices.push_back({corner, normal, blockColor(block), static_cast<float>(block)});
    }
    for (const std::uint32_t index : {0u, 1u, 2u, 0u, 2u, 3u}) {
        mesh.indices.push_back(first + index);
    }
}

} // namespace

MeshData buildMesh(const Chunk& chunk, glm::ivec3 origin,
                   const std::function<Block(glm::ivec3)>& sampleOutside)
{
    for (int axis = 0; axis < 3; ++axis) {
        if (origin[axis] < std::numeric_limits<int>::min() + 1
            || origin[axis] > std::numeric_limits<int>::max() - Chunk::Size) {
            throw std::out_of_range("Mesh neighbors extend beyond integer voxel coordinates");
        }
    }
    MeshData mesh;
    // Sweep slices along X, Y, and Z, once for each face direction. Each
    // slice records exposed materials, then merges matching rectangles.
    for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;
        for (const int sign : {-1, 1}) {
            for (int layer = 0; layer < Chunk::Size; ++layer) {
                std::array<Block, Chunk::Size * Chunk::Size> mask{};
                for (int row = 0; row < Chunk::Size; ++row) {
                    for (int column = 0; column < Chunk::Size; ++column) {
                        glm::ivec3 local(0);
                        local[axis] = layer;
                        local[u] = column;
                        local[v] = row;
                        const Block block = chunk.get(local.x, local.y, local.z);
                        if (!isSolid(block)) continue;
                        glm::ivec3 neighbor = local;
                        neighbor[axis] += sign;
                        const Block adjacent = Chunk::inBounds(neighbor.x, neighbor.y, neighbor.z)
                            ? chunk.get(neighbor.x, neighbor.y, neighbor.z)
                            : (sampleOutside ? sampleOutside(origin + neighbor) : Block::Air);
                        if (!isSolid(adjacent)) {
                            mask[column + row * Chunk::Size] = block;
                        }
                    }
                }

                for (int row = 0; row < Chunk::Size; ++row) {
                    for (int column = 0; column < Chunk::Size;) {
                        const Block block = mask[column + row * Chunk::Size];
                        if (!isSolid(block)) {
                            ++column;
                            continue;
                        }
                        int width = 1;
                        while (column + width < Chunk::Size
                               && mask[column + width + row * Chunk::Size] == block) {
                            ++width;
                        }
                        int height = 1;
                        while (row + height < Chunk::Size) {
                            bool matches = true;
                            for (int offset = 0; offset < width; ++offset) {
                                if (mask[column + offset + (row + height) * Chunk::Size] != block) {
                                    matches = false;
                                    break;
                                }
                            }
                            if (!matches) break;
                            ++height;
                        }
                        appendQuad(mesh, origin, axis, sign, layer, column, row, width, height, block);
                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                mask[column + x + (row + y) * Chunk::Size] = Block::Air;
                            }
                        }
                        column += width;
                    }
                }
            }
        }
    }
    return mesh;
}

MeshData buildMesh(const World& world, std::size_t chunkIndex)
{
    return buildMesh(world.chunk(chunkIndex), world.origin(chunkIndex), [&world](glm::ivec3 position) {
        return world.get(position);
    });
}

} // namespace voxel
