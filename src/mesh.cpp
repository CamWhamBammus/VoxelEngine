#include "mesh.h"
#include "block_shapes.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace voxel {

namespace {

void appendFace(MeshData& mesh, glm::vec3 base, int axis, int sign,
                float width, float height, Block block)
{
    // Cycling the axes makes cross(u, v) point along the positive face normal.
    const int u = (axis + 1) % 3;
    const int v = (axis + 2) % 3;
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

void appendQuad(MeshData& mesh, glm::ivec3 origin, int axis, int sign,
                int layer, int column, int row, int width, int height, Block block)
{
    glm::vec3 base(origin);
    base[axis] += static_cast<float>(layer + (sign > 0 ? 1 : 0));
    base[(axis + 1) % 3] += static_cast<float>(column);
    base[(axis + 2) % 3] += static_cast<float>(row);
    appendFace(mesh, base, axis, sign, static_cast<float>(width), static_cast<float>(height), block);
}

bool fillsHalfCell(Block block, glm::ivec3 cell)
{
    const auto shape = blockShape(block);
    const auto middle = (glm::vec3(cell) + .5f) * .5f;
    for (int i = 0; i < shape.count; ++i) {
        const auto& box = shape.boxes[i];
        if (middle.x > box.minimum.x && middle.x < box.maximum.x
            && middle.y > box.minimum.y && middle.y < box.maximum.y
            && middle.z > box.minimum.z && middle.z < box.maximum.z) return true;
    }
    return false;
}

// Merge a two-by-two mask into half-block rectangles. Only stairs and faces
// touching stairs use this finer grid; ordinary terrain keeps greedy meshing.
void appendHalfFaces(MeshData& mesh, glm::ivec3 origin, int axis, int sign,
                     int layer, std::array<bool, 4> visible, Block material)
{
    const int u = (axis + 1) % 3;
    const int v = (axis + 2) % 3;
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            if (!visible[column + row * 2]) continue;
            const int width = column == 0 && visible[1 + row * 2] ? 2 : 1;
            int height = 1;
            if (row == 0 && visible[column + 2] && (width == 1 || visible[column + 3])) height = 2;
            glm::vec3 base(origin);
            base[axis] += .5f * static_cast<float>(layer + (sign > 0 ? 1 : 0));
            base[u] += .5f * static_cast<float>(column);
            base[v] += .5f * static_cast<float>(row);
            appendFace(mesh, base, axis, sign, .5f * width, .5f * height, material);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) visible[column + x + (row + y) * 2] = false;
            }
        }
    }
}

void appendFaceBesideStair(MeshData& mesh, glm::ivec3 origin, int axis, int sign,
                          Block block, Block adjacent)
{
    std::array<bool, 4> visible{};
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            glm::ivec3 neighbor(0);
            neighbor[axis] = sign > 0 ? 0 : 1;
            neighbor[(axis + 1) % 3] = column;
            neighbor[(axis + 2) % 3] = row;
            visible[column + row * 2] = !fillsHalfCell(adjacent, neighbor);
        }
    }
    appendHalfFaces(mesh, origin, axis, sign, sign > 0 ? 1 : 0, visible, block);
}

void appendStair(MeshData& mesh, glm::ivec3 origin, Block block,
                 const std::function<Block(glm::ivec3)>& sample)
{
    for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;
        for (const int sign : {-1, 1}) {
            auto neighborPosition = origin;
            neighborPosition[axis] += sign;
            const auto adjacent = sample(neighborPosition);
            for (int layer = 0; layer < 2; ++layer) {
                std::array<bool, 4> visible{};
                for (int row = 0; row < 2; ++row) {
                    for (int column = 0; column < 2; ++column) {
                        glm::ivec3 cell(0);
                        cell[axis] = layer;
                        cell[u] = column;
                        cell[v] = row;
                        if (!fillsHalfCell(block, cell)) continue;
                        auto neighbor = cell;
                        neighbor[axis] += sign;
                        bool hidden = false;
                        if (neighbor[axis] >= 0 && neighbor[axis] < 2) {
                            hidden = fillsHalfCell(block, neighbor);
                        } else if (isStair(adjacent)) {
                            neighbor[axis] = sign > 0 ? 0 : 1;
                            hidden = fillsHalfCell(adjacent, neighbor);
                        } else {
                            hidden = occludes(adjacent, block);
                        }
                        visible[column + row * 2] = !hidden;
                    }
                }
                appendHalfFaces(mesh, origin, axis, sign, layer, visible, stairMaterial(block));
            }
        }
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
                        if (!isRenderable(block) || isStair(block)) continue;
                        glm::ivec3 neighbor = local;
                        neighbor[axis] += sign;
                        const Block adjacent = Chunk::inBounds(neighbor.x, neighbor.y, neighbor.z)
                            ? chunk.get(neighbor.x, neighbor.y, neighbor.z)
                            : (sampleOutside ? sampleOutside(origin + neighbor) : Block::Air);
                        if (isStair(adjacent)) {
                            appendFaceBesideStair(mesh, origin + local, axis, sign, block, adjacent);
                        } else if (!occludes(adjacent, block)) {
                            mask[column + row * Chunk::Size] = block;
                        }
                    }
                }

                for (int row = 0; row < Chunk::Size; ++row) {
                    for (int column = 0; column < Chunk::Size;) {
                        const Block block = mask[column + row * Chunk::Size];
                        if (!isRenderable(block)) {
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
    const auto sample = [&](glm::ivec3 position) {
        const auto local = position - origin;
        return Chunk::inBounds(local.x, local.y, local.z) ? chunk.get(local.x, local.y, local.z)
            : (sampleOutside ? sampleOutside(position) : Block::Air);
    };
    for (int y = 0; y < Chunk::Size; ++y) {
        for (int z = 0; z < Chunk::Size; ++z) {
            for (int x = 0; x < Chunk::Size; ++x) {
                const auto block = chunk.get(x, y, z);
                if (isStair(block)) appendStair(mesh, origin + glm::ivec3(x, y, z), block, sample);
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

MeshData splitWaterMesh(MeshData& opaque)
{
    std::array<MeshData, 2> parts;
    constexpr auto unused = std::numeric_limits<std::uint32_t>::max();
    std::array<std::vector<std::uint32_t>, 2> remapped;
    for (auto& indices : remapped) indices.assign(opaque.vertices.size(), unused);

    for (std::size_t triangle = 0; triangle < opaque.indices.size(); triangle += 3) {
        // buildMesh gives every vertex of a triangle the same material.
        const bool water = opaque.vertices[opaque.indices[triangle]].material
            == static_cast<float>(Block::Water);
        auto& part = parts[water];
        auto& indices = remapped[water];
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const auto original = opaque.indices[triangle + corner];
            if (indices[original] == unused) {
                indices[original] = static_cast<std::uint32_t>(part.vertices.size());
                part.vertices.push_back(opaque.vertices[original]);
            }
            part.indices.push_back(indices[original]);
        }
    }
    opaque = std::move(parts[0]);
    return std::move(parts[1]);
}

} // namespace voxel
