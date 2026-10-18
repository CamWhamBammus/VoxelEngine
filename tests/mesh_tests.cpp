#include "mesh.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

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

    for (int axis = 0; axis < 3; ++axis) {
        for (const int coordinate : {std::numeric_limits<int>::min(),
                                      std::numeric_limits<int>::max() - voxel::Chunk::Size + 1}) {
            glm::ivec3 invalidOrigin(0);
            invalidOrigin[axis] = coordinate;
            int samples = 0;
            bool rejected = false;
            try {
                static_cast<void>(voxel::buildMesh(full, invalidOrigin, [&](glm::ivec3) {
                    ++samples;
                    return voxel::Block::Air;
                }));
            } catch (const std::out_of_range&) {
                rejected = true;
            }
            check(rejected && samples == 0,
                  "unrepresentable mesh neighbors must fail before invoking the sampler");
        }
        for (const int coordinate : {std::numeric_limits<int>::min() + 1,
                                      std::numeric_limits<int>::max() - voxel::Chunk::Size}) {
            glm::ivec3 edgeOrigin(0);
            edgeOrigin[axis] = coordinate;
            int samples = 0;
            int smallest = std::numeric_limits<int>::max();
            int largest = std::numeric_limits<int>::min();
            static_cast<void>(voxel::buildMesh(full, edgeOrigin, [&](glm::ivec3 position) {
                ++samples;
                smallest = std::min(smallest, position[axis]);
                largest = std::max(largest, position[axis]);
                return voxel::Block::Stone;
            }));
            check(samples == 6 * voxel::Chunk::Size * voxel::Chunk::Size
                  && smallest == coordinate - 1 && largest == coordinate + voxel::Chunk::Size,
                  "nearest representable mesh extents must sample exact boundary coordinates");
        }
    }

    const auto sameVertex = [](const voxel::MeshVertex& a, const voxel::MeshVertex& b) {
        return a.position == b.position && a.normal == b.normal && a.color == b.color
            && a.material == b.material && a.relief == b.relief;
    };
    const auto verifySplit = [&](const voxel::MeshData& original,
                                 std::size_t opaqueIndices, std::size_t waterIndices) {
        auto opaque = original;
        const auto water = voxel::splitWaterMesh(opaque);
        check(opaque.indices.size() == opaqueIndices && water.indices.size() == waterIndices,
              "water split must keep the expected triangles in each output");
        check(opaque.indices.size() + water.indices.size() == original.indices.size(),
              "water split must neither lose nor duplicate triangles");
        for (const bool isWater : {false, true}) {
            const auto& result = isWater ? water : opaque;
            std::vector<bool> referenced(result.vertices.size(), false);
            for (const auto index : result.indices) {
                check(index < result.vertices.size(), "split mesh indices must stay in bounds");
                if (index < referenced.size()) referenced[index] = true;
            }
            check(std::all_of(referenced.begin(), referenced.end(), [](bool used) { return used; }),
                  "split meshes must contain no unreferenced vertices");
            std::vector<bool> originalUsed(original.vertices.size(), false);
            std::size_t outputIndex = 0;
            for (std::size_t triangle = 0; triangle < original.indices.size(); triangle += 3) {
                const bool wasWater = original.vertices[original.indices[triangle]].material
                    == static_cast<float>(voxel::Block::Water);
                if (wasWater != isWater) continue;
                for (std::size_t corner = 0; corner < 3; ++corner) {
                    const auto index = original.indices[triangle + corner];
                    originalUsed[index] = true;
                    const bool valid = outputIndex < result.indices.size()
                        && result.indices[outputIndex] < result.vertices.size();
                    check(valid && sameVertex(original.vertices[index],
                          result.vertices[result.indices[outputIndex]]),
                          "split must preserve triangle order, winding and every vertex attribute");
                    ++outputIndex;
                }
            }
            check(result.vertices.size() == static_cast<std::size_t>(
                      std::count(originalUsed.begin(), originalUsed.end(), true)),
                  "split meshes must reuse shared vertices without retaining unused vertices");
        }
    };
    verifySplit(empty, 0, 0);
    verifySplit(single, 36, 0);

    voxel::Chunk waterChunk;
    waterChunk.set(3, 4, 5, voxel::Block::Water);
    verifySplit(voxel::buildMesh(waterChunk), 0, 36);
    waterChunk.set(4, 4, 5, voxel::Block::Stone);
    auto mixed = voxel::buildMesh(waterChunk, origin);
    // Non-default attributes catch accidental vertex reconstruction during the split.
    for (std::size_t i = 0; i < mixed.vertices.size(); ++i) {
        mixed.vertices[i].color = glm::vec3(float(i) / 100.0f, .23f, .61f);
        mixed.vertices[i].relief = glm::vec4(mixed.vertices[i].normal, float(i) / 1000.0f);
    }
    mixed.vertices.push_back({glm::vec3(100), glm::vec3(0, 1, 0), glm::vec3(1)});
    verifySplit(mixed, 36, 30);
    auto splitAgain = mixed;
    auto firstWater = voxel::splitWaterMesh(splitAgain);
    verifySplit(splitAgain, 36, 0);
    verifySplit(firstWater, 0, 30);

    voxel::Chunk waterBoundary;
    waterBoundary.set(15, 3, 4, voxel::Block::Water);
    for (const auto neighbor : {voxel::Block::Water, voxel::Block::Stone}) {
        const auto mesh = voxel::buildMesh(waterBoundary, origin,
            [neighbor](glm::ivec3) { return neighbor; });
        verifySplit(mesh, 0, 30);
        check(std::none_of(mesh.vertices.begin(), mesh.vertices.end(), [](const auto& vertex) {
            return vertex.normal == glm::vec3(1, 0, 0);
        }), "water split input must omit hidden faces across the chunk boundary");
    }
    const auto wetBank = voxel::buildMesh(boundary, origin,
        [](glm::ivec3) { return voxel::Block::Water; });
    verifySplit(wetBank, 36, 0);
    return failures == 0 ? 0 : 1;
}
