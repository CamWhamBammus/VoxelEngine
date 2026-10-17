#include "graphics/cave_details.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <glm/geometric.hpp>

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void clear(voxel::World& world)
{
    for (int z = voxel::World::Min.z; z < voxel::World::MaxExclusive.z; ++z) {
        for (int x = voxel::World::Min.x; x < voxel::World::MaxExclusive.x; ++x) {
            for (int y = 0; y < voxel::Chunk::Size; ++y) world.set({x, y, z}, voxel::Block::Air);
        }
    }
}

bool same(const voxel::MeshData& first, const voxel::MeshData& second)
{
    if (first.indices != second.indices || first.vertices.size() != second.vertices.size()) return false;
    for (std::size_t i = 0; i < first.vertices.size(); ++i) {
        const auto& a = first.vertices[i];
        const auto& b = second.vertices[i];
        if (a.position != b.position || a.normal != b.normal || a.color != b.color || a.material != b.material) return false;
    }
    return true;
}

void validate(const voxel::MeshData& mesh, glm::ivec3 origin)
{
    check(mesh.indices.size() % 3 == 0 && mesh.indices.size() / 3 <= 668,
          "detail triangle count is valid and bounded per chunk");
    for (const auto& vertex : mesh.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            check(std::isfinite(vertex.position[axis]) && std::isfinite(vertex.normal[axis])
                  && std::isfinite(vertex.color[axis]), "detail attributes are finite");
            check(vertex.position[axis] > origin[axis] - 1.0f
                  && vertex.position[axis] < origin[axis] + voxel::Chunk::Size + 1.0f,
                  "detail geometry fits its chunk plus one-cell margin");
            check(vertex.color[axis] >= 0 && vertex.color[axis] <= 1, "facet colors stay in the displayable range");
        }
        check(std::abs(glm::length(vertex.normal) - 1.0f) < 0.0001f, "facet normals are unit length");
        check(vertex.material >= 0 && vertex.material < voxel::BlockCount
              && std::floor(vertex.material) == vertex.material, "detail materials use existing block IDs");
    }
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size()) {
            check(false, "triangle indices address valid vertices");
            continue;
        }
        const auto cross = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                     mesh.vertices[c].position - mesh.vertices[a].position);
        check(glm::length(cross) > 0.00001f, "detail triangles have nonzero area");
        check(glm::dot(glm::normalize(cross), mesh.vertices[a].normal) > 0.9999f,
              "triangle winding agrees with its outward flat normal");
    }
}

} // namespace

int main()
{
    using voxel::Block;
    using voxel::World;
    constexpr std::size_t centerChunk = 12; // Origin (0, 0, 0) in the 5x5 chunk grid.
    constexpr std::array<glm::ivec3, 6> directions{{
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
    }};
    World world(42);
    clear(world);
    check(voxel::buildCaveDetails(world, centerChunk).indices.empty(), "empty chunks have no decorations");
    const glm::ivec3 crystal{15, 6, 5};
    world.set(crystal, Block::Crystal);
    const auto before = world;
    const auto mesh = voxel::buildCaveDetails(world, centerChunk);
    check(!mesh.indices.empty() && world.sameContents(before), "crystal decoration is cosmetic and leaves voxels intact");
    check(same(mesh, voxel::buildCaveDetails(world, centerChunk)), "identical world and seed produce identical details");
    validate(mesh, world.origin(centerChunk));
    for (const auto& vertex : mesh.vertices) {
        const glm::ivec3 cell{static_cast<int>(std::floor(vertex.position.x)),
                              static_cast<int>(std::floor(vertex.position.y)),
                              static_cast<int>(std::floor(vertex.position.z))};
        const auto offset = glm::abs(cell - crystal);
        check(offset.x + offset.y + offset.z <= 1, "shards remain within their anchor and one adjacent cell");
        check(vertex.material == static_cast<float>(Block::Crystal), "crystal shards keep the crystal material");
        check(vertex.color.b > 0.8f && vertex.color.g > 0.6f, "crystals use saturated cyan and blue facets");
        check(glm::dot(vertex.normal, vertex.position - (glm::vec3(crystal) + glm::vec3(0.5f))) > 0,
              "crystal triangles face outward from the solid anchor");
    }

    // Neighbors across chunk boundaries must suppress decoration just like local neighbors.
    for (const auto direction : directions) world.set(crystal + direction, Block::Stone);
    check(voxel::buildCaveDetails(world, centerChunk).indices.empty(), "covered crystal faces generate no hidden shards");
    for (const auto direction : directions) {
        world.set(crystal + direction, Block::Air);
        const auto exposed = voxel::buildCaveDetails(world, centerChunk);
        for (const auto& vertex : exposed.vertices) {
            const glm::ivec3 cell{static_cast<int>(std::floor(vertex.position.x)),
                                  static_cast<int>(std::floor(vertex.position.y)),
                                  static_cast<int>(std::floor(vertex.position.z))};
            check(cell == crystal || cell == crystal + direction, "only the newly exposed face can grow geometry");
        }
        world.set(crystal + direction, Block::Stone);
    }
    for (const auto direction : directions) world.set(crystal + direction, Block::Air);
    world.set(crystal, Block::Air);
    check(voxel::buildCaveDetails(world, centerChunk).indices.empty(), "mining the anchor removes its decorations");

    world.set({5, 3, 5}, Block::Grass);
    const auto grass = voxel::buildCaveDetails(world, centerChunk);
    check(grass.indices.size() == 24, "one exposed grass block grows a small two-sided tuft");
    validate(grass, world.origin(centerChunk));
    for (const auto& vertex : grass.vertices) {
        check(vertex.position.y >= 3.99f && vertex.position.y < 4.6f
              && vertex.color.g > vertex.color.r && vertex.color.g > vertex.color.b,
              "grass stays short with healthy green blades");
    }
    world.set({5, 4, 5}, Block::Stone);
    check(voxel::buildCaveDetails(world, centerChunk).indices.empty(), "covering grass removes tufts");
    clear(world);

    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) world.set({x, 12, z}, Block::Limestone);
    }
    const auto ceiling = voxel::buildCaveDetails(world, centerChunk);
    check(!ceiling.indices.empty() && ceiling.indices.size() / 3 <= 60, "natural ceiling gets sparse capped stalactites");
    validate(ceiling, world.origin(centerChunk));
    for (const auto& vertex : ceiling.vertices) {
        check(vertex.position.y > 11.2f && vertex.position.y < 12.02f,
              "stalactites stay attached and less than one block below the ceiling");
    }
    clear(world);
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) world.set({x, 8, z}, Block::Limestone);
    }
    check(voxel::buildCaveDetails(world, centerChunk).indices.empty(), "low ceilings never receive stalactites");
    clear(world);

    // Stress the caps with hundreds of eligible anchors in one chunk.
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            world.set({x, 3, z}, Block::Grass);
            world.set({x, 7, z}, Block::Crystal);
            world.set({x, 12, z}, Block::Slate);
        }
    }
    validate(voxel::buildCaveDetails(world, centerChunk), world.origin(centerChunk));
    bool threw = false;
    try { voxel::buildCaveDetails(world, World::ChunkCount); }
    catch (const std::out_of_range&) { threw = true; }
    check(threw, "invalid chunk indices follow World bounds checking");
    return failures ? 1 : 0;
}
