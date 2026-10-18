#pragma once

#include "world.h"

#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace voxel {

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    float material = 0.0f; // Block ID; the shader patterns stay aligned to voxel coordinates.
    // Raised surfaces carry the original block-face normal and signed height.
    // Projecting back along it keeps their paint aligned with the flat block.
    glm::vec4 relief{0.0f};
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Vertex positions and sampleOutside coordinates are in world space. If no
// sampler is supplied, voxels beyond the chunk are treated as air.
// The one-cell neighbor envelope [origin - 1, origin + Chunk::Size] must fit
// in integer coordinates; otherwise throws std::out_of_range before sampling.
MeshData buildMesh(const Chunk& chunk, glm::ivec3 origin = glm::ivec3(0),
                   const std::function<Block(glm::ivec3)>& sampleOutside = {});

// Mesh one world chunk while sampling its neighbors to hide shared faces.
MeshData buildMesh(const World& world, std::size_t chunkIndex);

// Separate a buildMesh result into opaque and water triangles. Both outputs
// retain triangle order and all vertex attributes, with no unused vertices.
MeshData splitWaterMesh(MeshData& opaque);

} // namespace voxel
