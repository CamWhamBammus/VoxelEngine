#include "mesh.h"

#include <glm/geometric.hpp>
#include <iostream>

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
    const auto empty = voxel::buildMesh(chunk);
    check(empty.vertices.empty() && empty.indices.empty(), "empty chunk must produce no geometry");

    chunk.set(3, 4, 5, voxel::Block::Stone);
    int interiorSamples = 0;
    const glm::ivec3 origin(-32, 16, 48);
    const auto single = voxel::buildMesh(chunk, origin, [&interiorSamples](glm::ivec3) {
        ++interiorSamples;
        return voxel::Block::Stone;
    });
    check(interiorSamples == 0, "outside sampler must not replace local chunk reads");
    check(single.vertices.size() == 24 && single.indices.size() == 36,
          "one voxel must have six indexed faces");
    for (const auto& vertex : single.vertices) {
        check(vertex.position.x >= -29 && vertex.position.x <= -28
              && vertex.position.y >= 20 && vertex.position.y <= 21
              && vertex.position.z >= 53 && vertex.position.z <= 54,
              "mesh positions must include chunk origin");
        check(vertex.color == voxel::blockColor(voxel::Block::Stone),
              "mesh must preserve material colors");
    }
    for (std::size_t i = 0; i + 2 < single.indices.size(); i += 3) {
        const auto a = single.indices[i];
        const auto b = single.indices[i + 1];
        const auto c = single.indices[i + 2];
        const bool valid = a < single.vertices.size() && b < single.vertices.size()
                           && c < single.vertices.size();
        check(valid, "triangle indices must reference vertices");
        if (valid) {
            const auto& first = single.vertices[a];
            const auto cross = glm::cross(single.vertices[b].position - first.position,
                                          single.vertices[c].position - first.position);
            check(glm::dot(cross, first.normal) > 0.99f,
                  "triangles must wind counterclockwise when viewed from outside");
            check(first.normal == single.vertices[b].normal
                  && first.normal == single.vertices[c].normal,
                  "face vertices must share the outward normal");
        }
    }

    for (const auto offset : {glm::ivec3(1, 0, 0), glm::ivec3(0, 1, 0), glm::ivec3(0, 0, 1)}) {
        const auto adjacent = glm::ivec3(3, 4, 5) + offset;
        chunk.set(adjacent.x, adjacent.y, adjacent.z, voxel::Block::Dirt);
        const auto pair = voxel::buildMesh(chunk);
        check(pair.vertices.size() == 40 && pair.indices.size() == 60,
              "adjacent solids must hide both shared faces on every axis");
        chunk.set(adjacent.x, adjacent.y, adjacent.z, voxel::Block::Air);
    }

    voxel::Chunk boundary;
    boundary.set(15, 3, 4, voxel::Block::Grass);
    int outsideSamples = 0;
    const auto connected = voxel::buildMesh(boundary, origin, [&](glm::ivec3 world) {
        ++outsideSamples;
        check(world == origin + glm::ivec3(16, 3, 4),
              "outside sampler must receive absolute world voxel coordinates");
        return voxel::Block::Sand;
    });
    check(outsideSamples == 1, "boundary voxel must sample only exterior neighbors");
    check(connected.vertices.size() == 20 && connected.indices.size() == 30,
          "neighbor chunk must suppress the shared boundary face");

    voxel::Chunk full;
    for (int y = 0; y < voxel::Chunk::Size; ++y) {
        for (int z = 0; z < voxel::Chunk::Size; ++z) {
            for (int x = 0; x < voxel::Chunk::Size; ++x) {
                full.set(x, y, z, voxel::Block::Stone);
            }
        }
    }
    const auto shell = voxel::buildMesh(full);
    check(shell.vertices.size() == 6 * 4 && shell.indices.size() == 6 * 6,
          "solid chunk must merge its outer shell into six quads");
    const auto enclosed = voxel::buildMesh(full, origin, [](glm::ivec3) { return voxel::Block::Stone; });
    check(enclosed.vertices.empty() && enclosed.indices.empty(),
          "fully enclosed chunk must emit no geometry");
    return failures == 0 ? 0 : 1;
}
