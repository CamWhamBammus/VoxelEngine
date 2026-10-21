#include "mesh.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <set>
#include <string>

namespace {

using voxel::Block;
using voxel::Chunk;
using FaceKey = std::array<int, 5>; // Source voxel XYZ, direction, material.
using Coverage = std::map<FaceKey, int>;
using Sampler = std::function<Block(glm::ivec3)>;
int failures = 0;

bool check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
    return condition;
}

FaceKey faceKey(glm::ivec3 position, int axis, int sign, Block block)
{
    return {position.x, position.y, position.z, axis * 2 + (sign > 0), static_cast<int>(block)};
}

Coverage naiveCoverage(const Chunk& chunk, glm::ivec3 origin, const Sampler& outside)
{
    Coverage result;
    for (int y = 0; y < Chunk::Size; ++y) {
        for (int z = 0; z < Chunk::Size; ++z) {
            for (int x = 0; x < Chunk::Size; ++x) {
                const Block block = chunk.get(x, y, z);
                if (!voxel::isRenderable(block)) continue;
                const glm::ivec3 local{x, y, z};
                for (int axis = 0; axis < 3; ++axis) {
                    for (const int sign : {-1, 1}) {
                        auto neighbor = local;
                        neighbor[axis] += sign;
                        const Block adjacent = Chunk::inBounds(neighbor.x, neighbor.y, neighbor.z)
                            ? chunk.get(neighbor.x, neighbor.y, neighbor.z)
                            : (outside ? outside(origin + neighbor) : Block::Air);
                        const bool leafNeighbor = adjacent == Block::Leaves || adjacent == Block::GlowLeaves
                            || adjacent == Block::BlossomLeaves;
                        const bool leafFace = block == Block::Leaves || block == Block::GlowLeaves
                            || block == Block::BlossomLeaves;
                        if (adjacent == Block::Air || (adjacent == Block::Water && block != Block::Water)
                            || (leafNeighbor && !leafFace)) {
                            ++result[faceKey(origin + local, axis, sign, block)];
                        }
                    }
                }
            }
        }
    }
    return result;
}

// Validate the actual triangles before expanding their rectangles into unit
// faces. Multiplicity in Coverage detects overlapping as well as missing faces.
Coverage meshCoverage(const voxel::MeshData& mesh, glm::ivec3 origin, const std::string& label)
{
    Coverage result;
    if (!check(mesh.vertices.size() % 4 == 0 && mesh.indices.size() == mesh.vertices.size() / 4 * 6,
               label + ": output must contain complete indexed quads")) return result;
    for (std::size_t quad = 0; quad < mesh.vertices.size() / 4; ++quad) {
        const auto first = quad * 4;
        const auto& reference = mesh.vertices[first];
        int axis = -1;
        bool valid = true;
        for (int component = 0; component < 3; ++component) {
            if (reference.normal[component] != 0.0f) {
                valid &= axis == -1 && std::abs(reference.normal[component]) == 1.0f;
                axis = component;
            }
        }
        if (!check(valid && axis >= 0, label + ": face normal must be an axis unit vector")) continue;
        const int sign = reference.normal[axis] > 0.0f ? 1 : -1;
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;
        const auto material = static_cast<Block>(static_cast<int>(reference.material));
        valid = voxel::isRenderable(material) && reference.color == voxel::blockColor(material);
        std::array<glm::ivec3, 4> corners{};
        for (std::size_t corner = 0; corner < 4; ++corner) {
            const auto& vertex = mesh.vertices[first + corner];
            valid &= vertex.normal == reference.normal && vertex.color == reference.color
                && vertex.material == reference.material;
            for (int component = 0; component < 3; ++component) {
                const float coordinate = vertex.position[component];
                const bool integral = std::isfinite(coordinate) && std::floor(coordinate) == coordinate
                    && coordinate >= origin[component] && coordinate <= origin[component] + Chunk::Size;
                valid &= integral;
                if (integral) corners[corner][component] = static_cast<int>(coordinate);
            }
        }
        if (!check(valid, label + ": vertices must preserve integer positions, normal, and material")) continue;
        auto low = corners[0];
        auto high = corners[0];
        for (const auto corner : corners) {
            low = glm::min(low, corner);
            high = glm::max(high, corner);
        }
        valid = low[axis] == high[axis] && low[u] < high[u] && low[v] < high[v];
        std::set<std::array<int, 2>> distinct;
        for (const auto corner : corners) {
            valid &= (corner[u] == low[u] || corner[u] == high[u])
                && (corner[v] == low[v] || corner[v] == high[v]);
            distinct.insert({corner[u], corner[v]});
        }
        if (!check(valid && distinct.size() == 4, label + ": quad must have four rectangle corners")) continue;

        std::array<std::set<std::uint32_t>, 2> triangles;
        float doubleArea = 0.0f;
        for (int triangle = 0; triangle < 2; ++triangle) {
            std::array<std::uint32_t, 3> indices{};
            bool inRange = true;
            for (int corner = 0; corner < 3; ++corner) {
                indices[corner] = mesh.indices[quad * 6 + triangle * 3 + corner];
                inRange &= indices[corner] >= first && indices[corner] < first + 4;
                triangles[triangle].insert(indices[corner]);
            }
            valid &= inRange && triangles[triangle].size() == 3;
            if (!inRange) continue;
            const auto a = mesh.vertices[indices[0]].position;
            const auto b = mesh.vertices[indices[1]].position;
            const auto c = mesh.vertices[indices[2]].position;
            const float area = glm::dot(glm::cross(b - a, c - a), reference.normal);
            valid &= area > 0.0f;
            doubleArea += area;
        }
        std::vector<std::uint32_t> shared;
        std::set_intersection(triangles[0].begin(), triangles[0].end(),
                              triangles[1].begin(), triangles[1].end(), std::back_inserter(shared));
        valid &= shared.size() == 2;
        if (valid) {
            const auto diagonal = corners[shared[0] - first] - corners[shared[1] - first];
            valid &= diagonal[u] != 0 && diagonal[v] != 0;
        }
        valid &= doubleArea == 2.0f * (high[u] - low[u]) * (high[v] - low[v]);
        if (!check(valid, label + ": triangles must cover rectangle once with outward CCW winding")) continue;
        auto source = low;
        source[axis] -= sign > 0 ? 1 : 0;
        for (source[u] = low[u]; source[u] < high[u]; ++source[u]) {
            for (source[v] = low[v]; source[v] < high[v]; ++source[v]) {
                ++result[faceKey(source, axis, sign, material)];
            }
        }
    }
    return result;
}

voxel::MeshData verify(const Chunk& chunk, const std::string& label,
                       glm::ivec3 origin = glm::ivec3(0), const Sampler& outside = {})
{
    Sampler checkedOutside;
    if (outside) {
        checkedOutside = [&](glm::ivec3 position) {
            const auto local = position - origin;
            int boundaryAxes = 0;
            bool valid = true;
            for (int axis = 0; axis < 3; ++axis) {
                boundaryAxes += local[axis] == -1 || local[axis] == Chunk::Size;
                valid &= local[axis] >= -1 && local[axis] <= Chunk::Size;
            }
            check(valid && boundaryAxes == 1, label + ": sampler must receive only face-adjacent exterior cells");
            return outside(position);
        };
    }
    auto mesh = voxel::buildMesh(chunk, origin, checkedOutside);
    check(meshCoverage(mesh, origin, label) == naiveCoverage(chunk, origin, outside),
          label + ": geometry must exactly cover naive exposed faces, with no holes, overlaps, or material changes");
    return mesh;
}

void fill(Chunk& chunk, Block block)
{
    for (int y = 0; y < Chunk::Size; ++y)
        for (int z = 0; z < Chunk::Size; ++z)
            for (int x = 0; x < Chunk::Size; ++x) chunk.set(x, y, z, block);
}

} // namespace

int main()
{
    Chunk empty;
    check(verify(empty, "empty").indices.empty(), "empty chunk has no quads");
    Chunk single;
    single.set(3, 4, 5, Block::Stone);
    check(verify(single, "single").indices.size() == 36, "single block has six quads");
    for (int axis = 0; axis < 3; ++axis) {
        auto pair = single;
        glm::ivec3 next(3, 4, 5);
        ++next[axis];
        pair.set(next.x, next.y, next.z, Block::Stone);
        check(verify(pair, "same-material pair").indices.size() == 36,
              "same-material neighbors merge to six quads on every axis");
        pair.set(next.x, next.y, next.z, Block::Dirt);
        check(verify(pair, "mixed-material pair").indices.size() == 60,
              "different materials keep ten quads on every axis");

        Chunk slab;
        for (int y = 0; y < Chunk::Size; ++y)
            for (int z = 0; z < Chunk::Size; ++z)
                for (int x = 0; x < Chunk::Size; ++x)
                    if (glm::ivec3(x, y, z)[axis] == 7) slab.set(x, y, z, Block::Sand);
        check(verify(slab, "slab").indices.size() == 36, "flat slab has six quads on every axis");
    }
    Chunk water;
    fill(water, Block::Water);
    check(verify(water, "water surface").indices.size() == 36, "water merges into six visible outer surfaces");
    check(verify(water, "continuous neighboring water", {}, [](glm::ivec3) { return Block::Water; }).indices.empty(),
          "water across chunk boundaries has no internal faces");
    for (int axis = 0; axis < 3; ++axis) {
        Chunk bank;
        bank.set(5, 5, 5, Block::Stone);
        glm::ivec3 adjacent(5); ++adjacent[axis];
        bank.set(adjacent.x, adjacent.y, adjacent.z, Block::Water);
        check(verify(bank, "solid-water bank").indices.size() == 66,
              "solid banks remain visible next to water without overlapping opposite water faces");
    }
    Chunk canopy;
    fill(canopy, Block::Leaves);
    check(verify(canopy, "dense leaf canopy").indices.size() == 36,
          "cutout foliage remains a cheap outer shell with no interior leaf faces");
    check(verify(canopy, "connected mixed foliage", {}, [](glm::ivec3) { return Block::BlossomLeaves; }).indices.empty(),
          "different foliage across chunk boundaries also hides internal leaf faces");
    Chunk full;
    fill(full, Block::Grass);
    const glm::ivec3 origin(-48, -32, -16);
    check(verify(full, "full negative-origin chunk", origin).indices.size() == 36,
          "full solid chunk collapses to six quads");
    check(verify(full, "enclosed chunk", origin, [](glm::ivec3) { return Block::Stone; }).indices.empty(),
          "solid exterior suppresses all boundary quads");
    for (int axis = 0; axis < 3; ++axis) {
        for (const int sign : {-1, 1}) {
            const auto neighbor = [&](glm::ivec3 position) {
                return position[axis] == origin[axis] + (sign < 0 ? -1 : Chunk::Size)
                    ? Block::Stone : Block::Air;
            };
            check(verify(full, "neighbor culling", origin, neighbor).indices.size() == 30,
                  "each solid neighbor suppresses exactly one merged quad");
        }
    }
    Chunk checkerboard, materialCheckerboard, steps;
    int occupied = 0;
    for (int y = 0; y < Chunk::Size; ++y) {
        for (int z = 0; z < Chunk::Size; ++z) {
            for (int x = 0; x < Chunk::Size; ++x) {
                const bool odd = (x + y + z) % 2 != 0;
                if (odd) {
                    checkerboard.set(x, y, z, Block::Stone);
                    ++occupied;
                }
                materialCheckerboard.set(x, y, z, odd ? Block::Dirt : Block::Grass);
                if (y <= x / 3 + z / 5) steps.set(x, y, z, y % 2 ? Block::Stone : Block::Sand);
            }
        }
    }
    check(verify(checkerboard, "occupancy checkerboard", origin).indices.size()
          == static_cast<std::size_t>(occupied) * 36, "isolated checkerboard blocks cannot merge");
    check(verify(materialCheckerboard, "material checkerboard", origin).indices.size()
          == 6 * Chunk::Size * Chunk::Size * 6, "checkerboard materials keep every boundary unit face");
    verify(steps, "layered steps", origin);

    std::mt19937 random(0x51a7u);
    for (int sample = 0; sample < 12; ++sample) {
        Chunk chunk;
        const unsigned density = std::array<unsigned, 3>{20, 50, 85}[sample % 3];
        for (int y = 0; y < Chunk::Size; ++y)
            for (int z = 0; z < Chunk::Size; ++z)
                for (int x = 0; x < Chunk::Size; ++x)
                    if (random() % 100 < density)
                        // This oracle describes full cube faces. Partial stairs
                        // have independent half-cell coverage tests in mesh_tests.
                        chunk.set(x, y, z, sample % 2 ? Block::Stone
                            : static_cast<Block>(1 + random() % static_cast<unsigned>(Block::GlowMoss)));
        const auto outside = [](glm::ivec3 position) {
            return (position.x + 3 * position.y + 7 * position.z) % 4 == 0 ? Block::Stone : Block::Air;
        };
        verify(chunk, "random chunk " + std::to_string(sample), origin + glm::ivec3(sample * 16), outside);
    }
    return failures == 0 ? 0 : 1;
}
