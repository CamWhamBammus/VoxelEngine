#include "graphics/built_surface_relief.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <utility>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace voxel {
namespace {
using Polygon = std::vector<glm::vec2>;

std::uint32_t hash(std::uint32_t value)
{
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

std::uint32_t tileHash(glm::ivec2 tile, std::uint32_t seed)
{
    return hash(seed ^ static_cast<std::uint32_t>(tile.x) * 0x9e3779b9u
                     ^ static_cast<std::uint32_t>(tile.y) * 0x85ebca6bu);
}

float variation(std::uint32_t seed, std::uint32_t part)
{
    return static_cast<float>(hash(seed ^ part * 0x9e3779b9u) & 0xffffu) / 65535.0f;
}

glm::vec3 pavingColor(std::uint32_t seed, std::uint32_t part)
{
    // Same warm limestone and cool gray palette as the village's fitted slabs.
    constexpr std::array<glm::vec3, 5> palette{{
        {.57f,.55f,.47f}, {.65f,.60f,.49f}, {.51f,.55f,.52f},
        {.69f,.65f,.55f}, {.57f,.53f,.46f}
    }};
    return palette[hash(seed ^ part * 0x9e3779b9u) % palette.size()]
        * (.94f + .06f * variation(seed, part + 13));
}

Polygon rectangle(glm::vec2 low, glm::vec2 high)
{
    return {{low.x, low.y}, {high.x, low.y}, {high.x, high.y}, {low.x, high.y}};
}

Polygon clip(const Polygon& polygon, int axis, float limit, bool upper)
{
    Polygon result;
    if (polygon.empty()) return result;
    auto previous = polygon.back();
    float previousDistance = (previous[axis] - limit) * (upper ? 1.0f : -1.0f);
    for (const auto point : polygon) {
        const float distance = (point[axis] - limit) * (upper ? 1.0f : -1.0f);
        if ((distance <= 0) != (previousDistance <= 0))
            result.push_back(previous + (point - previous) * (previousDistance / (previousDistance - distance)));
        if (distance <= 0) result.push_back(point);
        previous = point;
        previousDistance = distance;
    }
    return result;
}

Polygon clipToSurface(Polygon polygon, glm::vec2 low, glm::vec2 high)
{
    for (int axis = 0; axis < 2; ++axis) {
        polygon = clip(polygon, axis, low[axis], false);
        polygon = clip(polygon, axis, high[axis], true);
    }
    // Half-block clipping can repeat a corner already on the cut plane.
    for (std::size_t i = 0; polygon.size() >= 3 && i < polygon.size();) {
        const auto a = polygon[i] - polygon[(i + polygon.size() - 1) % polygon.size()];
        const auto b = polygon[(i + 1) % polygon.size()] - polygon[i];
        if (std::abs(a.x * b.y - a.y * b.x) < .000001f) polygon.erase(polygon.begin() + i);
        else ++i;
    }
    float area = 0;
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
        const auto a = polygon[i] - polygon[0], b = polygon[i + 1] - polygon[0];
        area += a.x * b.y - a.y * b.x;
    }
    if (std::abs(area) < .001f) return {};
    if (area < 0) std::reverse(polygon.begin(), polygon.end());
    return polygon;
}

struct Face {
    glm::vec3 center, normal, u, v;
    glm::vec2 low, high;
};

void vertex(MeshData& mesh, glm::vec3 point, glm::vec3 normal,
            const Face& face, Block material, glm::vec3 color)
{
    mesh.vertices.push_back({point, normal, color, static_cast<float>(material),
        glm::vec4(face.normal, glm::dot(point - face.center, face.normal))});
}

void triangle(MeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c,
              const Face& face, Block material, glm::vec3 color)
{
    if (glm::dot(glm::cross(face.u, face.v), face.normal) < 0) std::swap(b, c);
    const auto crossed = glm::cross(b - a, c - a);
    if (glm::dot(crossed, crossed) < .0000000001f) return;
    const auto normal = glm::normalize(crossed);
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const auto point : {a, b, c}) vertex(mesh, point, normal, face, material, color);
    mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2});
}

void planarFace(MeshData& mesh, std::span<const glm::vec3> points,
                const Face& face, Block material, glm::vec3 color)
{
    const auto crossed = glm::cross(points[1] - points[0], points[2] - points[0]);
    if (glm::dot(crossed, crossed) < .0000000001f) return;
    const bool reverse = glm::dot(glm::cross(face.u, face.v), face.normal) < 0;
    const auto normal = glm::normalize(crossed) * (reverse ? -1.0f : 1.0f);
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const auto point : points) vertex(mesh, point, normal, face, material, color);
    for (std::uint32_t i = 1; i + 1 < points.size(); ++i) {
        if (reverse) mesh.indices.insert(mesh.indices.end(), {first, first + i + 1, first + i});
        else mesh.indices.insert(mesh.indices.end(), {first, first + i, first + i + 1});
    }
}

void bevelSide(MeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
               const Face& face, Block material, glm::vec3 color)
{
    const auto first = glm::cross(b - a, c - a), second = glm::cross(c - a, d - a);
    if (glm::dot(first, first) > .0000000001f && glm::dot(second, second) > .0000000001f
        && glm::dot(glm::normalize(first), glm::normalize(second)) > .99999f) {
        const std::array points{a, b, c, d};
        planarFace(mesh, points, face, material, color);
    } else {
        // A chipped polygon can have a twisted bevel. Keep its two real facet
        // normals rather than smoothing the hard corner to save two vertices.
        triangle(mesh, a, b, c, face, material, color);
        triangle(mesh, a, c, d, face, material, color);
    }
}

// The skirt is buried in the original surface. Only the bevel and cap catch
// the light, without exposing a dark floating box beneath each stone or board.
void bevel(MeshData& mesh, Polygon outline, const Face& face, Block material,
           glm::vec3 color, float height, float width)
{
    outline = clipToSurface(std::move(outline), face.low, face.high);
    if (outline.empty()) return;
    glm::vec2 center(0);
    for (const auto point : outline) center += point;
    center /= static_cast<float>(outline.size());
    const auto uv = glm::vec2(glm::dot(face.center, face.u), glm::dot(face.center, face.v));
    std::vector<glm::vec3> low, cap;
    for (const auto point : outline) {
        const auto delta = center - point;
        const auto inset = delta * std::min(.28f, width / glm::length(delta));
        const auto foot = point - uv;
        const auto top = point + inset - uv;
        low.push_back(face.center + face.u * foot.x + face.v * foot.y - face.normal * .004f);
        cap.push_back(face.center + face.u * top.x + face.v * top.y + face.normal * height);
    }
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const auto next = (i + 1) % outline.size();
        bevelSide(mesh, low[i], low[next], cap[next], cap[i], face, material, color);
    }
    planarFace(mesh, cap, face, material, color);
}

void roofTile(MeshData& mesh, glm::vec2 low, glm::vec2 high, const Face& face)
{
    low = glm::max(low, face.low);
    high = glm::min(high, face.high);
    if (high.x - low.x < .003f || high.y - low.y < .003f) return;
    const auto uv = glm::vec2(glm::dot(face.center, face.u), glm::dot(face.center, face.v));
    const auto position = [&](glm::vec2 point, float depth) {
        point -= uv;
        return face.center + face.u * point.x + face.v * point.y + face.normal * depth;
    };
    const auto inset = glm::min((high - low) * .16f, glm::vec2(.014f));
    const auto a = position(low, -.004f);
    const auto b = position({high.x, low.y}, -.004f);
    const auto c = position(high, -.004f);
    const auto d = position({low.x, high.y}, -.004f);
    const auto e = position(low + inset, .036f);
    const auto f = position({high.x - inset.x, low.y + inset.y}, .036f);
    const auto color = blockColor(Block::RoofTiles);
    // A shingle needs a raised leading lip and two sides. Its rear edge merges
    // into the supporting roof, so there is no hidden back bevel to draw.
    const std::array cap{e, f, c, d}, lip{a, b, f, e};
    planarFace(mesh, cap, face, Block::RoofTiles, color);
    planarFace(mesh, lip, face, Block::RoofTiles, color);
    triangle(mesh, a, e, d, face, Block::RoofTiles, color);
    triangle(mesh, b, c, f, face, Block::RoofTiles, color);
}

void paving(MeshData& mesh, const Face& face, std::uint32_t seed)
{
    const glm::ivec2 first(glm::floor(face.low)), last(glm::floor(face.high));
    for (int y = first.y; y <= last.y; ++y) {
        for (int x = first.x; x <= last.x; ++x) {
            const glm::vec2 origin(x, y);
            const auto tileSeed = tileHash({x, y}, seed);
            const float left = .34f + .19f * variation(tileSeed, 1);
            const float right = .46f + .18f * variation(tileSeed, 2);
            const float split = .38f + .30f * variation(tileSeed, 3);
            const float meeting = glm::mix(left, right, split);
            const std::array<Polygon, 3> stones{{
                {{0, 0}, {1, 0}, {1, right}, {0, left}},
                {{0, left}, {split, meeting}, {split, 1}, {0, 1}},
                {{split, meeting}, {1, right}, {1, 1}, {split, 1}}
            }};
            for (std::size_t i = 0; i < stones.size(); ++i) {
                auto corners = stones[i];
                glm::vec2 middle(0);
                for (const auto p : corners) middle += p * .25f;
                for (auto& p : corners) p += glm::normalize(middle - p) * .006f;
                const float chip = .06f + .06f * variation(tileSeed, 11 + i);
                Polygon rim{
                    glm::mix(corners[0], corners[3], chip), glm::mix(corners[0], corners[1], chip),
                    corners[1], glm::mix(corners[2], corners[1], chip * .7f),
                    glm::mix(corners[2], corners[3], chip * .7f), corners[3]
                };
                for (auto& p : rim) p += origin;
                bevel(mesh, std::move(rim), face, Block::Path, pavingColor(tileSeed, 7 + i),
                      .023f + .019f * variation(tileSeed, 4 + i), .012f);
            }
        }
    }
}

void courses(MeshData& mesh, const Face& face, Block material, std::uint32_t seed)
{
    const bool wood = material == Block::Planks || material == Block::AgedPlanks;
    const bool roof = material == Block::RoofTiles;
    const bool path = material == Block::Path;
    const glm::vec2 size = wood ? glm::vec2(2, .5f) : roof ? glm::vec2(.5f, .4f)
        : path ? glm::vec2(.625f, 1.0f / 3.0f) : glm::vec2(1, .5f);
    const glm::vec2 joint = roof ? glm::vec2(.022f, .026f)
        : path ? glm::vec2(.016f, .013f) : glm::vec2(.025f);
    const int firstRow = static_cast<int>(std::floor(face.low.y / size.y));
    const int lastRow = static_cast<int>(std::floor(face.high.y / size.y));
    for (int row = firstRow; row <= lastRow; ++row) {
        // These are the exact world-space course offsets used by materials.h.
        const int cycle = wood ? 3 : 2;
        const float shift = static_cast<float>((row % cycle + cycle) % cycle) / cycle;
        const int first = static_cast<int>(std::floor(face.low.x / size.x + shift));
        const int last = static_cast<int>(std::floor(face.high.x / size.x + shift));
        for (int column = first; column <= last; ++column) {
            const glm::vec2 low((column - shift) * size.x, row * size.y);
            const auto tileSeed = tileHash({column, row}, seed);
            if (roof) {
                roofTile(mesh, low + joint, low + size - joint, face);
                continue;
            }
            const float height = wood ? .016f + .006f * variation(tileSeed, 1)
                : .022f + .011f * variation(tileSeed, 1);
            const auto color = path ? pavingColor(tileSeed, 2) : blockColor(material);
            bevel(mesh, rectangle(low + joint, low + size - joint), face, material, color,
                  height, wood ? .012f : .020f);
        }
    }
}
} // namespace

bool hasBuiltSurfaceRelief(Block material)
{
    material = stairMaterial(material);
    return material == Block::Brick || material == Block::MossBrick || material == Block::Path
        || material == Block::Planks || material == Block::AgedPlanks || material == Block::RoofTiles;
}

void appendBuiltSurfaceRelief(MeshData& mesh, glm::ivec3 cell,
    const BlockSurface& surface, Block material, std::uint32_t seed)
{
    material = stairMaterial(material);
    if (!hasBuiltSurfaceRelief(material)) return;
    const glm::vec3 normal(surface.normal);
    const auto u = normal.x != 0 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
    const auto v = normal.y != 0 ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const auto minimum = glm::vec3(cell) + surface.minimum;
    const auto maximum = glm::vec3(cell) + surface.maximum;
    const Face face{(minimum + maximum) * .5f, normal, u, v,
        glm::vec2(glm::dot(minimum, u), glm::dot(minimum, v)) + .006f,
        glm::vec2(glm::dot(maximum, u), glm::dot(maximum, v)) - .006f};
    if (face.low.x >= face.high.x || face.low.y >= face.high.y) return;
    if (material == Block::Path && normal.y > 0) paving(mesh, face, seed);
    else courses(mesh, face, material, seed);
}

} // namespace voxel
