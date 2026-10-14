#pragma once

#include "voxel/chunk.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace voxel {

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Vertex positions and sampleOutside coordinates are in world space. If no
// sampler is supplied, voxels beyond the chunk are treated as air.
MeshData buildMesh(const Chunk& chunk, glm::ivec3 origin = glm::ivec3(0),
                   const std::function<Block(glm::ivec3)>& sampleOutside = {});

} // namespace voxel
