#include "asset_builder.h"

#include <array>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

constexpr glm::vec3 Joint{0.16f, 0.18f, 0.17f};
using Quad = std::array<glm::vec2, 4>;

std::uint32_t hash(std::uint32_t seed, std::uint32_t part)
{
    auto value = seed ^ (part * 0x9e3779b9u);
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

float variation(std::uint32_t seed, std::uint32_t part)
{
    return float(hash(seed, part) & 0xffffu) / 65535.0f;
}

glm::vec3 stoneColor(std::uint32_t seed, std::uint32_t part)
{
    constexpr std::array<glm::vec3, 5> palette{{
        {0.57f,0.55f,0.47f}, {0.65f,0.60f,0.49f}, {0.51f,0.55f,0.52f},
        {0.69f,0.65f,0.55f}, {0.57f,0.53f,0.46f}
    }};
    return palette[hash(seed, part) % palette.size()] * (0.94f + 0.06f * variation(seed, part + 13));
}

// Quad points run clockwise in the tile plane. Pulling them slightly inward
// leaves a fitted joint; clipping two corners gives each slab a worn outline.
void slab(AssetBuilder& out, Quad corners, float bottom, float top,
          glm::vec3 color, float chip, bool upright = false)
{
    glm::vec2 center{};
    for (const auto p : corners) center += p * 0.25f;
    for (auto& p : corners) p += glm::normalize(center - p) * 0.006f;
    const std::array<glm::vec2, 6> rim{{
        glm::mix(corners[0], corners[3], chip), glm::mix(corners[0], corners[1], chip),
        corners[1], glm::mix(corners[2], corners[1], chip * 0.7f),
        glm::mix(corners[2], corners[3], chip * 0.7f), corners[3]
    }};
    const auto point = [upright](glm::vec2 p, float height) {
        // On a riser the second coordinate points down, preserving +Z winding.
        return upright ? glm::vec3(p.x, -p.y, height) : glm::vec3(p.x, height, p.y);
    };
    std::array<glm::vec3, 6> low, high;
    for (std::size_t i = 0; i < rim.size(); ++i) {
        low[i] = point(rim[i], bottom);
        high[i] = point(rim[i] + glm::normalize(center - rim[i]) * 0.012f, top);
    }
    for (std::size_t i = 0; i < rim.size(); ++i) {
        const auto next = (i + 1) % rim.size();
        out.quad(low[i], low[next], high[next], high[i], Block::Path,
                 color * (0.78f + 0.045f * float(i % 3)));
    }
    for (std::size_t i = 1; i + 1 < high.size(); ++i)
        out.triangle(high[0], high[i], high[i + 1], Block::Path,
                     color * (i % 2 == 0 ? 0.98f : 1.0f));
}

Quad rectangle(float left, float near, float right, float far)
{
    return {{{left,near}, {left,far}, {right,far}, {right,near}}};
}

void paving(AssetBuilder& out, std::uint32_t seed)
{
    out.quad({-0.5f,0.003f,-0.5f}, {-0.5f,0.003f,0.5f},
             {0.5f,0.003f,0.5f}, {0.5f,0.003f,-0.5f}, Block::Path, Joint);
    const float left = -0.16f + 0.19f * variation(seed, 1);
    const float right = -0.04f + 0.18f * variation(seed, 2);
    const float split = -0.12f + 0.30f * variation(seed, 3);
    const float meeting = glm::mix(left, right, split + 0.5f);
    const std::array<Quad, 3> stones{{
        {{{-0.5f,-0.5f}, {-0.5f,left}, {0.5f,right}, {0.5f,-0.5f}}},
        {{{-0.5f,left}, {-0.5f,0.5f}, {split,0.5f}, {split,meeting}}},
        {{{split,meeting}, {split,0.5f}, {0.5f,0.5f}, {0.5f,right}}}
    }};
    for (std::size_t i = 0; i < stones.size(); ++i)
        slab(out, stones[i], 0.006f, 0.023f + 0.019f * variation(seed, 4 + i),
             stoneColor(seed, 7 + i), 0.06f + 0.06f * variation(seed, 11 + i));
}

void edging(AssetBuilder& out, std::uint32_t seed)
{
    out.quad({-0.5f,0.005f,0.34f}, {-0.5f,0.005f,0.5f},
             {0.5f,0.005f,0.5f}, {0.5f,0.005f,0.34f}, Block::Path, Joint);
    const float split = -0.08f + 0.16f * variation(seed, 1);
    slab(out, rectangle(-0.5f,0.34f,split,0.5f), 0.009f,
         0.065f + 0.018f * variation(seed, 2), stoneColor(seed, 3), 0.08f);
    slab(out, rectangle(split,0.34f,0.5f,0.5f), 0.009f,
         0.065f + 0.018f * variation(seed, 4), stoneColor(seed, 5), 0.11f);
}

void stair(AssetBuilder& out, std::uint32_t seed)
{
    out.quad({-0.5f,-0.98f,0.501f}, {0.5f,-0.98f,0.501f},
             {0.5f,0.01f,0.501f}, {-0.5f,0.01f,0.501f}, Block::Path, Joint);
    const float first = 0.30f + 0.055f * variation(seed, 1);
    const float second = 0.62f + 0.055f * variation(seed, 2);
    const std::array<float, 4> courses{{0.025f, first, second, 0.98f}};
    for (std::size_t i = 0; i + 1 < courses.size(); ++i)
        slab(out, rectangle(-0.5f,courses[i],0.5f,courses[i + 1]), 0.503f,
             0.513f + 0.005f * variation(seed, 3 + i), stoneColor(seed, 7 + i), 0.045f, true);
    // A thin overhanging lip catches the light without changing the stair's
    // one-block collision height or projecting into the next tread.
    slab(out, rectangle(-0.5f,-0.035f,0.5f,0.025f), 0.503f, 0.52f,
         stoneColor(seed, 12) * 1.04f, 0.035f, true);
}

} // namespace

void appendHubPath(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh, asset);
    switch (asset.kind) {
    case HubAssetKind::PathPaving: paving(out, asset.variation); break;
    case HubAssetKind::PathEdging: edging(out, asset.variation); break;
    case HubAssetKind::PathStep: stair(out, asset.variation); break;
    default: break;
    }
}

} // namespace voxel
