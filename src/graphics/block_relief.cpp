#include "graphics/block_relief.h"
#include "graphics/built_surface_relief.h"
#include "block_shapes.h"

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

// The empty half of a neighboring stair remains visible; its occupied lower
// slab and upper half must clip chips just as they clip the block mesh itself.
bool exposedSurface(const World& world, glm::ivec3 cell, BlockSurface& surface)
{
    const glm::vec3 normal(surface.normal);
    const auto middle = glm::vec3(cell) + (surface.minimum + surface.maximum) * .5f;
    const auto probe = middle + normal * .001f;
    const glm::ivec3 adjacent(glm::floor(probe));
    if (!World::contains(adjacent)) return false;
    if (adjacent == cell) return true;
    const auto neighbor = world.get(adjacent);
    if (neighbor == Block::Air) return true;
    if (!isStair(neighbor)) return false;
    const auto upper = blockShape(neighbor).boxes[1];
    glm::vec3 low(0, .5f, 0), high(1);
    for (const int axis : {0, 2}) {
        if (upper.minimum[axis] > 0) high[axis] = .5f;
        if (upper.maximum[axis] < 1) low[axis] = .5f;
    }
    low += glm::vec3(adjacent - cell);
    high += glm::vec3(adjacent - cell);
    for (int axis = 0; axis < 3; ++axis) {
        if (normal[axis] != 0) {
            const float position = probe[axis] - cell[axis];
            if (position <= low[axis] || position >= high[axis]) return false;
        } else {
            surface.minimum[axis] = std::max(surface.minimum[axis], low[axis]);
            surface.maximum[axis] = std::min(surface.maximum[axis], high[axis]);
            if (surface.minimum[axis] >= surface.maximum[axis]) return false;
        }
    }
    return true;
}

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
    return (block >= Block::CoalOre && block <= Block::IronOre)
        || (block >= Block::TinOre && block <= Block::Obsidian);
}

// Match mineralAspect in materials.h so exposed geometry remains inside paint.
glm::vec2 mineralAspect(Block block)
{
    switch (block) {
    case Block::TinOre: return {.90f, .75f};
    case Block::SilverOre: return {1.10f, .85f};
    case Block::GoldOre: return {.85f, 1.20f};
    case Block::QuartzOre: return {1.12f, 1.50f};
    case Block::JadeOre: return {1.12f, 1.80f};
    case Block::AmethystOre: return {.88f, 1.60f};
    case Block::RubyOre: return {.80f, 1.80f};
    case Block::SapphireOre: return {.92f, 1.60f};
    case Block::EmeraldOre: return {1.20f, 1.40f};
    case Block::CobaltOre: return {1.22f, .72f};
    case Block::TitaniumOre: return {1.32f, .70f};
    case Block::MithrilOre: return {1.30f, .75f};
    case Block::Obsidian: return {1.35f, .90f};
    default: return {1, 1};
    }
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

std::vector<Patch> stoneFracture(glm::vec2 low, glm::vec2 high, float seed, bool slate,
                               bool stair)
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
    if (stair && !polygons.empty()) {
        // A small tread can lie entirely within one painted rock plate. Give
        // it a shallow cut across that plate instead of randomly leaving a
        // whole stair face flat; the cut still fits inside the same pigment.
        auto polygon = polygons.front();
        glm::vec2 center(0);
        for (const auto point : polygon) center += point;
        center /= static_cast<float>(polygon.size());
        const auto direction = glm::normalize(glm::vec2(1, .35f));
        polygon = clip(polygon, direction, glm::dot(center, direction) + .055f);
        polygon = clip(polygon, -direction, -glm::dot(center, direction) + .075f);
        if (polygon.size() >= 3 && area(polygon) > .004f) return {{std::move(polygon), direction}};
    }
    return {};
}

std::vector<Patch> oreVeins(glm::vec2 low, glm::vec2 high, float seed, Block material)
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
            const auto aspect = mineralAspect(material);
            const float length = (0.32f + 0.13f * h2) * aspect.x;
            const float width = (0.10f + 0.05f * h3) * aspect.y;
            Polygon corners;
            if (material >= Block::QuartzOre && material <= Block::EmeraldOre) {
                corners = {{-length, -width * .15f}, {-length * .45f, -width},
                    {length * .45f, -width * .85f}, {length, width * .20f},
                    {length * .45f, width}, {-length * .40f, width * .85f}};
            } else {
                corners = {{-length, -width * .15f}, {-length * .25f, -width},
                    {length, width * .20f}, {length * .20f, width}};
            }
            Polygon polygon;
            for (const auto q : corners) {
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
    keepLargest(polygons, 3);
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

std::vector<Polygon> crystalFacets(glm::vec2 low, glm::vec2 high, float seed, bool top)
{
    // The same elongated Voronoi cells color crystal and amber in materials.h.
    // A few broad facets lift from the body of the mineral, not a scattering
    // of separate protruding stones.
    const auto metric = [top](glm::vec2 point) {
        return top ? point * 1.8f : glm::vec2(point.x * 2.8f + point.y * .22f, point.y * .70f);
    };
    const auto inverse = [top](glm::vec2 point) {
        return top ? point / 1.8f : glm::vec2((point.x - point.y / .70f * .22f) / 2.8f, point.y / .70f);
    };
    const auto first = glm::ivec2(glm::floor(metric(low)));
    const auto last = glm::ivec2(glm::floor(metric(high)));
    std::vector<glm::vec2> sites;
    for (int y = first.y - 1; y <= last.y + 1; ++y) {
        for (int x = first.x - 1; x <= last.x + 1; ++x) {
            const glm::vec2 tile(x, y);
            const float h = mineralHash(tile + seed);
            sites.push_back(tile + .2f + .6f * glm::vec2(h, glm::fract(h * 13.71f)));
        }
    }
    std::vector<Polygon> polygons;
    for (const auto site : sites) {
        auto polygon = rectangle(low, high);
        for (auto& point : polygon) point = metric(point) - site;
        for (const auto other : sites) {
            const auto offset = other - site;
            if (offset == glm::vec2(0)) continue;
            polygon = clip(polygon, offset, glm::dot(offset, offset) * .5f);
            if (polygon.empty()) break;
        }
        for (auto& point : polygon) point = inverse(point + site);
        polygons.push_back(std::move(polygon));
    }
    keepLargest(polygons, 3);
    return polygons;
}

void triangle(MeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, Block material,
              glm::vec3 faceNormal, glm::vec3 faceCenter, bool reverse)
{
    if (reverse) std::swap(b, c);
    const auto cross = glm::cross(b - a, c - a);
    // A clipped outline can leave a near-zero skirt at a half-block edge.
    // Skip that sliver before normalizing so lighting never receives NaNs.
    if (glm::length(cross) <= .000001f) return;
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto normal = glm::normalize(cross);
    for (const auto point : {a, b, c}) {
        mesh.vertices.push_back({point, normal, blockColor(material), static_cast<float>(material),
                                 glm::vec4(faceNormal, glm::dot(point - faceCenter, faceNormal))});
    }
    mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2});
}

void wedge(MeshData& mesh, const Patch& patch, glm::vec3 faceCenter, glm::vec3 normal,
           glm::vec3 u, glm::vec3 v, glm::vec2 boundsLow, glm::vec2 boundsHigh, float height, Block material)
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
        const auto distance = glm::min(point - boundsLow, boundsHigh - point);
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

void crystalFacet(MeshData& mesh, const Polygon& polygon, glm::vec3 faceCenter, glm::vec3 normal,
                  glm::vec3 u, glm::vec3 v, float height, Block material)
{
    glm::vec2 center(0);
    for (const auto point : polygon) center += point;
    center /= static_cast<float>(polygon.size());
    const glm::vec2 faceUV(glm::dot(faceCenter, u), glm::dot(faceCenter, v));
    const bool reverse = glm::dot(glm::cross(u, v), normal) < 0;
    const auto position = [&](glm::vec2 point, float depth) {
        const auto offset = point - faceUV;
        return faceCenter + u * offset.x + v * offset.y + normal * depth;
    };
    // The perimeter sinks into the crystal body. An off-center ridge makes
    // wide planes catch a light at different angles without a raised collar.
    const auto peak = center + (polygon[0] - center) * .18f;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto a = center + (polygon[i] - center) * .98f;
        const auto b = center + (polygon[(i + 1) % polygon.size()] - center) * .98f;
        triangle(mesh, position(peak, height), position(a, -.004f), position(b, -.004f),
                 material, normal, faceCenter, reverse);
    }
}

} // namespace

MeshData buildBlockRelief(const World& world, std::size_t chunkIndex)
{
    const auto& chunk = world.chunk(chunkIndex);
    const auto origin = world.origin(chunkIndex);
    MeshData mesh;
    for (int y = 0; y < Chunk::Size; ++y) {
        for (int z = 0; z < Chunk::Size; ++z) {
            for (int x = 0; x < Chunk::Size; ++x) {
                const auto block = chunk.get(x, y, z);
                const auto material = stairMaterial(block);
                const bool mineral = ore(material);
                const bool crystal = material == Block::Crystal || material == Block::AmberCrystal;
                const bool built = hasBuiltSurfaceRelief(material);
                if (!mineral && !crystal && !built && material != Block::Stone && material != Block::Limestone
                    && material != Block::Slate && material != Block::MossStone && material != Block::Calcite) continue;
                const auto cell = origin + glm::ivec3(x, y, z);
                const auto surfaces = blockSurfaces(block);
                for (int surfaceIndex = 0; surfaceIndex < surfaces.count; ++surfaceIndex) {
                    auto surface = surfaces.faces[surfaceIndex];
                    if (!exposedSurface(world, cell, surface)) continue;
                    if (built) {
                        appendBuiltSurfaceRelief(mesh, cell, surface, material, world.seed());
                        continue;
                    }
                    const glm::vec3 normal(surface.normal);
                    const auto faceCenter = glm::vec3(cell) + (surface.minimum + surface.maximum) * .5f;
                    const int face = surface.normal.x != 0 ? (surface.normal.x > 0 ? 0 : 1)
                        : surface.normal.y != 0 ? (surface.normal.y > 0 ? 2 : 3) : (surface.normal.z > 0 ? 4 : 5);
                    const auto hash = faceHash(cell, face, world.seed());
                    if (!mineral && !crystal && !isStair(block) && hash % (normal.y > 0 ? 12u : 5u) != 0) continue;
                    const glm::vec3 u = normal.x != 0 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
                    const glm::vec3 v = normal.y != 0 ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
                    const auto minimum = glm::vec3(cell) + surface.minimum;
                    const auto maximum = glm::vec3(cell) + surface.maximum;
                    const glm::vec2 low = glm::vec2(glm::dot(minimum, u), glm::dot(minimum, v)) + .008f;
                    const glm::vec2 high = glm::vec2(glm::dot(maximum, u), glm::dot(maximum, v)) - .008f;
                    const float seed = glm::dot(glm::vec3(cell) * glm::abs(normal), glm::vec3(17, 37, 57))
                        + static_cast<float>(material) * 19.0f;
                    if (crystal) {
                        const auto facets = crystalFacets(low, high, seed, normal.y != 0);
                        for (std::size_t piece = 0; piece < facets.size(); ++piece) {
                            const float variation = float(mix(hash ^ static_cast<std::uint32_t>(piece)) & 0xffffu) / 65535.0f;
                            crystalFacet(mesh, facets[piece], faceCenter, normal, u, v,
                                         .030f + variation * .030f, material);
                        }
                        continue;
                    }
                    const auto patches = mineral ? oreVeins(low, high, seed, material)
                        : stoneFracture(low, high, seed, material == Block::Slate, isStair(block));
                    for (std::size_t piece = 0; piece < patches.size(); ++piece) {
                        const float variation = float(mix(hash ^ static_cast<std::uint32_t>(piece)) & 0xffffu) / 65535.0f;
                        const float height = mineral ? 0.040f + variation * 0.030f : 0.012f + variation * 0.018f;
                        wedge(mesh, patches[piece], faceCenter, normal, u, v, low, high, height, material);
                    }
                }
            }
        }
    }
    return mesh;
}

} // namespace voxel
