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
           float height, float radius, std::uint32_t variation)
{
    constexpr int sides = 5;
    const auto up = glm::cross(normal, across);
    const auto lean = across * (variation % 2 ? 0.06f : -0.04f);
    const auto tip = base + normal * height + lean;
    std::array<glm::vec3, sides> bottom, shoulder;
    for (int side = 0; side < sides; ++side) {
        const float angle = (side + 0.15f) * 6.2831853f / sides;
        const auto radial = across * std::cos(angle) + up * std::sin(angle);
        bottom[side] = base + radial * radius;
        shoulder[side] = base + normal * height * 0.62f + radial * radius * 0.85f + lean * 0.5f;
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

void grassTuft(MeshData& mesh, glm::ivec3 cell, std::uint32_t variation)
{
    const glm::vec3 base = glm::vec3(cell) + glm::vec3(0.5f, 0.99f, 0.5f);
    for (int blade = 0; blade < 4; ++blade) {
        const float angle = blade * 1.7f + (variation % 7) * 0.2f;
        const glm::vec3 across{std::cos(angle), 0, std::sin(angle)};
        const auto center = base + across * 0.12f;
        const auto tip = center + across * 0.18f + glm::vec3(0, 0.30f + blade * 0.065f, 0);
        const auto a = center - across * 0.075f, b = center + across * 0.075f;
        const glm::vec3 color{0.24f + blade * 0.025f, 0.64f + blade * 0.035f, 0.19f};
        // Back faces are explicit because the shared world renderer culls them.
        triangle(mesh, a, b, tip, Block::Grass, color);
        triangle(mesh, b, a, tip, Block::Grass, color);
    }
}

} // namespace

MeshData buildCaveDetails(const World& world, std::size_t chunkIndex)
{
    const auto& chunk = world.chunk(chunkIndex);
    const auto origin = world.origin(chunkIndex);
    MeshData mesh;
    int shards = 0, stones = 0, tufts = 0;
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
                    for (int axis = 0; axis < 3 && shards < 32; ++axis) {
                        for (const int sign : {-1, 1}) {
                            glm::ivec3 face(0);
                            face[axis] = sign;
                            if (shards == 32 || !air(cell + face)) continue;
                            // Alternate exposed faces keep large deposits from becoming hedgehogs.
                            if ((variation + axis + (sign > 0)) % 3 == 0) continue;
                            const glm::vec3 normal(face);
                            glm::vec3 across(0);
                            across[(axis + 1) % 3] = 1.0f;
                            const auto base = glm::vec3(cell) + glm::vec3(0.5f) + normal * 0.485f;
                            shard(mesh, base, normal, across, 0.55f + (variation % 4) * 0.08f,
                                  0.14f + (variation % 3) * 0.025f, variation);
                            ++shards;
                        }
                    }
                } else if (stones < 10 && y >= 10 && variation % 13 == 0 && air(cell + glm::ivec3(0, -1, 0))
                           && (material == Block::Stone || material == Block::Limestone || material == Block::Slate)) {
                    stalactite(mesh, cell, material, variation);
                    ++stones;
                } else if (tufts < 16 && material == Block::Grass && air(cell + glm::ivec3(0, 1, 0))) {
                    grassTuft(mesh, cell, variation);
                    ++tufts;
                }
            }
        }
    }
    return mesh;
}

} // namespace voxel
