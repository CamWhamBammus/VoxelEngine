#include "graphics/light_grid.h"
#include "hub_assets.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace voxel {
namespace {

constexpr std::size_t MaximumCells = 4 * 1024 * 1024;

std::size_t cellIndex(glm::ivec3 cell, glm::ivec3 dimensions)
{
    return std::size_t(cell.x) + std::size_t(dimensions.x)
        * (std::size_t(cell.y) + std::size_t(dimensions.y) * std::size_t(cell.z));
}

template<class Visit>
void visitCells(const LightGrid& grid, const PointLight& light, Visit visit)
{
    // GLSL evaluates source distances in floats. Retain a tiny conservative
    // shell so rounding at a cell seam cannot reject an in-range shader sample.
    const double radius = double(light.radius) * (1.0 + 8.0 * std::numeric_limits<float>::epsilon());
    glm::ivec3 first{}, last{};
    for (int axis = 0; axis < 3; ++axis) {
        const double low = double(light.position[axis]) - radius;
        const double high = double(light.position[axis]) + radius;
        if (high < grid.minimum[axis] || low > grid.maximum[axis]) return;
        const double clippedLow = std::max(low, double(grid.minimum[axis]));
        const double clippedHigh = std::min(high, double(grid.maximum[axis]));
        // Include the preceding cell at exact tangency. The sphere/AABB test
        // below removes the extra cell when the lower bound is not on an edge.
        first[axis] = std::max(0, int(std::floor((clippedLow - grid.minimum[axis]) / LightGrid::CellSize)) - 1);
        last[axis] = std::min(grid.dimensions[axis] - 1,
            int(std::floor((clippedHigh - grid.minimum[axis]) / LightGrid::CellSize)));
    }
    const double radiusSquared = radius * radius;
    for (int z = first.z; z <= last.z; ++z)
        for (int y = first.y; y <= last.y; ++y)
            for (int x = first.x; x <= last.x; ++x) {
                const glm::ivec3 cell{x,y,z};
                double distanceSquared = 0;
                for (int axis = 0; axis < 3; ++axis) {
                    const double low = double(grid.minimum[axis]) + double(cell[axis]) * LightGrid::CellSize;
                    const double high = std::min(low + LightGrid::CellSize, double(grid.maximum[axis]));
                    const double delta = light.position[axis] - std::clamp(double(light.position[axis]), low, high);
                    distanceSquared += delta * delta;
                }
                if (distanceSquared <= radiusSquared) visit(cellIndex(cell, grid.dimensions));
            }
}

} // namespace

std::optional<LightRange> LightGrid::query(glm::vec3 position) const
{
    glm::ivec3 cell{};
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(position[axis]) || double(position[axis]) < minimum[axis]
            || double(position[axis]) >= maximum[axis] || dimensions[axis] <= 0) return std::nullopt;
        cell[axis] = int(std::floor((double(position[axis]) - minimum[axis]) / CellSize));
    }
    return ranges.at(cellIndex(cell, dimensions));
}

LightGrid buildLightGrid(std::span<const PointLight> lights,
                        glm::ivec3 minimum, glm::ivec3 maximum)
{
    LightGrid grid;
    grid.minimum = minimum;
    grid.maximum = maximum;
    std::size_t cellCount = 1;
    for (int axis = 0; axis < 3; ++axis) {
        const auto extent = std::int64_t(maximum[axis]) - minimum[axis];
        if (extent <= 0) throw std::invalid_argument("Light grid bounds must have positive extents");
        const auto count = (extent + LightGrid::CellSize - 1) / LightGrid::CellSize;
        if (count > std::numeric_limits<int>::max() || std::size_t(count) > MaximumCells / cellCount)
            throw std::length_error("Light grid dimensions are too large");
        grid.dimensions[axis] = int(count);
        cellCount *= std::size_t(count);
    }
    if (lights.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Light grid has too many sources");
    for (const auto& light : lights) {
        if (!std::isfinite(light.radius) || light.radius <= 0)
            throw std::invalid_argument("Light radius must be finite and positive");
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(light.position[axis]) || !std::isfinite(light.color[axis]) || light.color[axis] < 0)
                throw std::invalid_argument("Light position and color must be finite; color must be nonnegative");
    }
    grid.ranges.resize(cellCount);
    for (const auto& light : lights)
        visitCells(grid, light, [&](std::size_t cell) { ++grid.ranges[cell].count; });

    std::uint64_t total = 0;
    for (auto& range : grid.ranges) {
        range.offset = static_cast<std::uint32_t>(total);
        total += range.count;
        if (total > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Light grid index offsets exceed 32 bits");
    }
    grid.indices.resize(static_cast<std::size_t>(total));
    std::vector<std::uint32_t> cursors(cellCount);
    for (std::size_t cell = 0; cell < cellCount; ++cell) cursors[cell] = grid.ranges[cell].offset;
    for (std::size_t source = 0; source < lights.size(); ++source)
        visitCells(grid, lights[source], [&](std::size_t cell) {
            grid.indices[cursors[cell]++] = static_cast<std::uint32_t>(source);
        });
    return grid;
}

LightGrid buildLightGrid(std::span<const PointLight> lights)
{
    constexpr int margin = static_cast<int>(HubAssetMargin);
    static_assert(float(margin) == HubAssetMargin);
    return buildLightGrid(lights, World::Min - margin, World::MaxExclusive + margin);
}

static_assert(sizeof(LightRange) == 2 * sizeof(std::uint32_t));

} // namespace voxel
