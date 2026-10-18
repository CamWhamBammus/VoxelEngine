#include "graphics/block_relief.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace {
int failures = 0;
constexpr std::size_t centerChunk = 12;
constexpr std::array<glm::ivec3, 6> faces{{
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
}};

void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void clear(voxel::World& world)
{
    for (int z = voxel::World::Min.z; z < voxel::World::MaxExclusive.z; ++z) {
        for (int y = 0; y < voxel::Chunk::Size; ++y) {
            for (int x = voxel::World::Min.x; x < voxel::World::MaxExclusive.x; ++x) {
                world.set({x, y, z}, voxel::Block::Air);
            }
        }
    }
}

bool sameVertex(const voxel::MeshVertex& a, const voxel::MeshVertex& b)
{
    return a.position == b.position && a.normal == b.normal && a.color == b.color
        && a.material == b.material && a.relief == b.relief;
}

bool same(const voxel::MeshData& a, const voxel::MeshData& b)
{
    return a.indices == b.indices && a.vertices.size() == b.vertices.size()
        && std::equal(a.vertices.begin(), a.vertices.end(), b.vertices.begin(), sameVertex);
}

void cover(voxel::World& world, glm::ivec3 anchor)
{
    for (const auto face : faces) world.set(anchor + face, voxel::Block::Dirt);
}

// Reference the shader's shape field independently of the polygon clipping.
float paintHash(glm::vec2 p)
{
    const glm::ivec2 cell(p);
    std::uint32_t value = static_cast<std::uint32_t>(cell.x) * 0x9e3779b9u
                        ^ static_cast<std::uint32_t>(cell.y) * 0x85ebca6bu;
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / 16777216.0f;
}

bool paintedOre(glm::vec2 uv, float seed)
{
    const auto tile = glm::floor(uv * 3.0f);
    const float h = paintHash(tile + seed), h2 = paintHash(tile + seed + 11.0f);
    const auto local = uv * 3.0f - tile - (0.35f + 0.30f * glm::vec2(h, h2));
    const auto q = glm::abs(glm::vec2(local.x * 0.8f + local.y * 0.6f,
                                    -local.x * 0.6f + local.y * 0.8f));
    const float shape = std::max(q.x * 0.83f + q.y * 0.66f, std::max(q.x, q.y) * 0.94f);
    return h > 0.48f && shape <= 0.21f + 0.16f * h2 + 0.0001f;
}

glm::ivec2 paintedStone(glm::vec2 uv, float seed, bool slate)
{
    const auto p = uv * glm::vec2(0.95f, slate ? 2.4f : 1.15f);
    const glm::ivec2 tile(glm::floor(p));
    float nearest = 100;
    glm::ivec2 result{};
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            const auto cell = tile + glm::ivec2(x, y);
            const float h = paintHash(glm::vec2(cell) + seed);
            const auto delta = p - (glm::vec2(cell) + 0.18f + 0.64f * glm::vec2(h, glm::fract(h * 17.73f)));
            const float distance = glm::dot(delta, delta);
            if (distance < nearest) { nearest = distance; result = cell; }
        }
    }
    return result;
}

void validate(const voxel::MeshData& mesh, glm::ivec3 anchor, glm::ivec3 face,
              voxel::Block material, float minimumPeak, float maximumPeak)
{
    const bool mineral = material == voxel::Block::CoalOre || material == voxel::Block::CopperOre || material == voxel::Block::IronOre;
    check(!mesh.indices.empty() && mesh.indices.size() % 3 == 0 && mesh.indices.size() / 3 <= (mineral ? 102 : 68),
          "one decorated face has a bounded, nonempty triangle mesh");
    const glm::vec3 normal(face);
    const auto faceCenter = glm::vec3(anchor) + glm::vec3(0.5f) + normal * 0.5f;
    const glm::vec3 u = face.x != 0 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
    const glm::vec3 v = face.y != 0 ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const auto uv = [&](glm::vec3 p) { return glm::vec2(glm::dot(p, u), glm::dot(p, v)); };
    const float paintSeed = glm::dot(glm::vec3(anchor) * glm::abs(normal), glm::vec3(17, 37, 57))
        + static_cast<float>(material) * 19.0f;
    bool embedded = false;
    float peak = 0;
    for (const auto& vertex : mesh.vertices) {
        check(vertex.material == static_cast<float>(material), "relief keeps its anchor's material ID");
        check(std::abs(glm::length(vertex.normal) - 1.0f) < 0.0001f, "relief normals are unit length");
        check(vertex.color == voxel::blockColor(material), "relief preserves the base color used by the painted material");
        const float depth = glm::dot(vertex.position - faceCenter, normal);
        check(glm::vec3(vertex.relief) == normal && std::abs(vertex.relief.w - depth) < 0.00001f,
              "relief metadata stores the anchor face normal and signed height");
        const auto projected = vertex.position - glm::vec3(vertex.relief) * vertex.relief.w;
        check(std::abs(glm::dot(projected - faceCenter, normal)) < 0.00001f,
              "projected texture positions lie on the original block face");
        if (mineral) check(paintedOre(uv(projected), paintSeed), "raised ore vertices lie inside painted mineral flecks");
        check(depth >= -0.0041f && depth <= maximumPeak + 0.00001f,
              "relief stays embedded and below its material's maximum height");
        peak = std::max(peak, depth);
        embedded |= depth < -0.003f;
        for (int axis = 0; axis < 3; ++axis) {
            check(std::isfinite(vertex.position[axis]) && std::isfinite(vertex.normal[axis])
                  && std::isfinite(vertex.color[axis]), "relief attributes are finite");
            check(vertex.color[axis] >= 0 && vertex.color[axis] <= 1, "relief albedo stays in range");
            if (face[axis] == 0) {
                check(vertex.position[axis] > anchor[axis] && vertex.position[axis] < anchor[axis] + 1,
                      "plates never cross the tangential block edges");
            }
        }
        const glm::ivec3 cell(glm::floor(vertex.position));
        check(cell == anchor || cell == anchor + face, "every vertex stays in the anchor or its adjacent air cell");
    }
    check(embedded && peak >= minimumPeak, "each face has real raised depth and an anchored skirt");
    float raisedCapArea = 0;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size()) {
            check(false, "relief indices reference valid vertices");
            continue;
        }
        const auto cross = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                     mesh.vertices[c].position - mesh.vertices[a].position);
        check(glm::length(cross) > 0.000001f, "all relief triangles have nonzero area");
        check(glm::dot(glm::normalize(cross), mesh.vertices[a].normal) > 0.9999f,
              "counterclockwise triangle winding matches the flat facet normal");
        check(glm::dot(mesh.vertices[a].normal, normal) >= -0.0001f,
              "caps and bevels face outward, while skirts face sideways");
        if (glm::dot(mesh.vertices[a].position - faceCenter, normal) > peak * 0.8f
            && glm::dot(mesh.vertices[b].position - faceCenter, normal) > peak * 0.8f
            && glm::dot(mesh.vertices[c].position - faceCenter, normal) > peak * 0.8f) {
            raisedCapArea += glm::dot(cross, normal) * 0.5f;
        }
        if (!mineral && glm::dot(mesh.vertices[a].normal, normal) > 0.9999f) {
            const auto site = paintedStone(uv(mesh.vertices[a].position), paintSeed, material == voxel::Block::Slate);
            check(site == paintedStone(uv(mesh.vertices[b].position), paintSeed, material == voxel::Block::Slate)
                  && site == paintedStone(uv(mesh.vertices[c].position), paintSeed, material == voxel::Block::Slate),
                  "raised stone caps stay within a single painted Voronoi plate");
        }
    }
    check(raisedCapArea > 0.003f, "raised geometry has planar caps instead of pointed tips");
}

} // namespace

int main()
{
    using namespace voxel;
    World world(42);
    clear(world);
    check(buildBlockRelief(world, centerChunk).indices.empty(), "air has no relief");
    const glm::ivec3 anchor{15, 6, 5};
    // These painted ore fixtures cover bottoms and all negative orientations.
    for (const auto material : {Block::CoalOre, Block::CopperOre, Block::IronOre}) {
        world.set(anchor, material);
        cover(world, anchor);
        check(buildBlockRelief(world, centerChunk).indices.empty(), "fully covered ore has no relief");
        for (const auto face : faces) {
            world.set(anchor + face, Block::Air);
            const auto before = world;
            const auto mesh = buildBlockRelief(world, centerChunk);
            validate(mesh, anchor, face, material, 0.060f, 0.12f);
            check(world.sameContents(before), "building relief leaves voxel contents unchanged");
            check(same(mesh, buildBlockRelief(world, centerChunk)), "relief is deterministic");
            world.set(anchor + face, Block::Dirt);
            check(buildBlockRelief(world, centerChunk).indices.empty(), "covering any face removes its relief");
        }
    }

    world.set(anchor, Block::CopperOre);
    world.set(anchor + faces[0], Block::Air); // x=16 belongs to the next chunk.
    const auto seam = buildBlockRelief(world, centerChunk);
    check(!seam.indices.empty() && buildBlockRelief(world, centerChunk + 1).indices.empty(),
          "cross-chunk relief belongs only to its solid anchor's chunk");
    world.set(anchor + faces[0], Block::Brick);
    check(buildBlockRelief(world, centerChunk).indices.empty(), "a neighboring chunk can cover the relief");
    world.set(anchor + faces[0], Block::Air);
    check(same(seam, buildBlockRelief(world, centerChunk)), "reopening the neighboring chunk restores the same geometry");
    world.set(anchor, Block::Air);
    check(buildBlockRelief(world, centerChunk).indices.empty(), "mining an anchor removes its relief");
    for (const auto material : {Block::Grass, Block::Dirt, Block::Sand, Block::Crystal,
                               Block::Timber, Block::Planks, Block::Brick, Block::Lantern, Block::Bedrock}) {
        world.set(anchor, material);
        check(buildBlockRelief(world, centerChunk).indices.empty(), "other materials never grow stone or ore plates");
    }

    World anotherSeed(43);
    clear(anotherSeed);
    anotherSeed.set(anchor, Block::CopperOre);
    cover(anotherSeed, anchor);
    anotherSeed.set(anchor + faces[0], Block::Air);
    check(!same(seam, buildBlockRelief(anotherSeed, centerChunk)), "world seed varies plate geometry and facets");

    // Adding an earlier anchor must not consume a cap or reshuffle later shapes.
    world.set(anchor, Block::CopperOre);
    world.set({1, 1, 1}, Block::IronOre);
    const auto more = buildBlockRelief(world, centerChunk);
    check(more.vertices.size() > seam.vertices.size()
          && std::equal(seam.vertices.begin(), seam.vertices.end(),
                        more.vertices.end() - static_cast<std::ptrdiff_t>(seam.vertices.size()), sameVertex),
          "unrelated earlier blocks do not change existing relief");

    clear(world);
    const glm::ivec3 edge{World::Min.x, 6, 5};
    world.set(edge, Block::IronOre);
    cover(world, edge);
    check(buildBlockRelief(world, 10).indices.empty(), "world exterior is not an adjacent air cell for relief");

    clear(world);
    const glm::ivec3 negative{-1, 6, -1};
    world.set(negative, Block::IronOre);
    cover(world, negative);
    world.set(negative + faces[0], Block::Air);
    validate(buildBlockRelief(world, 6), negative, faces[0], Block::IronOre, 0.060f, 0.12f);
    world.set(negative + faces[0], Block::Stone);
    check(buildBlockRelief(world, 6).indices.empty(), "negative-coordinate chunk seams also suppress hidden relief");

    // A top-only slab exercises sparse stone density without a first-N chunk cap.
    for (const auto material : {Block::Stone, Block::Limestone, Block::Slate}) {
        clear(world);
        for (int z = -1; z <= 16; ++z) {
            for (int x = -1; x <= 16; ++x) {
                world.set({x, 6, z}, Block::Dirt);
                world.set({x, 7, z}, x >= 0 && x < 16 && z >= 0 && z < 16 ? material : Block::Dirt);
            }
        }
        const auto slab = buildBlockRelief(world, centerChunk);
        std::array<MeshData, 256> byFace;
        for (std::size_t i = 0; i + 2 < slab.indices.size(); i += 3) {
            const auto& a = slab.vertices[slab.indices[i]];
            const int x = static_cast<int>(std::floor(a.position.x));
            const int z = static_cast<int>(std::floor(a.position.z));
            check(x >= 0 && x < 16 && z >= 0 && z < 16, "slab relief stays inside the owning chunk");
            if (x < 0 || x >= 16 || z < 0 || z >= 16) continue;
            auto& face = byFace[x + 16 * z];
            for (int corner = 0; corner < 3; ++corner) {
                const auto& vertex = slab.vertices[slab.indices[i + corner]];
                check(std::floor(vertex.position.x) == x && std::floor(vertex.position.z) == z,
                      "triangles do not cross face boundaries");
                face.indices.push_back(static_cast<std::uint32_t>(face.vertices.size()));
                face.vertices.push_back(vertex);
            }
        }
        int decorated = 0;
        std::array<bool, 4> quadrants{};
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                const auto& face = byFace[x + 16 * z];
                if (face.indices.empty()) continue;
                ++decorated;
                quadrants[(x >= 8) + 2 * (z >= 8)] = true;
                validate(face, {x, 7, z}, {0, 1, 0}, material, 0.025f, 0.075f);
            }
        }
        check(decorated > 90 && decorated < 166, "roughly half of stone faces get plates");
        check(std::all_of(quadrants.begin(), quadrants.end(), [](bool value) { return value; }),
              "stone density reaches every chunk quadrant without truncation");
    }

    bool threw = false;
    try { buildBlockRelief(world, World::ChunkCount); }
    catch (const std::out_of_range&) { threw = true; }
    check(threw, "invalid chunk indices are rejected");
    return failures ? 1 : 0;
}
