#include "graphics/asset_builder.h"

#include <array>
#include <cmath>

namespace voxel {
namespace {

constexpr float Pi = 3.14159265359f;
constexpr glm::vec3 Stem{0.19f, 0.35f, 0.13f};
constexpr glm::vec3 Leaf{0.24f, 0.55f, 0.25f};

float variation(std::uint32_t seed, std::uint32_t part)
{
    auto value = seed ^ (part * 0x9e3779b9u + 0x85ebca6bu);
    value = (value ^ (value >> 16)) * 0x7feb352du;
    value = (value ^ (value >> 15)) * 0x846ca68bu;
    return static_cast<float>((value ^ (value >> 16)) & 0x00ffffffu) / 16777216.0f;
}

glm::vec3 radial(float angle)
{
    return {std::cos(angle), 0.0f, std::sin(angle)};
}

// Four broad facets give each leaf a folded midrib, with a painted light side.
// Both sides are real triangles so leaves remain visible from below the canopy.
void leaf(AssetBuilder& b, glm::vec3 base, glm::vec3 tip, float width,
          glm::vec3 color, Block material = Block::Leaves)
{
    const auto direction = tip - base;
    const auto along = glm::normalize(direction);
    const auto across = glm::normalize(glm::cross(along,
        std::abs(along.y) > 0.9f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0)));
    const auto normal = glm::normalize(glm::cross(across, along));
    const auto ridge = base + direction * 0.48f + normal * width * 0.18f;
    const auto left = base + direction * 0.38f + across * width;
    const auto right = base + direction * 0.53f - across * width * 0.86f;
    b.triangle(base, left, ridge, material, color, true);
    b.triangle(left, tip, ridge, material, color, true);
    b.triangle(tip, right, ridge, material, color * 0.82f, true);
    b.triangle(right, base, ridge, material, color * 0.82f, true);
}

// Small irregular leaf masses and flower hearts use the same six-sided shape.
void clump(AssetBuilder& b, glm::vec3 center, glm::vec3 radius,
           glm::vec3 color, std::uint32_t seed, Block material = Block::Leaves)
{
    std::array<glm::vec3, 6> lower{}, upper{};
    for (int i = 0; i < 6; ++i) {
        const auto direction = radial(static_cast<float>(i) * Pi / 3.0f);
        const float size = 0.83f + 0.17f * variation(seed, i + 1);
        const auto edge = direction * radius * size;
        lower[i] = center + edge * 0.67f - glm::vec3(0, radius.y * 0.45f, 0);
        upper[i] = center + edge + glm::vec3(0, radius.y * 0.22f, 0);
    }
    const auto top = center + glm::vec3(radius.x * 0.12f, radius.y * 0.85f, 0);
    const auto bottom = center - glm::vec3(0, radius.y * 0.9f, 0);
    for (int i = 0; i < 6; ++i) {
        const int next = (i + 1) % 6;
        const auto sideColor = color * (0.86f + 0.12f * variation(seed, i + 9));
        b.quad(lower[i], upper[i], upper[next], lower[next], material, sideColor);
        b.triangle(top, upper[next], upper[i], material, color);
        b.triangle(bottom, lower[i], lower[next], material, color * 0.7f);
    }
}

void fern(AssetBuilder& b, std::uint32_t seed)
{
    const float turn = variation(seed, 1) * 2.0f * Pi;
    for (int frond = 0; frond < 7; ++frond) {
        const auto outward = radial(turn + static_cast<float>(frond) * 2.0f * Pi / 7.0f);
        const auto sideways = glm::vec3(-outward.z, 0, outward.x);
        const float length = 0.48f + 0.18f * variation(seed, frond + 12);
        const float height = 0.43f + 0.31f * variation(seed, frond + 21);
        const auto color = Leaf * (0.85f + 0.22f * variation(seed, frond + 30));
        const auto center = [&](float t) {
            return outward * length * t + glm::vec3(0, 0.025f + height * std::sin(t * Pi * 0.79f), 0);
        };
        auto previous = glm::vec3(0);
        for (int step = 1; step <= 4; ++step) {
            const float t = static_cast<float>(step) / 4.0f;
            const auto stem = center(t);
            b.beam(previous, stem, 0.018f, Block::Leaves, Stem);
            previous = stem;
            if (step == 4) continue;
            const float reach = 0.23f * (1.0f - t * 0.55f);
            for (const float side : {-1.0f, 1.0f}) {
                const auto tip = stem + sideways * reach * side + outward * 0.13f
                    + glm::vec3(0, 0.07f - t * 0.09f, 0);
                leaf(b, stem, tip, reach * 0.33f, color);
            }
        }
        leaf(b, center(0.79f), center(1.0f) + outward * 0.10f, 0.057f, color);
    }
}

void blossom(AssetBuilder& b, glm::vec3 center, float radius,
             glm::vec3 color, float turn, std::uint32_t seed)
{
    for (int petal = 0; petal < 5; ++petal) {
        const auto direction = radial(turn + static_cast<float>(petal) * 2.0f * Pi / 5.0f);
        leaf(b, center, center + direction * radius + glm::vec3(0, radius * 0.16f, 0),
             radius * 0.40f, color);
    }
    clump(b, center + glm::vec3(0, 0.016f, 0), {radius * 0.24f, 0.026f, radius * 0.24f},
          {0.91f, 0.72f, 0.30f}, seed);
}

void flowers(AssetBuilder& b, std::uint32_t seed)
{
    constexpr std::array<glm::vec3, 3> colors{{
        {0.62f, 0.36f, 0.86f}, {0.85f, 0.28f, 0.57f}, {0.96f, 0.83f, 0.52f}}};
    const float turn = variation(seed, 2) * 2.0f * Pi;
    for (int i = 0; i < 5; ++i) {
        const auto direction = radial(turn + static_cast<float>(i) * 2.4f);
        const auto foot = direction * (0.12f + 0.22f * variation(seed, i + 11));
        const float height = 0.43f + 0.46f * variation(seed, i + 21);
        const auto middle = foot + glm::vec3(0, height * 0.50f, 0);
        const auto flower = foot + direction * 0.09f + glm::vec3(0, height, 0);
        b.beam(foot, middle, 0.022f, Block::Leaves, Stem);
        b.beam(middle, flower, 0.018f, Block::Leaves, Stem);
        leaf(b, middle, middle + direction * 0.25f + glm::vec3(0, 0.11f, 0), 0.09f, Leaf);
        leaf(b, foot, foot - direction * 0.22f + glm::vec3(0, 0.19f, 0), 0.08f, Leaf * 0.86f);
        const auto color = colors[(seed + static_cast<std::uint32_t>(i / 3)) % colors.size()];
        blossom(b, flower, 0.13f + 0.045f * variation(seed, i + 31), color, turn + i, seed + i);
    }
}

void mushrooms(AssetBuilder& b, std::uint32_t seed)
{
    constexpr std::array<glm::vec3, 3> colors{{
        {0.48f, 0.23f, 0.52f}, {0.73f, 0.29f, 0.53f}, {0.21f, 0.57f, 0.52f}}};
    const int count = 3 + static_cast<int>(seed % 3);
    const float turn = variation(seed, 1) * 2.0f * Pi;
    for (int mushroom = 0; mushroom < count; ++mushroom) {
        const auto direction = radial(turn + static_cast<float>(mushroom) * 2.4f);
        const auto foot = direction * (mushroom == 0 ? 0.08f : 0.38f);
        const float height = 0.28f + 0.48f * variation(seed, mushroom + 11);
        const float radius = 0.18f + 0.15f * variation(seed, mushroom + 21);
        const auto cap = foot + direction * 0.045f + glm::vec3(0, height, 0);
        b.beam(foot, cap, radius * 0.31f, Block::Leaves, {0.79f, 0.76f, 0.58f});
        const auto color = colors[(seed + static_cast<std::uint32_t>(mushroom)) % colors.size()];
        std::array<std::array<glm::vec3, 8>, 4> rings{};
        constexpr float widths[]{0.91f, 1.0f, 0.72f, 0.30f};
        constexpr float heights[]{-0.035f, 0.012f, 0.13f, 0.19f};
        for (int level = 0; level < 4; ++level) {
            for (int side = 0; side < 8; ++side) {
                const auto edge = radial(turn + static_cast<float>(side) * Pi / 4.0f);
                const float uneven = 0.92f + 0.08f * variation(seed + mushroom, side + 40);
                rings[level][side] = cap + edge * radius * widths[level] * uneven
                    + glm::vec3(0, heights[level], 0);
            }
        }
        for (int side = 0; side < 8; ++side) {
            const int next = (side + 1) % 8;
            for (int level = 0; level < 3; ++level) {
                b.quad(rings[level][side], rings[level + 1][side], rings[level + 1][next], rings[level][next],
                       Block::Leaves, color * (0.8f + static_cast<float>(level) * 0.1f));
            }
            b.triangle(cap + glm::vec3(0, 0.19f, 0), rings[3][next], rings[3][side], Block::Leaves, color);
            const bool glow = mushroom == 0 && side % 4 == 0;
            b.triangle(cap - glm::vec3(0, 0.047f, 0), rings[0][side], rings[0][next],
                       glow ? Block::GlowLeaves : Block::Leaves,
                       glow ? glm::vec3(0.45f, 0.79f, 0.66f) : glm::vec3(0.66f, 0.64f, 0.49f));
        }
    }
}

void reeds(AssetBuilder& b, std::uint32_t seed)
{
    const float turn = variation(seed, 1) * 2.0f * Pi;
    for (int i = 0; i < 6; ++i) {
        const auto direction = radial(turn + static_cast<float>(i) * 2.4f);
        const auto foot = direction * (0.10f + 0.20f * variation(seed, i + 11));
        const float height = 1.03f + 0.48f * variation(seed, i + 21);
        const auto middle = foot + direction * 0.08f + glm::vec3(0, height * 0.55f, 0);
        const auto tip = foot + direction * 0.20f + glm::vec3(0, height, 0);
        b.beam(foot, middle, 0.024f, Block::Leaves, {0.32f, 0.43f, 0.18f});
        b.beam(middle, tip, 0.019f, Block::Leaves, {0.43f, 0.51f, 0.22f});
        leaf(b, middle, middle + direction * 0.40f + glm::vec3(0, 0.32f, 0), 0.065f, Leaf);
        const auto other = foot + glm::vec3(0, height * 0.27f, 0);
        leaf(b, other, other - direction * 0.32f + glm::vec3(0, 0.41f, 0), 0.07f, Leaf * 0.87f);
        if ((seed + static_cast<std::uint32_t>(i)) % 3 == 0) {
            b.beam(tip - glm::vec3(0, 0.20f, 0), tip, 0.065f, Block::Leaves, {0.55f, 0.36f, 0.17f});
        } else {
            leaf(b, middle, tip + direction * 0.08f, 0.042f, {0.40f, 0.62f, 0.29f});
        }
    }
}

void lilyPads(AssetBuilder& b, std::uint32_t seed)
{
    const float turn = variation(seed, 1) * 2.0f * Pi;
    for (int pad = 0; pad < 3; ++pad) {
        const auto center = radial(turn + static_cast<float>(pad) * 2.25f) * (pad == 0 ? 0.13f : 0.49f)
            + glm::vec3(0, 0.023f + static_cast<float>(pad) * 0.008f, 0);
        const float radius = 0.30f + 0.12f * variation(seed, pad + 10);
        std::array<glm::vec3, 9> edge{};
        for (int side = 0; side < 9; ++side) {
            edge[side] = center + radial(turn + static_cast<float>(side) * 2.0f * Pi / 9.0f)
                * radius * (0.88f + 0.12f * variation(seed + pad, side + 31));
        }
        for (int side = 1; side < 9; ++side) {
            const auto color = glm::vec3(0.19f, 0.48f, 0.28f) * (0.88f + static_cast<float>(side % 3) * 0.08f);
            b.triangle(center, edge[(side + 1) % 9], edge[side], Block::Leaves, color, true);
        }
        if (pad == 0) {
            blossom(b, center + glm::vec3(0, 0.035f, 0), 0.17f,
                    {0.91f, 0.44f, 0.65f}, turn, seed);
        }
    }
}

void vines(AssetBuilder& b, std::uint32_t seed)
{
    const float length = 1.9f + 1.15f * variation(seed, 1);
    const float turn = variation(seed, 2) * 2.0f * Pi;
    auto previous = glm::vec3(0);
    for (int node = 1; node <= 7; ++node) {
        const float t = static_cast<float>(node) / 7.0f;
        const auto point = glm::vec3(std::sin(t * 4.5f + turn) * 0.18f * t,
                                    -length * t, std::cos(t * 3.0f + turn) * 0.13f * t);
        b.beam(previous, point, 0.023f, Block::Leaves, Stem * 0.88f);
        previous = point;
        const auto outward = radial(turn + static_cast<float>(node) * 0.63f);
        for (const float side : {-1.0f, 1.0f}) {
            const auto branch = point + outward * side * 0.085f - glm::vec3(0, 0.025f, 0);
            b.beam(point, branch, 0.014f, Block::Leaves, Stem);
            leaf(b, branch, branch + outward * side * (0.24f + 0.09f * variation(seed, node + 10))
                     - glm::vec3(0, 0.15f, 0), 0.12f, Leaf * (side > 0 ? 1.05f : 0.84f));
        }
        if (node == 3 || node == 6) {
            const auto bud = point + glm::vec3(0, -0.10f, 0);
            b.beam(point, bud, 0.012f, Block::Leaves, Stem);
            clump(b, bud, {0.047f, 0.061f, 0.047f}, {0.43f, 0.75f, 0.58f}, seed + node, Block::GlowLeaves);
        }
    }
}

void shrub(AssetBuilder& b, std::uint32_t seed)
{
    const float turn = variation(seed, 1) * 2.0f * Pi;
    const auto trunk = glm::vec3(0, 0.34f, 0);
    b.beam({0, 0, 0}, trunk, 0.065f, Block::Timber, {0.28f, 0.29f, 0.16f});
    for (int branch = 0; branch < 6; ++branch) {
        const auto outward = radial(turn + static_cast<float>(branch) * 2.4f);
        const auto center = outward * (0.18f + 0.20f * variation(seed, branch + 11))
            + glm::vec3(0, 0.48f + 0.37f * variation(seed, branch + 21), 0);
        b.beam(trunk, center, 0.032f, Block::Timber, {0.30f, 0.34f, 0.18f});
        const float size = 0.22f + 0.09f * variation(seed, branch + 31);
        const auto color = Leaf * (0.85f + 0.23f * variation(seed, branch + 41));
        clump(b, center, {size, size * 0.73f, size * 0.92f}, color, seed + branch);
        for (int spray = 0; spray < 3; ++spray) {
            const auto direction = radial(turn + static_cast<float>(branch + spray) * 1.7f);
            leaf(b, center, center + direction * (size + 0.16f) + glm::vec3(0, 0.13f, 0), 0.085f, color);
        }
    }
}

} // namespace

void appendHubFoliage(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder builder(mesh, asset);
    switch (asset.kind) {
    case HubAssetKind::Fern: fern(builder, asset.variation); break;
    case HubAssetKind::Flowers: flowers(builder, asset.variation); break;
    case HubAssetKind::Mushrooms: mushrooms(builder, asset.variation); break;
    case HubAssetKind::Reeds: reeds(builder, asset.variation); break;
    case HubAssetKind::LilyPads: lilyPads(builder, asset.variation); break;
    case HubAssetKind::Vines: vines(builder, asset.variation); break;
    case HubAssetKind::Shrub: shrub(builder, asset.variation); break;
    default: break;
    }
}

} // namespace voxel
