#include "graphics/detail_lod.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace voxel {
namespace {
std::uint32_t cellHash(glm::ivec3 cell)
{
    auto hash = static_cast<std::uint32_t>(cell.x) * 0x9e3779b9u
              ^ static_cast<std::uint32_t>(cell.y) * 0x85ebca6bu
              ^ static_cast<std::uint32_t>(cell.z) * 0xc2b2ae35u;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    return hash ^ (hash >> 16);
}

std::size_t triangleBand(const MeshData& mesh, std::size_t first, DetailLodKind kind)
{
    glm::vec3 center(0);
    for (std::size_t corner = 0; corner < 3; ++corner) {
        const auto& vertex = mesh.vertices.at(mesh.indices[first + corner]);
        auto anchor = vertex.position;
        if (kind == DetailLodKind::Relief)
            anchor -= glm::vec3(vertex.relief) * (vertex.relief.w + .001f);
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(anchor[axis]) || anchor[axis] < -1000000.f || anchor[axis] > 1000000.f)
                throw std::invalid_argument("detail mesh contains an invalid position");
        center += anchor / 3.f;
    }
    const auto hash = cellHash(glm::ivec3(glm::floor(center)));
    // Half the cosmetic cells fade first, another quarter in the middle,
    // and the final quarter keeps a sparse distant texture silhouette.
    return (hash & 3u) < 2u ? 0 : (hash & 3u) == 2u ? 1 : 2;
}
} // namespace

DetailLodMesh partitionDetailLod(MeshData mesh, DetailLodKind kind)
{
    if (mesh.indices.size() % 3 != 0)
        throw std::invalid_argument("detail mesh indices must contain complete triangles");
    std::vector<unsigned char> bands;
    bands.reserve(mesh.indices.size() / 3);
    std::array<std::size_t, DetailLodBandCount> counts{};
    for (std::size_t first = 0; first < mesh.indices.size(); first += 3) {
        const auto band = triangleBand(mesh, first, kind);
        bands.push_back(static_cast<unsigned char>(band));
        counts[band] += 3;
    }
    DetailLodMesh result;
    result.mesh.vertices = std::move(mesh.vertices);
    result.mesh.indices.resize(mesh.indices.size());
    std::array<std::size_t, DetailLodBandCount> write{};
    std::size_t offset = 0;
    for (std::size_t band = 0; band < DetailLodBandCount; ++band) {
        result.ranges[band] = {offset, counts[band]};
        write[band] = offset;
        offset += counts[band];
    }
    for (std::size_t triangle = 0; triangle < bands.size(); ++triangle) {
        auto& destination = write[bands[triangle]];
        for (std::size_t corner = 0; corner < 3; ++corner)
            result.mesh.indices[destination++] = mesh.indices[triangle * 3 + corner];
    }
    return result;
}

DetailLodFade detailLodFade(DetailLodKind kind, std::size_t band)
{
    constexpr std::array<DetailLodFade, DetailLodBandCount> relief{{{24,36},{40,56},{64,84}}};
    constexpr std::array<DetailLodFade, DetailLodBandCount> cave{{{34,48},{52,72},{76,100}}};
    return (kind == DetailLodKind::Relief ? relief : cave).at(band);
}

float detailLodCoverage(float distance, DetailLodFade fade)
{
    if (fade.end <= fade.start) return distance < fade.end ? 1.f : 0.f;
    const float t = std::clamp((distance - fade.start) / (fade.end - fade.start), 0.f, 1.f);
    return 1.f - t * t * (3.f - 2.f * t);
}

bool detailLodVisible(glm::vec3 camera, glm::vec3 minimum, glm::vec3 maximum,
                      DetailLodFade fade)
{
    const auto delta = camera - glm::clamp(camera, minimum, maximum);
    return glm::dot(delta, delta) < fade.end * fade.end;
}

} // namespace voxel
