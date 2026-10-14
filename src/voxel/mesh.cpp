#include "voxel/mesh.h"

#include <array>

namespace voxel {
namespace {

struct Face {
    glm::ivec3 normal;
    std::array<glm::vec3, 4> corners;
};

const std::array<Face, 6> faces{{
    {{ 1, 0, 0}, {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}}},
    {{-1, 0, 0}, {{{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}}}},
    {{ 0, 1, 0}, {{{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}}},
    {{ 0,-1, 0}, {{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}}},
    {{ 0, 0, 1}, {{{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}}}},
    {{ 0, 0,-1}, {{{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}}},
}};

} // namespace

MeshData buildMesh(const Chunk& chunk, glm::ivec3 origin,
                   const std::function<Block(glm::ivec3)>& sampleOutside)
{
    MeshData mesh;
    for (int y = 0; y < Chunk::Size; ++y) {
        for (int z = 0; z < Chunk::Size; ++z) {
            for (int x = 0; x < Chunk::Size; ++x) {
                const Block block = chunk.get(x, y, z);
                if (!isSolid(block)) {
                    continue;
                }
                const glm::ivec3 local{x, y, z};
                const glm::vec3 worldPosition(origin + local);
                for (const Face& face : faces) {
                    const glm::ivec3 neighbor = local + face.normal;
                    const Block adjacent = Chunk::inBounds(neighbor.x, neighbor.y, neighbor.z)
                        ? chunk.get(neighbor.x, neighbor.y, neighbor.z)
                        : (sampleOutside ? sampleOutside(origin + neighbor) : Block::Air);
                    if (isSolid(adjacent)) {
                        continue;
                    }
                    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
                    for (const glm::vec3 corner : face.corners) {
                        mesh.vertices.push_back({worldPosition + corner,
                                                 glm::vec3(face.normal), blockColor(block)});
                    }
                    for (const std::uint32_t index : {0u, 1u, 2u, 0u, 2u, 3u}) {
                        mesh.indices.push_back(first + index);
                    }
                }
            }
        }
    }
    return mesh;
}

} // namespace voxel
