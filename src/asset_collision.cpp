#include "asset_collision.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace voxel {
namespace {

constexpr double ContactTolerance = 0.00001;
constexpr double BinSize = 4.0;

bool finite(glm::vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct Bounds {
    glm::dvec3 minimum;
    glm::dvec3 maximum;
};

bool validBounds(glm::vec3 minimum, glm::vec3 maximum)
{
    return finite(minimum) && finite(maximum)
        && glm::all(glm::lessThan(minimum, maximum));
}

bool intersects(const Bounds& a, const Bounds& b)
{
    return glm::all(glm::lessThanEqual(a.minimum, b.maximum))
        && glm::all(glm::greaterThanEqual(a.maximum, b.minimum));
}

struct Cell {
    int x, y, z;
    bool operator==(const Cell&) const = default;
};

struct CellHash {
    std::size_t operator()(Cell cell) const
    {
        // Unsigned arithmetic keeps negative world coordinates well-defined.
        return std::size_t(std::uint32_t(cell.x)) * 73856093u
            ^ std::size_t(std::uint32_t(cell.y)) * 19349663u
            ^ std::size_t(std::uint32_t(cell.z)) * 83492791u;
    }
};

Cell binAt(glm::dvec3 position)
{
    return {int(std::floor(position.x / BinSize)), int(std::floor(position.y / BinSize)),
            int(std::floor(position.z / BinSize))};
}

struct Triangle {
    glm::vec3 a, b, c;
    Bounds bounds;
};

// SAT axes for a triangle against an axis-aligned box: the three box normals,
// the triangle normal, and each triangle edge crossed with each box edge.
template<typename Visit>
bool visitAxes(const Triangle& triangle, Visit visit)
{
    constexpr std::array<glm::dvec3, 3> boxAxes{{{1,0,0}, {0,1,0}, {0,0,1}}};
    const std::array<glm::dvec3, 3> edges{{glm::dvec3(triangle.b) - glm::dvec3(triangle.a),
        glm::dvec3(triangle.c) - glm::dvec3(triangle.b),
        glm::dvec3(triangle.a) - glm::dvec3(triangle.c)}};
    for (const auto axis : boxAxes) if (!visit(axis)) return false;
    if (!visit(glm::cross(edges[0], edges[1]))) return false;
    for (const auto edge : edges)
        for (const auto axis : boxAxes)
            if (!visit(glm::cross(edge, axis))) return false;
    return true;
}

// Project relative to the box center, avoiding precision loss from translated
// rooms. Unnormalized axes are fine; the contact tolerance scales with length.
std::pair<double, double> project(const Triangle& triangle, glm::dvec3 center, glm::dvec3 axis)
{
    const double a = glm::dot(glm::dvec3(triangle.a) - center, axis);
    const double b = glm::dot(glm::dvec3(triangle.b) - center, axis);
    const double c = glm::dot(glm::dvec3(triangle.c) - center, axis);
    return {std::min({a, b, c}), std::max({a, b, c})};
}

bool triangleOverlaps(const Triangle& triangle, const Bounds& box)
{
    const auto center = (box.minimum + box.maximum) * 0.5;
    const auto half = (box.maximum - box.minimum) * 0.5;
    return visitAxes(triangle, [&](glm::dvec3 axis) {
        const double length = glm::length(axis);
        if (length < 1e-12) return true;
        const auto [low, high] = project(triangle, center, axis);
        const double radius = glm::dot(half, glm::abs(axis));
        const double tolerance = ContactTolerance * length;
        return high > -radius + tolerance && low < radius - tolerance;
    });
}

std::optional<double> triangleSweep(const Triangle& triangle, const Bounds& box,
                                     int moveAxis, double distance)
{
    const auto center = (box.minimum + box.maximum) * 0.5;
    const auto half = (box.maximum - box.minimum) * 0.5;
    double enter = -std::numeric_limits<double>::infinity();
    double leave = std::numeric_limits<double>::infinity();
    const bool intersects = visitAxes(triangle, [&](glm::dvec3 axis) {
        const double length = glm::length(axis);
        if (length < 1e-12) return true;
        const auto [low, high] = project(triangle, center, axis);
        const double radius = glm::dot(half, glm::abs(axis));
        const double speed = axis[moveAxis] * distance;
        if (std::abs(speed) < 1e-12 * length) {
            // A resting foot can slide across a flat triangle. Requiring
            // interior overlap on stationary axes also avoids catching edges.
            const double tolerance = ContactTolerance * length;
            return high > -radius + tolerance && low < radius - tolerance;
        }
        double first = (low - radius) / speed;
        double last = (high + radius) / speed;
        if (first > last) std::swap(first, last);
        enter = std::max(enter, first);
        leave = std::min(leave, last);
        return enter <= leave;
    });
    if (!intersects || enter > 1.0 || leave <= ContactTolerance / std::abs(distance)) return {};
    if (enter < 0.0) {
        // Permit escape from tiny floating-point penetration instead of
        // trapping a resting player. Actual new contacts still stop at t=0.
        const auto normal = glm::cross(glm::dvec3(triangle.b) - glm::dvec3(triangle.a),
                                       glm::dvec3(triangle.c) - glm::dvec3(triangle.a));
        const double side = glm::dot(center - glm::dvec3(triangle.a), normal);
        if (side * normal[moveAxis] * distance >= 0.0) return {};
    }
    return std::max(0.0, enter);
}

std::optional<double> triangleRay(const Triangle& triangle, glm::dvec3 origin,
                                  glm::dvec3 direction, double maximum)
{
    const auto edge1 = glm::dvec3(triangle.b) - glm::dvec3(triangle.a);
    const auto edge2 = glm::dvec3(triangle.c) - glm::dvec3(triangle.a);
    const auto across = glm::cross(direction, edge2);
    const double determinant = glm::dot(edge1, across);
    if (std::abs(determinant) <= 1e-12 * glm::length(edge1) * glm::length(edge2)) return {};
    const auto relative = origin - glm::dvec3(triangle.a);
    const double u = glm::dot(relative, across) / determinant;
    const auto crossed = glm::cross(relative, edge1);
    const double v = glm::dot(direction, crossed) / determinant;
    if (u < -1e-9 || v < -1e-9 || u + v > 1.0 + 1e-9) return {};
    const double distance = glm::dot(edge2, crossed) / determinant;
    if (distance < -ContactTolerance || distance > maximum) return {};
    return std::max(0.0, distance);
}

double solidAngle(const Triangle& triangle, glm::dvec3 point)
{
    const auto a = glm::dvec3(triangle.a) - point;
    const auto b = glm::dvec3(triangle.b) - point;
    const auto c = glm::dvec3(triangle.c) - point;
    const double la = glm::length(a), lb = glm::length(b), lc = glm::length(c);
    return 2.0 * std::atan2(glm::dot(a, glm::cross(b, c)),
        la * lb * lc + glm::dot(a, b) * lc + glm::dot(b, c) * la + glm::dot(c, a) * lb);
}

} // namespace

struct AssetCollision::Data {
    struct Model {
        Bounds bounds;
        std::uint32_t first, end;
    };
    std::vector<Triangle> triangles;
    std::vector<Model> models;
    std::unordered_map<Cell, std::vector<std::uint32_t>, CellHash> bins;
    std::unordered_map<Cell, std::vector<std::uint32_t>, CellHash> modelBins;
    Bounds bounds{glm::dvec3(std::numeric_limits<double>::infinity()),
                  glm::dvec3(-std::numeric_limits<double>::infinity())};

    std::vector<std::uint32_t> candidates(Bounds query) const
    {
        std::vector<std::uint32_t> result;
        if (!intersects(query, bounds)) return result;
        query.minimum = glm::max(query.minimum, bounds.minimum);
        query.maximum = glm::min(query.maximum, bounds.maximum);
        const auto first = binAt(query.minimum), last = binAt(query.maximum);
        const double cells = (double(last.x) - first.x + 1) * (double(last.y) - first.y + 1)
            * (double(last.z) - first.z + 1);
        // Very long rays/sweeps inspect the finite geometry instead of walking
        // billions of empty grid cells. Ordinary player queries visit few bins.
        if (cells > double(bins.size()) * 2.0 + 64.0) {
            for (std::uint32_t i = 0; i < triangles.size(); ++i)
                if (intersects(query, triangles[i].bounds)) result.push_back(i);
            return result;
        }
        for (int y = first.y; y <= last.y; ++y)
            for (int z = first.z; z <= last.z; ++z)
                for (int x = first.x; x <= last.x; ++x) {
                    const auto found = bins.find({x,y,z});
                    if (found != bins.end()) result.insert(result.end(), found->second.begin(), found->second.end());
                }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        std::erase_if(result, [&](std::uint32_t i) { return !intersects(query, triangles[i].bounds); });
        return result;
    }
};

void AssetCollision::rebuild(std::span<const HubAsset> assets)
{
    auto data = std::make_shared<Data>();
    for (const auto& asset : assets) {
        // Leave ample room for mesh extents and safe conversion to grid cells.
        if (static_cast<std::size_t>(asset.kind) >= HubAssetCount || !finite(asset.position)
            || glm::any(glm::greaterThan(glm::abs(asset.position), glm::vec3(10000000.0f)))) continue;
        const auto mesh = buildHubAsset(asset);
        Data::Model model{{glm::dvec3(std::numeric_limits<double>::infinity()),
                           glm::dvec3(-std::numeric_limits<double>::infinity())},
                           static_cast<std::uint32_t>(data->triangles.size()), 0};
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const auto a = mesh.vertices[mesh.indices[i]].position;
            const auto b = mesh.vertices[mesh.indices[i+1]].position;
            const auto c = mesh.vertices[mesh.indices[i+2]].position;
            const auto normal = glm::cross(glm::dvec3(b) - glm::dvec3(a), glm::dvec3(c) - glm::dvec3(a));
            if (!finite(a) || !finite(b) || !finite(c) || glm::dot(normal, normal) < 1e-20) continue;
            Triangle triangle{a,b,c,{glm::dvec3(glm::min(a,glm::min(b,c))),
                                     glm::dvec3(glm::max(a,glm::max(b,c)))}};
            const auto index = static_cast<std::uint32_t>(data->triangles.size());
            data->triangles.push_back(triangle);
            model.bounds.minimum = glm::min(model.bounds.minimum, triangle.bounds.minimum);
            model.bounds.maximum = glm::max(model.bounds.maximum, triangle.bounds.maximum);
            const auto first = binAt(triangle.bounds.minimum), last = binAt(triangle.bounds.maximum);
            for (int y = first.y; y <= last.y; ++y)
                for (int z = first.z; z <= last.z; ++z)
                    for (int x = first.x; x <= last.x; ++x) data->bins[{x,y,z}].push_back(index);
        }
        model.end = static_cast<std::uint32_t>(data->triangles.size());
        if (model.first == model.end) continue;
        const auto modelIndex = static_cast<std::uint32_t>(data->models.size());
        data->models.push_back(model);
        const auto first = binAt(model.bounds.minimum), last = binAt(model.bounds.maximum);
        for (int y = first.y; y <= last.y; ++y)
            for (int z = first.z; z <= last.z; ++z)
                for (int x = first.x; x <= last.x; ++x) data->modelBins[{x,y,z}].push_back(modelIndex);
        data->bounds.minimum = glm::min(data->bounds.minimum, model.bounds.minimum);
        data->bounds.maximum = glm::max(data->bounds.maximum, model.bounds.maximum);
    }
    data_ = data->triangles.empty() ? nullptr : std::move(data);
}

void AssetCollision::clear() { data_.reset(); }
bool AssetCollision::empty() const { return !data_; }

bool AssetCollision::overlaps(glm::vec3 minimum, glm::vec3 maximum) const
{
    if (!validBounds(minimum, maximum)) return true;
    if (!data_) return false;
    const Bounds box{minimum, maximum};
    for (const auto index : data_->candidates(box))
        if (triangleOverlaps(data_->triangles[index], box)) return true;

    // A teleport can place a small body entirely inside a solid without
    // crossing its skin. A winding-number check catches that case without
    // filling hollow logs, arch openings, or the space beneath mushroom caps.
    // Double-sided decorative sheets cancel instead of inventing a volume.
    const auto center = (box.minimum + box.maximum) * 0.5;
    if (!intersects({center,center}, data_->bounds)) return false;
    const auto containingBin = data_->modelBins.find(binAt(center));
    if (containingBin == data_->modelBins.end()) return false;
    for (const auto modelIndex : containingBin->second) {
        const auto& model = data_->models[modelIndex];
        if (!glm::all(glm::greaterThan(box.minimum, model.bounds.minimum))
            || !glm::all(glm::lessThan(box.maximum, model.bounds.maximum))) continue;
        double angle = 0.0;
        for (auto i = model.first; i < model.end; ++i) angle += solidAngle(data_->triangles[i], center);
        if (std::abs(angle) > 6.283185307179586) return true;
    }
    return false;
}

float AssetCollision::sweep(glm::vec3 minimum, glm::vec3 maximum, int axis, float distance) const
{
    if (!validBounds(minimum, maximum) || axis < 0 || axis > 2 || !std::isfinite(distance)) return 0.0f;
    if (!data_ || distance == 0.0f) return distance;
    const Bounds box{minimum, maximum};
    auto traveled = box;
    if (distance < 0.0f) traveled.minimum[axis] += double(distance);
    else traveled.maximum[axis] += double(distance);
    double fraction = 1.0;
    for (const auto index : data_->candidates(traveled)) {
        const auto contact = triangleSweep(data_->triangles[index], box, axis, distance);
        if (contact) fraction = std::min(fraction, *contact);
    }
    return static_cast<float>(double(distance) * fraction);
}

std::optional<AssetSurfaceHit> AssetCollision::raycast(glm::vec3 origin, glm::vec3 direction,
                                                       float maxDistance) const
{
    if (!data_ || !finite(origin) || !finite(direction) || !std::isfinite(maxDistance)
        || maxDistance < 0.0f) return {};
    const double length = glm::length(glm::dvec3(direction));
    if (length < 1e-12) return {};
    const auto ray = glm::dvec3(direction) / length;
    const auto start = glm::dvec3(origin), end = start + ray * double(maxDistance);
    const Bounds query{glm::min(start,end), glm::max(start,end)};
    std::optional<AssetSurfaceHit> result;
    double closest = maxDistance;
    for (const auto index : data_->candidates(query)) {
        const auto& triangle = data_->triangles[index];
        const auto hit = triangleRay(triangle, start, ray, closest);
        if (!hit) continue;
        closest = *hit;
        auto normal = glm::normalize(glm::cross(glm::dvec3(triangle.b) - glm::dvec3(triangle.a),
                                                glm::dvec3(triangle.c) - glm::dvec3(triangle.a)));
        if (glm::dot(normal, ray) > 0.0) normal = -normal;
        result = AssetSurfaceHit{glm::vec3(start + ray * closest), glm::vec3(normal), float(closest)};
    }
    return result;
}

} // namespace voxel
