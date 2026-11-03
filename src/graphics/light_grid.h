#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace voxel {

struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float radius;
};

// Matches one RG32UI texel. Indices address the original PointLight array.
struct LightRange {
    std::uint32_t offset = 0;
    std::uint32_t count = 0;
    bool operator==(const LightRange&) const = default;
};

struct LightGrid {
    static constexpr int CellSize = 4;
    glm::ivec3 minimum{}, maximum{}, dimensions{};
    // OpenGL 3D texture order: X fastest, then Y, then Z.
    std::vector<LightRange> ranges;
    std::vector<std::uint32_t> indices;

    // A missing range means the point is outside this grid, not unlit. The
    // renderer must use all sources there; clamping to an edge could lose light.
    std::optional<LightRange> query(glm::vec3 position) const;
};

// Every sphere touching a cell is retained, in original source order. There is
// no light cap or camera dependency. Invalid bounds/sources throw invalid_argument;
// dimensions or packed offsets beyond their supported size throw length_error.
LightGrid buildLightGrid(std::span<const PointLight> lights,
                        glm::ivec3 minimum, glm::ivec3 maximum);
// Covers the world and the full margin of decorative assets outside its edges.
LightGrid buildLightGrid(std::span<const PointLight> lights);

} // namespace voxel
