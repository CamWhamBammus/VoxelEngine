#include "graphics/light_grid.h"
#include "hub_assets.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
using namespace voxel;
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

template<class Exception, class Call>
void throws(Call call, const char* message)
{
    try { call(); check(false, message); }
    catch (const Exception&) {}
    catch (...) { check(false, message); }
}

std::vector<std::uint32_t> at(const LightGrid& grid, glm::vec3 position)
{
    const auto range = grid.query(position);
    if (!range) return {};
    return {grid.indices.begin() + range->offset,
            grid.indices.begin() + range->offset + range->count};
}

double distanceToBoxSquared(glm::vec3 point, glm::dvec3 low, glm::dvec3 high)
{
    double squared = 0;
    for (int axis = 0; axis < 3; ++axis) {
        double distance = 0;
        if (point[axis] < low[axis]) distance = low[axis] - point[axis];
        else if (point[axis] > high[axis]) distance = point[axis] - high[axis];
        squared += distance * distance;
    }
    return squared;
}

void compareEveryCell(std::span<const PointLight> lights, glm::ivec3 minimum, glm::ivec3 maximum)
{
    const auto grid = buildLightGrid(lights,minimum,maximum);
    std::uint32_t expectedOffset = 0;
    for (int z = 0; z < grid.dimensions.z; ++z)
        for (int y = 0; y < grid.dimensions.y; ++y)
            for (int x = 0; x < grid.dimensions.x; ++x) {
                const auto index = x + grid.dimensions.x * (y + grid.dimensions.y * z);
                const auto low = glm::dvec3(minimum) + glm::dvec3(x,y,z) * 4.0;
                glm::dvec3 high;
                for (int axis = 0; axis < 3; ++axis) high[axis] = std::min(low[axis] + 4.0,double(maximum[axis]));
                const auto range = grid.ranges[index];
                check(range.offset == expectedOffset, "Ranges use contiguous OpenGL X/Y/Z texture order");
                expectedOffset += range.count;
                const auto actual = at(grid,glm::vec3((low + high) * .5));
                check(std::is_sorted(actual.begin(),actual.end()), "Cell sources retain input order");
                check(std::adjacent_find(actual.begin(),actual.end()) == actual.end(), "A cell contains each source only once");
                for (std::uint32_t source = 0; source < lights.size(); ++source) {
                    const auto& light = lights[source];
                    const double distance = distanceToBoxSquared(light.position,low,high);
                    const double radiusSquared = double(light.radius) * light.radius;
                    const bool present = std::binary_search(actual.begin(),actual.end(),source);
                    if (distance <= radiusSquared)
                        check(present, "Every sphere intersecting a cell survives the spatial index");
                    else if (distance > radiusSquared * 1.000003)
                        check(!present, "Disjoint spheres are excluded outside the float-rounding shell");
                }
            }
    check(expectedOffset == grid.indices.size(), "Packed ranges consume exactly the index buffer");
    const auto repeated = buildLightGrid(lights,minimum,maximum);
    check(repeated.ranges == grid.ranges && repeated.indices == grid.indices,
          "Identical lights always produce identical candidate buffers");
}

void boundaries()
{
    const std::array<PointLight,4> lights{{
        {{0,0,0},{1,1,1},1},
        {{-9,0,0},{1,0,0},1},
        {{9,0,0},{0,1,0},1},
        {{4,4,4},{0,0,1},.25f}
    }};
    compareEveryCell(lights,{-8,-8,-8},{8,8,8});
    const auto grid = buildLightGrid(lights,{-8,-8,-8},{8,8,8});
    for (const float x : {-0.001f,0.f,.001f})
        for (const float y : {-0.001f,0.f,.001f})
            for (const float z : {-0.001f,0.f,.001f}) {
                const auto sources = at(grid,{x,y,z});
                check(std::find(sources.begin(),sources.end(),0) != sources.end(),
                      "An influencing source survives negative faces, edges and corners");
            }
    check(at(grid,{-8,0,0}) == std::vector<std::uint32_t>{1},
          "A source outside the lower bound can illuminate the edge cell");
    check(at(grid,{std::nextafter(8.f,0.f),0,0}) == std::vector<std::uint32_t>({2,3}),
          "A source beyond the upper bound can illuminate the last cell");
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 point(0);
        point[axis] = -8;
        check(grid.query(point).has_value(), "Minimum grid faces are included");
        point[axis] = std::nextafter(-8.f,-9.f);
        check(!grid.query(point), "Outside lower faces request the global light fallback");
        point[axis] = 8;
        check(!grid.query(point), "Maximum grid faces are excluded without clamping");
        point[axis] = std::numeric_limits<float>::quiet_NaN();
        check(!grid.query(point), "Nonfinite query coordinates cannot index a texture");
    }
    // The final cell is clipped to the actual bounds, not padded to four metres.
    const std::array<PointLight,1> beyond{{{{11,0,0},{1,1,1},.25f}}};
    const auto clipped = buildLightGrid(beyond,{-3,-2,-1},{9,3,2});
    check(clipped.indices.empty(), "Sources solely in the last cell's padding are excluded");
    check(!clipped.query({9,0,0}), "Non-aligned maximum remains exclusive");
}

void manyLights()
{
    std::vector<PointLight> lights(700,{{0,0,0},{1,1,1},100});
    const auto grid = buildLightGrid(lights,{-8,-8,-8},{8,8,8});
    for (const auto& range : grid.ranges) {
        check(range.count == lights.size(), "Dense light clusters never hit an arbitrary source cap");
        for (std::uint32_t i = 0; i < range.count; ++i)
            check(grid.indices[range.offset+i] == i, "Large lists keep exact global light indices");
    }
    std::mt19937 random(47293);
    std::uniform_real_distribution<float> position(-24,24), radius(.05f,17);
    lights.clear();
    for (int i = 0; i < 250; ++i)
        lights.push_back({{position(random),position(random),position(random)},{1,.5f,.2f},radius(random)});
    compareEveryCell(lights,{-9,-7,-13},{10,8,11});
    const auto worldGrid = buildLightGrid(lights);
    check(worldGrid.minimum == World::Min - int(HubAssetMargin)
          && worldGrid.maximum == World::MaxExclusive + int(HubAssetMargin),
          "Default grid covers decorative assets beyond every world face");
    check(worldGrid.query(glm::vec3(World::Min)-glm::vec3(3.9f)).has_value()
          && worldGrid.query(glm::vec3(World::MaxExclusive)+glm::vec3(3.9f)).has_value(),
          "Asset margin points have real cells instead of edge clamping");
}

void emptyAndInvalid()
{
    const auto empty = buildLightGrid({},{-4,-4,-4},{4,4,4});
    check(empty.indices.empty() && empty.ranges.size() == 8, "No lights still gives a complete empty range texture");
    check(empty.query({0,0,0}) == LightRange{}, "A valid empty cell differs from an outside query");
    check(!LightGrid{}.query({0,0,0}), "An unbuilt grid safely requests fallback");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    PointLight source{{0,0,0},{1,1,1},1};
    const auto reject = [&](PointLight light) {
        throws<std::invalid_argument>([&] { buildLightGrid(std::span(&light,1),{-4,-4,-4},{4,4,4}); },
                                     "Invalid source values are rejected before packing");
    };
    for (float radius : {0.f,-1.f,nan,inf}) { auto light = source; light.radius = radius; reject(light); }
    for (int axis = 0; axis < 3; ++axis) {
        for (float value : {nan,inf,-inf}) { auto light = source; light.position[axis] = value; reject(light); }
        for (float value : {-1.f,nan,inf}) { auto light = source; light.color[axis] = value; reject(light); }
        glm::ivec3 minimum(0), maximum(4);
        maximum[axis] = 0;
        throws<std::invalid_argument>([&] { buildLightGrid({},minimum,maximum); }, "Empty bounds are rejected");
        maximum[axis] = -1;
        throws<std::invalid_argument>([&] { buildLightGrid({},minimum,maximum); }, "Inverted bounds are rejected");
    }
    throws<std::length_error>([] {
        buildLightGrid({},glm::ivec3(std::numeric_limits<int>::lowest()),glm::ivec3(std::numeric_limits<int>::max()));
    }, "Extreme integer extents fail before allocation or overflow");
    // Finite positions/radii can exceed float squared-distance range.
    const std::array<PointLight,2> extremes{{
        {{1e30f,0,0},{0,0,0},1}, {{0,0,0},{1,1,1},1e30f}
    }};
    const auto grid = buildLightGrid(extremes,{-4,-4,-4},{4,4,4});
    check(at(grid,{0,0,0}) == std::vector<std::uint32_t>{1},
          "Huge finite source data uses safe distances instead of overflow");
}

} // namespace

int main()
{
    boundaries();
    manyLights();
    emptyAndInvalid();
    if (failures) return 1;
    std::cout << "Light grid: exact candidates, stable ordering, boundaries, density and invalid inputs passed\n";
}
