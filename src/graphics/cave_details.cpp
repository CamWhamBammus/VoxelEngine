#include "graphics/cave_details.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>

namespace voxel {
namespace {

std::uint32_t hash(glm::ivec3 cell, std::uint32_t seed)
{
    auto value = seed ^ (static_cast<std::uint32_t>(cell.x) * 0x9e3779b9u)
                      ^ (static_cast<std::uint32_t>(cell.y) * 0x85ebca6bu)
                      ^ (static_cast<std::uint32_t>(cell.z) * 0xc2b2ae35u);
    value ^= value >> 16;
    value *= 0x7feb352du;
    return value ^ (value >> 15);
}

void triangle(MeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, Block material, glm::vec3 color)
{
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto normal = glm::normalize(glm::cross(b - a, c - a));
    for (const auto point : {a, b, c}) {
        mesh.vertices.push_back({point, normal, color, static_cast<float>(material)});
    }
    mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2});
}

void shard(MeshData& mesh, glm::vec3 base, glm::vec3 normal, glm::vec3 across,
           float height, float radius, glm::vec3 lean, std::uint32_t variation)
{
    constexpr int sides = 5;
    const auto up = glm::cross(normal, across);
    const auto tip = base + normal * height + lean;
    std::array<glm::vec3, sides> bottom, shoulder;
    for (int side = 0; side < sides; ++side) {
        const float angle = (side + 0.15f) * 6.2831853f / sides;
        const auto radial = across * std::cos(angle) + up * std::sin(angle);
        bottom[side] = base + radial * radius;
        shoulder[side] = base + normal * height * 0.72f + radial * radius * 0.82f + lean * 0.72f;
    }
    constexpr std::array<glm::vec3, 5> colors{{
        {0.12f, 0.80f, 0.92f}, {0.18f, 0.63f, 0.98f}, {0.30f, 0.92f, 0.96f},
        {0.12f, 0.70f, 0.86f}, {0.40f, 0.84f, 1.0f},
    }};
    for (int side = 0; side < sides; ++side) {
        const int next = (side + 1) % sides;
        const auto color = colors[(side + variation) % colors.size()];
        triangle(mesh, bottom[side], bottom[next], shoulder[next], Block::Crystal, color);
        triangle(mesh, bottom[side], shoulder[next], shoulder[side], Block::Crystal, color);
        triangle(mesh, shoulder[side], shoulder[next], tip, Block::Crystal, color);
    }
    // The open base sits just inside its solid anchor, so no hidden cap is needed.
}

void stalactite(MeshData& mesh, glm::ivec3 cell, Block material, std::uint32_t variation)
{
    const glm::vec3 base = glm::vec3(cell) + glm::vec3(0.5f, 0.015f, 0.5f);
    const glm::vec3 tip = base + glm::vec3(0.04f, -0.40f - (variation % 5) * 0.08f, -0.03f);
    constexpr int sides = 6;
    for (int side = 0; side < sides; ++side) {
        const float firstAngle = side * 6.2831853f / sides;
        const float nextAngle = (side + 1) * 6.2831853f / sides;
        const auto a = base + glm::vec3(std::cos(firstAngle), 0, std::sin(firstAngle)) * 0.23f;
        const auto b = base + glm::vec3(std::cos(nextAngle), 0, std::sin(nextAngle)) * 0.23f;
        const auto color = blockColor(material) * (0.88f + 0.04f * (side % 4));
        triangle(mesh, a, b, tip, material, color);
    }
}

void grassTuft(MeshData& mesh, glm::ivec3 cell, std::uint32_t variation, bool flowering)
{
    const glm::vec3 base = glm::vec3(cell) + glm::vec3(0.5f, 0.99f, 0.5f);
    glm::vec3 flowerCenter{};
    for (int blade = 0; blade < 4; ++blade) {
        const float angle = blade * 2.3f + (variation % 11) * 0.37f;
        const glm::vec3 across{std::cos(angle), 0, std::sin(angle)};
        const auto root = base + across * 0.06f;
        const float height = 0.30f + ((variation >> (blade * 3)) % 5) * 0.04f;
        const auto shoulder = root + across * 0.06f + glm::vec3(0, height * 0.55f, 0);
        const auto tip = root + across * 0.18f + glm::vec3(0, height, 0);
        const auto a = shoulder - across * 0.09f, b = shoulder + across * 0.09f;
        const glm::vec3 color{0.20f + blade * 0.035f, 0.55f + blade * 0.045f, 0.13f + blade * 0.02f};
        if (blade == 0) flowerCenter = tip;
        // Broad bent leaves taper at both ends, rather than identical spikes.
        triangle(mesh, root, a, tip, Block::Grass, color);
        triangle(mesh, root, tip, b, Block::Grass, color * 0.88f);
        // Back faces are explicit because the shared world renderer culls them.
        triangle(mesh, root, tip, a, Block::Grass, color);
        triangle(mesh, root, b, tip, Block::Grass, color * 0.88f);
    }
    if (flowering) {
        const glm::vec3 color = variation % 2 ? glm::vec3(0.86f, 0.35f, 0.72f) : glm::vec3(0.95f, 0.73f, 0.25f);
        for (int petal = 0; petal < 4; ++petal) {
            const float angle = petal * 1.5707963f;
            const glm::vec3 along{std::cos(angle), 0, std::sin(angle)};
            const glm::vec3 across{-along.z, 0, along.x};
            const auto a = flowerCenter - across * 0.035f;
            const auto b = flowerCenter + along * 0.13f + glm::vec3(0, 0.015f, 0);
            const auto c = flowerCenter + across * 0.035f;
            triangle(mesh, a, b, c, Block::Grass, color);
            triangle(mesh, c, b, a, Block::Grass, color);
        }
    }
}

} // namespace

MeshData buildCaveDetails(const World& world, std::size_t chunkIndex)
{
    const auto& chunk = world.chunk(chunkIndex);
    const auto origin = world.origin(chunkIndex);
    MeshData mesh;
    // Independent caps reserve the silhouette budget for upright clusters.
    // 24 top + 4 wall shards, 10 stones, 12 tufts, 3 flowers = 696 triangles.
    int topShards = 0, wallShards = 0, stones = 0, tufts = 0, flowers = 0;
    for (int y = 0; y < Chunk::Size; ++y) {
        for (int z = 0; z < Chunk::Size; ++z) {
            for (int x = 0; x < Chunk::Size; ++x) {
                const auto cell = origin + glm::ivec3(x, y, z);
                const auto material = chunk.get(x, y, z);
                const auto variation = hash(cell, world.seed());
                const auto air = [&world](glm::ivec3 neighbor) {
                    return World::contains(neighbor) && world.get(neighbor) == Block::Air;
                };
                if (material == Block::Crystal) {
                    if (topShards < 24 && air(cell + glm::ivec3(0, 1, 0))) {
                        const float angle = (variation % 17) * 0.37f;
                        const glm::vec3 across{std::cos(angle), 0, std::sin(angle)};
                        const auto base = glm::vec3(cell) + glm::vec3(0.5f, 0.985f, 0.5f);
                        // One tall mineral and a smaller companion share an uneven base.
                        shard(mesh, base - across * 0.13f, {0, 1, 0}, across,
                              0.78f + (variation % 4) * 0.05f, 0.14f + (variation % 3) * 0.025f,
                              -across * 0.10f, variation);
                        shard(mesh, base + across * 0.24f, {0, 1, 0}, across,
                              0.40f + ((variation >> 3) % 4) * 0.05f, 0.09f + ((variation >> 5) % 3) * 0.015f,
                              across * 0.08f, variation + 2);
                        topShards += 2;
                    }
                    // A few stubby upward-leaning wall minerals break the outline.
                    // Never decorate the underside or repeat a spike on every face.
                    if (wallShards < 4 && variation % 7 == 0) {
                        constexpr std::array<glm::ivec3, 4> faces{{{1, 0, 0}, {0, 0, 1}, {-1, 0, 0}, {0, 0, -1}}};
                        const auto face = faces[(variation >> 4) % faces.size()];
                        if (air(cell + face)) {
                            const glm::vec3 normal(face);
                            const auto across = glm::cross(glm::vec3(0, 1, 0), normal);
                            const auto base = glm::vec3(cell) + glm::vec3(0.5f, 0.32f, 0.5f) + normal * 0.485f;
                            shard(mesh, base, normal, across, 0.28f + (variation % 3) * 0.06f,
                                  0.11f, {0, 0.20f, 0}, variation);
                            ++wallShards;
                        }
                    }
                } else if (stones < 10 && y >= 10 && variation % 13 == 0 && air(cell + glm::ivec3(0, -1, 0))
                           && (material == Block::Stone || material == Block::Limestone || material == Block::Slate)) {
                    stalactite(mesh, cell, material, variation);
                    ++stones;
                } else if (tufts < 12 && material == Block::Grass && air(cell + glm::ivec3(0, 1, 0))) {
                    const bool flowering = flowers < 3 && variation % 5 == 0;
                    grassTuft(mesh, cell, variation, flowering);
                    flowers += flowering;
                    ++tufts;
                }
            }
        }
    }
    return mesh;
}

} // namespace voxel
