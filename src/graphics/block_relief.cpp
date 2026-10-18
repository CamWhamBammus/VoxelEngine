#include "graphics/block_relief.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace voxel {
namespace {
using Polygon = std::vector<glm::vec2>;
struct Patch {
    Polygon outline;
    glm::vec2 rise; // Direction along the face from the flush end to the raised edge.
};

std::uint32_t mix(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

std::uint32_t faceHash(glm::ivec3 cell, int face, std::uint32_t seed)
{
    return mix(seed ^ (static_cast<std::uint32_t>(cell.x) * 0x9e3779b9u)
                    ^ (static_cast<std::uint32_t>(cell.y) * 0x85ebca6bu)
                    ^ (static_cast<std::uint32_t>(cell.z) * 0xc2b2ae35u)
                    ^ (static_cast<std::uint32_t>(face + 1) * 0x27d4eb2du));
}

// Shared with mineralHash in materials.h. Integer arithmetic and a power-of-two
// divisor keep CPU outlines and GPU paint identical across graphics drivers.
float mineralHash(glm::vec2 p)
{
    const glm::ivec2 cell(p);
    const auto value = static_cast<std::uint32_t>(cell.x) * 0x9e3779b9u
                     ^ static_cast<std::uint32_t>(cell.y) * 0x85ebca6bu;
    return static_cast<float>(mix(value) & 0x00ffffffu) / 16777216.0f;
}

bool ore(Block block)
{
    return block == Block::CoalOre || block == Block::CopperOre || block == Block::IronOre;
}

Polygon rectangle(glm::vec2 low, glm::vec2 high)
{
    return {{low.x, low.y}, {high.x, low.y}, {high.x, high.y}, {low.x, high.y}};
}

// Keep the half of a convex polygon where dot(point, normal) <= limit.
Polygon clip(const Polygon& polygon, glm::vec2 normal, float limit)
{
    Polygon result;
    if (polygon.empty()) return result;
    auto previous = polygon.back();
    float previousDistance = glm::dot(previous, normal) - limit;
    for (const auto point : polygon) {
        const float distance = glm::dot(point, normal) - limit;
        if ((distance <= 0) != (previousDistance <= 0)) {
            result.push_back(previous + (point - previous) * (previousDistance / (previousDistance - distance)));
        }
        if (distance <= 0) result.push_back(point);
        previous = point;
        previousDistance = distance;
    }
    return result;
}

float area(const Polygon& polygon)
{
    float result = 0;
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
        const auto a = polygon[i] - polygon[0], b = polygon[i + 1] - polygon[0];
        result += a.x * b.y - a.y * b.x;
    }
    return std::abs(result) * 0.5f;
}

void keepLargest(std::vector<Polygon>& polygons, std::size_t count)
{
    // Clipping can leave tiny slivers or duplicate corners at a boundary.
    for (auto& polygon : polygons) {
        for (std::size_t i = 0; polygon.size() >= 3 && i < polygon.size();) {
            const auto a = polygon[i] - polygon[(i + polygon.size() - 1) % polygon.size()];
            const auto b = polygon[(i + 1) % polygon.size()] - polygon[i];
            if (std::abs(a.x * b.y - a.y * b.x) < 0.000001f) polygon.erase(polygon.begin() + i);
            else ++i;
        }
    }
    std::erase_if(polygons, [](const Polygon& p) { return p.size() < 3 || p.size() > 12 || area(p) < 0.006f; });
    std::stable_sort(polygons.begin(), polygons.end(), [](const Polygon& a, const Polygon& b) { return area(a) > area(b); });
    if (polygons.size() > count) polygons.resize(count);
}

std::vector<Patch> stoneFracture(glm::vec2 low, glm::vec2 high, float seed, bool slate)
{
    const glm::vec2 scale(1.35f, slate ? 3.2f : 1.65f);
    const glm::ivec2 first(glm::floor(low * scale)), last(glm::floor(high * scale));
    std::vector<glm::vec2> sites;
    for (int y = first.y - 1; y <= last.y + 1; ++y) {
        for (int x = first.x - 1; x <= last.x + 1; ++x) {
            const glm::vec2 tile(x, y);
            const float h = mineralHash(tile + seed);
            sites.push_back(tile + 0.18f + 0.64f * glm::vec2(h, glm::fract(h * 17.73f)));
        }
    }
    std::vector<Polygon> polygons;
    for (const auto site : sites) {
        auto polygon = rectangle(low * scale - site, high * scale - site);
        for (const auto other : sites) {
            const auto offset = other - site;
            if (offset == glm::vec2(0)) continue;
            polygon = clip(polygon, offset, glm::dot(offset, offset) * 0.5f);
            if (polygon.empty()) break;
        }
        for (auto& point : polygon) point = (point + site) / scale;
        polygons.push_back(std::move(polygon));
    }
    keepLargest(polygons, polygons.size());
    for (const auto& polygon : polygons) {
        float longest = 0;
        glm::vec2 edgeStart{}, inward{};
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
            // A clipped block boundary is not a mineral fracture.
            bool boundary = false;
            for (int axis = 0; axis < 2; ++axis) {
                boundary |= (std::abs(a[axis] - low[axis]) < 0.0001f && std::abs(b[axis] - low[axis]) < 0.0001f)
                         || (std::abs(a[axis] - high[axis]) < 0.0001f && std::abs(b[axis] - high[axis]) < 0.0001f);
            }
            const auto edge = b - a;
            const float length = glm::length(edge);
            if (!boundary && length > longest) {
                longest = length;
                edgeStart = a;
                inward = glm::vec2(-edge.y, edge.x) / length;
            }
        }
        if (longest < 0.08f) continue;
        const float width = glm::clamp(longest * 0.2f, 0.07f, 0.14f);
        std::vector<Polygon> strip{clip(polygon, inward, glm::dot(edgeStart, inward) + width)};
        keepLargest(strip, 1);
        if (!strip.empty()) return {{std::move(strip[0]), -inward}};
    }
    return {};
}

std::vector<Patch> oreVeins(glm::vec2 low, glm::vec2 high, float seed)
{
    const glm::ivec2 first(glm::floor(low * 2.0f)), last(glm::floor(high * 2.0f));
    std::vector<Polygon> polygons;
    for (int y = first.y; y <= last.y; ++y) {
        for (int x = first.x; x <= last.x; ++x) {
            const glm::vec2 tile(x, y);
            const float h = mineralHash(tile + seed), h2 = mineralHash(tile + seed + 11.0f);
            const float h3 = mineralHash(tile + seed + 23.0f);
            if (h <= 0.32f) continue;
            const auto center = tile + 0.2f + 0.6f * glm::vec2(h, h2);
            const float sign = mineralHash(glm::floor(tile / 3.0f) + seed + 47.0f) > 0.5f ? 1.0f : -1.0f;
            const glm::vec2 u(0.8f, sign * 0.6f), v(-sign * 0.6f, 0.8f);
            const float length = 0.32f + 0.13f * h2, width = 0.10f + 0.05f * h3;
            Polygon polygon;
            for (const auto q : {glm::vec2(-length, -width * 0.15f), glm::vec2(-length * 0.25f, -width),
                                 glm::vec2(length, width * 0.20f), glm::vec2(length * 0.20f, width)}) {
                polygon.push_back((center + u * q.x + v * q.y) / 2.0f);
            }
            const auto minimum = glm::max(low, tile / 2.0f), maximum = glm::min(high, (tile + 1.0f) / 2.0f);
            polygon = clip(polygon, {-1, 0}, -minimum.x);
            polygon = clip(polygon, {1, 0}, maximum.x);
            polygon = clip(polygon, {0, -1}, -minimum.y);
            polygon = clip(polygon, {0, 1}, maximum.y);
            polygons.push_back(std::move(polygon));
        }
    }
    keepLargest(polygons, 2);
    std::vector<Patch> patches;
    for (auto& polygon : polygons) {
        glm::vec2 center(0);
        for (const auto point : polygon) center += point;
        center /= static_cast<float>(polygon.size());
        const auto tile = glm::floor(center * 2.0f);
        const float sign = mineralHash(glm::floor(tile / 3.0f) + seed + 47.0f) > 0.5f ? 1.0f : -1.0f;
        patches.push_back({std::move(polygon), {0.8f, sign * 0.6f}});
    }
    return patches;
}

void triangle(MeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, Block material,
              glm::vec3 faceNormal, glm::vec3 faceCenter, bool reverse)
{
    if (reverse) std::swap(b, c);
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto normal = glm::normalize(glm::cross(b - a, c - a));
    for (const auto point : {a, b, c}) {
        mesh.vertices.push_back({point, normal, blockColor(material), static_cast<float>(material),
                                 glm::vec4(faceNormal, glm::dot(point - faceCenter, faceNormal))});
    }
    mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2});
}

void wedge(MeshData& mesh, const Patch& patch, glm::vec3 faceCenter, glm::vec3 normal,
           glm::vec3 u, glm::vec3 v, float height, Block material)
{
    auto polygon = patch.outline;
    glm::vec2 center(0);
    for (const auto point : polygon) center += point;
    center /= static_cast<float>(polygon.size());
    const glm::vec2 faceUV(glm::dot(faceCenter, u), glm::dot(faceCenter, v));
    const bool reverse = glm::dot(glm::cross(u, v), normal) < 0;
    float low = glm::dot(polygon.front(), patch.rise), high = low;
    for (const auto point : polygon) {
        low = std::min(low, glm::dot(point, patch.rise));
        high = std::max(high, glm::dot(point, patch.rise));
    }
    const bool mineral = ore(material);
    const glm::vec2 across(-patch.rise.y, patch.rise.x);
    float sideLow = glm::dot(polygon.front(), across), sideHigh = sideLow;
    if (mineral) {
        for (const auto point : polygon) {
            sideLow = std::min(sideLow, glm::dot(point, across));
            sideHigh = std::max(sideHigh, glm::dot(point, across));
        }
    }
    if (!mineral) {
        // Keep a point on the lifted fracture when its two ends meet block
        // boundaries. The ends can then fade flush without flattening it all.
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
            if (high - glm::dot(a, patch.rise) < 0.0001f && high - glm::dot(b, patch.rise) < 0.0001f) {
                polygon.insert(polygon.begin() + i + 1, (a + b) * 0.5f);
                break;
            }
        }
    }
    const auto depthAt = [&](glm::vec2 point) {
        const float fraction = (glm::dot(point, patch.rise) - low) / (high - low);
        if (mineral) {
            const float side = (glm::dot(point, across) - sideLow) / (sideHigh - sideLow);
            // Bury the low tip and one side. The opposite lateral corner is
            // the short ridge; the far tip is lower, so no full kite lifts off.
            const float exposure = glm::smoothstep(0.025f, 0.425f, fraction)
                * glm::smoothstep(0.0f, 0.4f, side)
                * (1.0f - 0.65f * glm::smoothstep(0.6f, 1.0f, fraction));
            return -0.0025f + (height + 0.0025f) * exposure;
        }
        const auto distance = glm::vec2(0.492f) - glm::abs(point - faceUV);
        const float taper = glm::clamp(std::min(distance.x, distance.y) / 0.065f, 0.0f, 1.0f);
        return -0.001f + (height + 0.001f) * fraction * taper;
    };
    std::vector<glm::vec3> bottom, cap;
    for (const auto point : polygon) {
        // The cap merges into the wall on its buried edges. There is no
        // constant-height collar around the full mineral outline.
        const auto outer = center + (point - center) * 0.99f - faceUV;
        const auto inner = center + (point - center) * 0.97f - faceUV;
        const float depth = depthAt(point);
        bottom.push_back(faceCenter + u * outer.x + v * outer.y - normal * 0.004f);
        cap.push_back(faceCenter + u * inner.x + v * inner.y + normal * depth);
    }
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto next = (i + 1) % polygon.size();
        triangle(mesh, bottom[i], bottom[next], cap[next], material, normal, faceCenter, reverse);
        triangle(mesh, bottom[i], cap[next], cap[i], material, normal, faceCenter, reverse);
    }
    if (mineral) {
        for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
            triangle(mesh, cap[0], cap[i], cap[i + 1], material, normal, faceCenter, reverse);
        }
    } else {
        const auto offset = center - faceUV;
        const auto middle = faceCenter + u * offset.x + v * offset.y + normal * depthAt(center);
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            triangle(mesh, middle, cap[i], cap[(i + 1) % polygon.size()], material, normal, faceCenter, reverse);
        }
    }
}

} // namespace

MeshData buildBlockRelief(const World& world, std::size_t chunkIndex)
{
    const auto& chunk = world.chunk(chunkIndex);
    const auto origin = world.origin(chunkIndex);
    constexpr std::array<glm::ivec3, 6> faces{{
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
    }};
    MeshData mesh;
    for (int y = 0; y < Chunk::Size; ++y) {
        for (int z = 0; z < Chunk::Size; ++z) {
            for (int x = 0; x < Chunk::Size; ++x) {
                const auto material = chunk.get(x, y, z);
                const bool mineral = ore(material);
                if (!mineral && material != Block::Stone && material != Block::Limestone && material != Block::Slate) continue;
                const auto cell = origin + glm::ivec3(x, y, z);
                for (int face = 0; face < static_cast<int>(faces.size()); ++face) {
                    const auto adjacent = cell + faces[face];
                    if (!World::contains(adjacent) || world.get(adjacent) != Block::Air) continue;
                    const auto hash = faceHash(cell, face, world.seed());
                    if (!mineral && hash % (faces[face].y > 0 ? 12u : 5u) != 0) continue;
                    const glm::vec3 normal(faces[face]);
                    const glm::vec3 u = normal.x != 0 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
                    const glm::vec3 v = normal.y != 0 ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
                    const auto faceCenter = glm::vec3(cell) + glm::vec3(0.5f) + normal * 0.5f;
                    const glm::vec2 low(glm::dot(glm::vec3(cell), u), glm::dot(glm::vec3(cell), v));
                    const float seed = glm::dot(glm::vec3(cell) * glm::abs(normal), glm::vec3(17, 37, 57))
                        + static_cast<float>(material) * 19.0f;
                    const auto patches = mineral ? oreVeins(low + 0.008f, low + 0.992f, seed)
                        : stoneFracture(low + 0.008f, low + 0.992f, seed, material == Block::Slate);
                    for (std::size_t piece = 0; piece < patches.size(); ++piece) {
                        const float variation = float(mix(hash ^ static_cast<std::uint32_t>(piece)) & 0xffffu) / 65535.0f;
                        const float height = mineral ? 0.025f + variation * 0.025f : 0.012f + variation * 0.018f;
                        wedge(mesh, patches[piece], faceCenter, normal, u, v, height, material);
                    }
                }
            }
        }
    }
    return mesh;
}

} // namespace voxel
