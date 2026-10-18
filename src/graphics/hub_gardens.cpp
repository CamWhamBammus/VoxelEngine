#include "asset_builder.h"

#include <array>
#include <cmath>

namespace voxel {
namespace {

constexpr float Pi = 3.14159265359f;
constexpr glm::vec3 Stem{0.24f, 0.39f, 0.13f};
constexpr glm::vec3 Vein{0.57f, 0.68f, 0.31f};

float variation(std::uint32_t seed, std::uint32_t part)
{
    auto value = seed ^ (part * 0x9e3779b9u + 0x85ebca6bu);
    value = (value ^ (value >> 16)) * 0x7feb352du;
    value = (value ^ (value >> 15)) * 0x846ca68bu;
    return float((value ^ (value >> 16)) & 0xffffu) / 65535.0f;
}

glm::vec3 radial(float angle) { return {std::cos(angle), 0, std::sin(angle)}; }

glm::vec3 leafColor(std::uint32_t seed, int part)
{
    constexpr std::array<glm::vec3, 4> colors{{
        {0.22f,0.49f,0.23f}, {0.36f,0.58f,0.19f},
        {0.17f,0.44f,0.29f}, {0.42f,0.61f,0.25f}
    }};
    return colors[(seed + part) % colors.size()] * (0.91f + 0.09f * variation(seed, part + 19));
}

// A curved blade has four cross-sections and a folded, pale central vein.
// Explicit back faces let the broad leaves read from both paths and balconies.
void broadBlade(AssetBuilder& out, glm::vec3 base, glm::vec3 control,
                glm::vec3 tip, float width, glm::vec3 color)
{
    const auto across = glm::normalize(glm::cross(tip - base, glm::vec3(0,1,0)));
    constexpr std::array<float, 4> t{{0,0.27f,0.58f,0.83f}};
    constexpr std::array<float, 4> shape{{0.055f,0.87f,1.0f,0.55f}};
    constexpr std::array<float, 4> edges{{-1,-0.045f,0.045f,1}};
    const std::array<glm::vec3, 3> paint{{color * 0.84f, Vein, color}};
    std::array<glm::vec3, 4> previous{};
    for (std::size_t section = 0; section < t.size(); ++section) {
        const float s = t[section];
        const auto center = base * ((1-s)*(1-s)) + control * (2*s*(1-s)) + tip * (s*s);
        std::array<glm::vec3, 4> row{};
        for (std::size_t edge = 0; edge < edges.size(); ++edge) {
            const float fold = edge == 0 || edge == 3 ? -width * shape[section] * 0.13f : 0;
            row[edge] = center + across * (edges[edge] * width * shape[section]) + glm::vec3(0,fold,0);
        }
        if (section != 0) {
            for (std::size_t edge = 0; edge + 1 < edges.size(); ++edge)
                out.quad(previous[edge], previous[edge+1], row[edge+1], row[edge], Block::Leaves, paint[edge], true);
        }
        previous = row;
    }
    for (std::size_t edge = 0; edge + 1 < edges.size(); ++edge)
        out.triangle(previous[edge], previous[edge+1], tip, Block::Leaves, paint[edge], true);
}

void leaflet(AssetBuilder& out, glm::vec3 base, glm::vec3 tip, float width, glm::vec3 color)
{
    const auto along = tip - base;
    const auto across = glm::normalize(glm::cross(along, glm::vec3(0,1,0)));
    const auto ridge = base + along * 0.52f + glm::vec3(0,width*0.17f,0);
    const auto left = base + along * 0.36f + across * width;
    const auto right = base + along * 0.49f - across * width * 0.86f;
    out.triangle(base, left, ridge, Block::Leaves, color, true);
    out.triangle(left, tip, ridge, Block::Leaves, color, true);
    out.triangle(tip, right, ridge, Block::Leaves, color * 0.82f, true);
    out.triangle(right, base, ridge, Block::Leaves, color * 0.82f, true);
}

void flower(AssetBuilder& out, glm::vec3 foot, float height, float turn, glm::vec3 color)
{
    const auto center = foot + glm::vec3(0.025f,height,0);
    out.beam(foot, center, 0.017f, Block::Leaves, Stem);
    for (int petal = 0; petal < 4; ++petal)
        leaflet(out, center, center + radial(turn + petal * Pi * 0.5f) * 0.10f + glm::vec3(0,0.018f,0), 0.047f, color);
    // A tiny raised heart separates the four rounded petals.
    out.box(center - glm::vec3(0.018f,0,0.018f), center + glm::vec3(0.018f,0.025f,0.018f),
            Block::Leaves, {0.85f,0.66f,0.23f});
}

void broadleaf(AssetBuilder& out, std::uint32_t seed)
{
    const int count = 5 + int(seed % 4);
    const float turn = variation(seed, 1) * Pi * 2;
    for (int leaf = 0; leaf < count; ++leaf) {
        const auto outward = radial(turn + leaf * Pi * 2 / count + 0.12f * variation(seed, leaf + 2));
        const auto foot = outward * 0.12f + glm::vec3(0,0.03f,0);
        const float height = 1.15f + 0.80f * variation(seed, leaf + 11);
        const auto middle = foot + outward * 0.06f + glm::vec3(0,height*0.30f,0);
        const auto base = foot + outward * 0.13f + glm::vec3(0,height*0.59f,0);
        out.beam(foot, middle, 0.029f, Block::Leaves, Stem);
        out.beam(middle, base, 0.023f, Block::Leaves, Stem);
        const auto tip = outward * (0.92f + 0.17f * variation(seed, leaf + 21)) + glm::vec3(0,height*0.82f,0);
        const auto control = outward * 0.56f + glm::vec3(0,height+0.24f,0);
        broadBlade(out, base, control, tip, 0.31f + 0.09f * variation(seed, leaf + 31), leafColor(seed, leaf));
    }
}

void fernBed(AssetBuilder& out, std::uint32_t seed)
{
    const float turn = variation(seed, 1) * Pi * 2;
    for (int plant = 0; plant < 2; ++plant) {
        const auto foot = radial(turn + plant * Pi) * 0.28f + glm::vec3(0,0.03f,0);
        for (int frond = 0; frond < 4; ++frond) {
            const int tag = plant * 4 + frond;
            const auto outward = radial(turn + frond * Pi * 0.5f + plant * 0.65f);
            const auto across = glm::vec3(-outward.z,0,outward.x);
            const float reach = 0.67f + 0.15f * variation(seed, tag + 11);
            const float height = 0.83f + 0.43f * variation(seed, tag + 21);
            const auto center = [&](float t) {
                return foot + outward * (reach*t) + glm::vec3(0,height*std::sin(t*Pi*0.78f),0);
            };
            const auto color = leafColor(seed, tag);
            auto previous = foot;
            for (int section = 1; section <= 3; ++section) {
                const float t = section * 0.27f;
                const auto stem = center(t);
                if (section == 1) out.beam(previous, stem, 0.022f, Block::Leaves, Stem);
                else out.quad(previous-across*0.011f, previous+across*0.011f,
                              stem+across*0.008f, stem-across*0.008f, Block::Leaves, Vein, true);
                const float length = 0.27f * (1-t*0.47f);
                for (const float side : {-1.0f,1.0f})
                    leaflet(out, stem, stem + across*(side*length) + outward*0.12f + glm::vec3(0,0.055f-t*0.07f,0),
                            length*0.36f, color);
                previous = stem;
            }
            leaflet(out, previous, center(1) + outward*0.06f, 0.075f, color);
        }
    }
    for (int i = 0; i < 2; ++i) {
        const auto foot = radial(turn + 1.1f + i*2.6f) * 0.53f + glm::vec3(0,0.035f,0);
        flower(out, foot, 0.31f + i*0.10f, turn+i, i == 0 ? glm::vec3(0.86f,0.72f,0.41f) : glm::vec3(0.67f,0.54f,0.73f));
        leaflet(out, foot, foot + radial(turn+i) * 0.22f + glm::vec3(0,0.02f,0), 0.10f, {0.43f,0.31f,0.17f});
    }
}

void cushion(AssetBuilder& out, glm::vec3 center, glm::vec3 size,
             glm::vec3 color, std::uint32_t seed, Block material)
{
    std::array<glm::vec3, 8> low{}, shoulder{};
    for (int side = 0; side < 8; ++side) {
        const auto edge = radial(side*Pi*0.25f) * size * (0.85f+0.15f*variation(seed,side+1));
        low[side] = center + edge*0.80f;
        shoulder[side] = center + edge + glm::vec3(0,size.y*0.46f,0);
    }
    const auto top = center + glm::vec3(size.x*0.12f,size.y,size.z*0.08f);
    for (int side = 0; side < 8; ++side) {
        const int next = (side+1)%8;
        out.quad(low[side], shoulder[side], shoulder[next], low[next], material, color*0.77f);
        out.triangle(top, shoulder[next], shoulder[side], material,
                     color*(0.88f+0.12f*variation(seed,side+11)));
        out.triangle(center, low[side], low[next], material, color*0.60f);
    }
}

void mossGarden(AssetBuilder& out, std::uint32_t seed)
{
    const float turn = variation(seed, 1) * Pi * 2;
    for (int mound = 0; mound < 7; ++mound) {
        const auto center = mound == 0 ? glm::vec3(0) : radial(turn+mound*Pi/3) * (0.55f+0.10f*variation(seed,mound+2));
        const float radius = 0.30f + 0.12f*variation(seed,mound+11);
        cushion(out, center, {radius,0.14f+0.13f*variation(seed,mound+21),radius*0.85f},
                leafColor(seed,mound), seed+mound, Block::Leaves);
    }
    for (int pebble = 0; pebble < 3; ++pebble) {
        const auto center = radial(turn+pebble*2.1f) * 0.81f;
        cushion(out, center, {0.13f,0.10f,0.10f}, {0.39f,0.43f,0.40f}, seed+pebble, Block::Stone);
    }
    for (int i = 0; i < 2; ++i)
        flower(out, radial(turn+i*2.7f)*0.48f + glm::vec3(0,0.02f,0),
               0.25f+0.035f*variation(seed,i+31), turn+i, {0.86f,0.77f,0.51f});
}

} // namespace

void appendHubGarden(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh, asset);
    switch (asset.kind) {
    case HubAssetKind::BroadleafClump: broadleaf(out, asset.variation); break;
    case HubAssetKind::FernBed: fernBed(out, asset.variation); break;
    case HubAssetKind::MossGarden: mossGarden(out, asset.variation); break;
    default: break;
    }
}

} // namespace voxel
