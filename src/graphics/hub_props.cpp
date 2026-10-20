#include "asset_builder.h"

#include <array>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

constexpr glm::vec3 Wood{0.42f, 0.23f, 0.10f};
constexpr glm::vec3 LightWood{0.64f, 0.40f, 0.19f};
constexpr glm::vec3 Metal{0.075f, 0.13f, 0.15f};
constexpr glm::vec3 Copper{0.58f, 0.31f, 0.13f};
constexpr glm::vec3 Cream{0.84f, 0.75f, 0.51f};

glm::vec3 clothColor(std::uint32_t variation)
{
    constexpr std::array<glm::vec3, 3> colors{{
        {0.055f, 0.40f, 0.39f}, {0.39f, 0.15f, 0.32f}, {0.68f, 0.37f, 0.10f}
    }};
    return colors[variation % colors.size()];
}

// A closed square frustum makes a shallow, overhanging lantern roof.
void cap(AssetBuilder& out, float bottom, float top, float lowerWidth, float upperWidth, glm::vec3 color)
{
    const float a = lowerWidth * 0.5f, b = upperWidth * 0.5f;
    const std::array<glm::vec3, 4> low{{{-a,bottom,-a}, {-a,bottom,a}, {a,bottom,a}, {a,bottom,-a}}};
    const std::array<glm::vec3, 4> high{{{-b,top,-b}, {-b,top,b}, {b,top,b}, {b,top,-b}}};
    for (int side = 0; side < 4; ++side) {
        const int next = (side + 1) % 4;
        out.quad(low[side], low[next], high[next], high[side], Block::Slate,
                 color * (side % 2 == 0 ? 1.0f : 0.86f));
    }
    out.quad(high[0], high[1], high[2], high[3], Block::Slate, color);
    out.quad(low[3], low[2], low[1], low[0], Block::Slate, Metal);
}

void loop(AssetBuilder& out, float bottom, float width, float height)
{
    const float half = width * 0.5f;
    out.box({-half,bottom,-0.035f}, {-half+0.05f,bottom+height,0.035f}, Block::Slate, Copper);
    out.box({half-0.05f,bottom,-0.035f}, {half,bottom+height,0.035f}, Block::Slate, Copper);
    out.box({-half,bottom+height-0.05f,-0.035f}, {half,bottom+height,0.035f}, Block::Slate, Copper);
}

void lanternCap(AssetBuilder& out, std::uint32_t variation)
{
    cap(out, 0.51f, 0.83f, 1.46f, 0.54f, variation % 2 == 0 ? Copper : clothColor(0));
    out.box({-0.75f,0.50f,-0.75f}, {0.75f,0.55f,0.75f}, Block::Slate, Copper);
    out.box({-0.32f,0.82f,-0.32f}, {0.32f,0.89f,0.32f}, Block::Slate, Metal);
    loop(out, 0.88f, 0.26f, 0.30f);
    // The voxel lantern remains visible between these thin outside struts.
    for (const float x : {-0.525f, 0.525f}) for (const float z : {-0.525f, 0.525f})
        out.box({x-0.035f,-0.53f,z-0.035f}, {x+0.035f,0.53f,z+0.035f}, Block::Slate, Metal);
    out.box({-0.58f,-0.58f,-0.58f}, {0.58f,-0.50f,0.58f}, Block::Slate, Copper);
}

void hangingLamp(AssetBuilder& out, std::uint32_t variation)
{
    out.box({-0.25f,-0.35f,-0.25f}, {0.25f,0.35f,0.25f}, Block::Lantern, {1.0f,0.72f,0.28f});
    cap(out, 0.36f, 0.60f, 0.85f, 0.28f, variation % 2 == 0 ? Copper : clothColor(0));
    out.box({-0.34f,-0.43f,-0.34f}, {0.34f,-0.35f,0.34f}, Block::Slate, Copper);
    for (const float x : {-0.27f, 0.27f}) for (const float z : {-0.27f, 0.27f})
        out.box({x-0.025f,-0.38f,z-0.025f}, {x+0.025f,0.40f,z+0.025f}, Block::Slate, Metal);
    loop(out, 0.59f, 0.20f, 0.23f);
    // Alternating narrow rectangular links read clearly without a cylinder mesh.
    for (int link = 0; link < 4; ++link) {
        const float y = 0.78f + link * 0.18f;
        if (link % 2 == 0) loop(out, y, 0.13f, 0.23f);
        else {
            out.box({-0.025f,y,-0.075f}, {0.025f,y+0.23f,-0.025f}, Block::Slate, Metal);
            out.box({-0.025f,y,0.025f}, {0.025f,y+0.23f,0.075f}, Block::Slate, Metal);
            out.box({-0.025f,y+0.18f,-0.075f}, {0.025f,y+0.23f,0.075f}, Block::Slate, Metal);
        }
    }
}

void crystal(AssetBuilder& out, glm::vec3 base, float height, float radius, glm::vec2 lean)
{
    std::array<glm::vec3, 6> low, shoulder;
    for (int side = 0; side < 6; ++side) {
        const float angle = static_cast<float>(side) * 1.0471975512f;
        const glm::vec3 radial{std::cos(angle) * radius, 0, std::sin(angle) * radius};
        low[side] = base + radial * 0.80f;
        shoulder[side] = base + radial + glm::vec3(lean.x * 0.68f, height * 0.68f, lean.y * 0.68f);
    }
    const auto tip = base + glm::vec3(lean.x, height, lean.y);
    for (int side = 0; side < 6; ++side) {
        const int next = (side + 1) % 6;
        const glm::vec3 color = side % 3 == 0 ? glm::vec3(0.08f,0.58f,0.76f)
                                            : glm::vec3(0.12f,0.82f,0.87f);
        out.quad(low[next], low[side], shoulder[side], shoulder[next], Block::Crystal, color);
        out.triangle(shoulder[next], shoulder[side], tip, Block::Crystal, color * 1.08f);
        out.triangle(base, low[side], low[next], Block::Crystal, color * 0.6f);
    }
}

void beacon(AssetBuilder& out, std::uint32_t variation)
{
    out.box({-0.64f,0,-0.64f}, {0.64f,0.15f,0.64f}, Block::Slate, {0.22f,0.29f,0.31f});
    cap(out, 0.15f, 0.40f, 1.08f, 0.82f, Copper);
    out.box({-0.44f,0.38f,-0.44f}, {0.44f,0.45f,0.44f}, Block::Slate, Metal);
    crystal(out, {0,0.44f,0}, 1.66f, 0.30f, {variation % 2 == 0 ? 0.14f : -0.14f, 0.04f});
    crystal(out, {-0.30f,0.40f,0.14f}, 1.02f, 0.16f, {-0.28f,0.10f});
    crystal(out, {0.29f,0.40f,-0.16f}, 0.80f, 0.16f, {0.24f,-0.08f});
    for (const float x : {-0.48f, 0.48f})
        out.box({x-0.04f,0.14f,-0.52f}, {x+0.04f,0.46f,0.52f}, Block::Slate, Cream * 0.72f);
}

void windowBox(AssetBuilder& out, std::uint32_t variation)
{
    out.box({-1,-0.35f,0.02f}, {1,0,0.55f}, Block::Planks, Wood);
    out.box({-0.9f,-0.015f,0.08f}, {0.9f,0.025f,0.49f}, Block::Dirt, {0.19f,0.12f,0.075f});
    out.box({-1.04f,-0.07f,0.48f}, {1.04f,0.05f,0.60f}, Block::Timber, LightWood);
    for (const float x : {-0.76f, 0.76f})
        out.box({x-0.035f,-0.37f,0.01f}, {x+0.035f,0.035f,0.58f}, Block::Slate, Metal);
    for (int plant = 0; plant < 5; ++plant) {
        const float x = -0.72f + plant * 0.36f;
        const float height = 0.30f + 0.08f * ((plant + variation) % 3);
        out.beam({x,0.02f,0.27f}, {x+0.05f,height,0.28f}, 0.025f, Block::Leaves, {0.19f,0.38f,0.10f});
        out.triangle({x,0.08f,0.27f}, {x-0.22f,0.24f,0.35f}, {x-0.08f,0.29f,0.27f}, Block::Leaves, {0.25f,0.53f,0.13f}, true);
        out.triangle({x,0.13f,0.27f}, {x+0.10f,0.34f,0.27f}, {x+0.25f,0.23f,0.38f}, Block::Leaves, {0.34f,0.60f,0.16f}, true);
        const glm::vec3 center{x+0.05f,height,0.28f};
        const glm::vec3 color = (plant + variation) % 2 == 0 ? glm::vec3(0.87f,0.42f,0.24f) : glm::vec3(0.78f,0.59f,0.82f);
        for (int petal = 0; petal < 4; ++petal) {
            const float angle = petal * 1.5707963268f;
            const glm::vec3 along{std::cos(angle)*0.14f,0.04f,std::sin(angle)*0.14f};
            const glm::vec3 across{-std::sin(angle)*0.07f,0,std::cos(angle)*0.07f};
            out.quad(center, center+along+across, center+along*1.3f, center+along-across, Block::RoofTiles, color, true);
        }
        out.box(center-glm::vec3(0.035f,0.01f,0.035f), center+glm::vec3(0.035f,0.04f,0.035f), Block::RoofTiles, Cream);
    }
}

void shutters(AssetBuilder& out, std::uint32_t variation)
{
    const auto color = variation % 3 == 0 ? clothColor(0) : LightWood;
    for (const float side : {-1.0f, 1.0f}) {
        const float center = side * 1.36f;
        out.box({center-0.30f,0,0.02f}, {center+0.30f,1.80f,0.15f}, Block::Planks, color);
        for (const float y : {0.18f, 1.47f})
            out.box({center-0.34f,y,0.15f}, {center+0.34f,y+0.12f,0.21f}, Block::Timber, Wood);
        out.beam({center-0.24f,0.32f,0.20f}, {center+0.24f,1.46f,0.20f}, 0.065f, Block::Timber, Wood);
        for (const float y : {0.25f, 1.53f})
            out.box({center-side*0.24f-0.045f,y,0.19f}, {center-side*0.24f+0.045f,y+0.10f,0.25f}, Block::Slate, Metal);
    }
}

void awning(AssetBuilder& out, std::uint32_t variation)
{
    const auto color = clothColor(variation);
    for (int stripe = 0; stripe < 10; ++stripe) {
        const float left = -2.5f + stripe * 0.5f, right = left + 0.5f;
        const auto paint = stripe % 2 == 0 ? color : Cream;
        out.quad({left,0,0}, {right,0,0}, {right,-0.12f,0.9f}, {left,-0.12f,0.9f}, Block::RoofTiles, paint, true);
        out.quad({left,-0.12f,0.9f}, {right,-0.12f,0.9f}, {right,-0.45f,2.1f}, {left,-0.45f,2.1f}, Block::RoofTiles, paint, true);
        // Each stripe ends in a small rounded, faceted cloth scallop.
        const glm::vec3 top{(left+right)*0.5f,-0.44f,2.1f};
        for (int segment = 0; segment < 6; ++segment) {
            const float t0 = segment / 6.0f, t1 = (segment+1) / 6.0f;
            out.triangle(top, {left+t0*0.5f,-0.48f-0.23f*std::sin(t0*3.1415926536f),2.1f},
                {left+t1*0.5f,-0.48f-0.23f*std::sin(t1*3.1415926536f),2.1f}, Block::RoofTiles, paint, true);
        }
    }
    for (const float x : {-2.35f, 2.35f}) {
        out.beam({x,-0.10f,0.05f}, {x,-0.55f,2.12f}, 0.10f, Block::Timber, Wood);
        out.beam({x,-1.15f,0.05f}, {x,-0.47f,1.56f}, 0.12f, Block::Timber, Wood);
    }
    out.beam({-2.58f,-0.48f,2.08f}, {2.58f,-0.48f,2.08f}, 0.07f, Block::Timber, Wood);
}

void banner(AssetBuilder& out, std::uint32_t variation)
{
    const auto color = clothColor(variation);
    out.beam({-0.65f,0.05f,0.12f}, {0.65f,0.05f,0.12f}, 0.08f, Block::Timber, Wood);
    out.beam({0,0.10f,-0.3f}, {0,0.10f,0.18f}, 0.09f, Block::Slate, Copper);
    out.quad({-0.5f,0,0.1f}, {-0.5f,-2,0.1f}, {0.5f,-2,0.1f}, {0.5f,0,0.1f}, Block::RoofTiles, color, true);
    out.triangle({-0.5f,-2,0.1f}, {-0.5f,-2.5f,0.1f}, {0,-2,0.1f}, Block::RoofTiles, color, true);
    out.triangle({0,-2,0.1f}, {0.5f,-2.5f,0.1f}, {0.5f,-2,0.1f}, Block::RoofTiles, color, true);
    // Paint both cloth sides so the hanging banner can be viewed from either path.
    for (const float z : {0.095f, 0.105f}) {
        out.quad({0,-0.55f,z}, {-0.25f,-1.05f,z}, {0,-1.55f,z}, {0.25f,-1.05f,z}, Block::RoofTiles, Cream, true);
        out.quad({-0.45f,-0.18f,z}, {-0.45f,-0.25f,z}, {0.45f,-0.25f,z}, {0.45f,-0.18f,z}, Block::RoofTiles, Cream, true);
    }
}

void crate(AssetBuilder& out, glm::vec3 center, float width, float height)
{
    const float half = width * 0.5f;
    out.box(center+glm::vec3(-half,0,-half), center+glm::vec3(half,height,half), Block::Planks, LightWood);
    for (const float y : {0.05f, height-0.13f}) {
        out.box(center+glm::vec3(-half-0.025f,y,-half-0.025f), center+glm::vec3(half+0.025f,y+0.08f,half+0.025f), Block::Timber, Wood);
    }
    for (const float z : {-half-0.032f, half+0.032f})
        out.beam(center+glm::vec3(-half+0.06f,0.13f,z), center+glm::vec3(half-0.06f,height-0.13f,z), 0.065f, Block::Timber, Wood);
    for (const float x : {-half-0.032f, half+0.032f})
        out.beam(center+glm::vec3(x,0.13f,-half+0.06f), center+glm::vec3(x,height-0.13f,half-0.06f), 0.065f, Block::Timber, Wood);
}

void chestLid(AssetBuilder& out, float left, float right, float outset, Block material, glm::vec3 color)
{
    // A closed six-sided profile gives the lid real shoulders and a broad,
    // flat crown. Straps follow the same outline, slightly outside the wood.
    const std::array<glm::vec2, 6> profile{{
        {.75f,-.46f-outset}, {.91f+outset,-.46f-outset},
        {1.10f+outset,-.27f}, {1.10f+outset,.27f},
        {.91f+outset,.46f+outset}, {.75f,.46f+outset}
    }};
    for (std::size_t i = 0; i < profile.size(); ++i) {
        const auto a = profile[i], b = profile[(i+1) % profile.size()];
        out.quad({left,a.x,a.y}, {left,b.x,b.y}, {right,b.x,b.y}, {right,a.x,a.y},
                 material, color * (i == 2 ? 1.0f : .88f));
    }
    for (std::size_t i = 1; i+1 < profile.size(); ++i) {
        const auto a = profile[0], b = profile[i], c = profile[i+1];
        out.triangle({left,c.x,c.y}, {left,b.x,b.y}, {left,a.x,a.y}, material, color * .82f);
        out.triangle({right,a.x,a.y}, {right,b.x,b.y}, {right,c.x,c.y}, material, color * .92f);
    }
}

void lootChest(AssetBuilder& out, HubAssetKind kind)
{
    // Style is a distinct asset. Neither its variation nor its contents changes
    // the model's silhouette or collision.
    const auto preset = kind == HubAssetKind::CrystalChest ? 1
                      : kind == HubAssetKind::IronboundChest ? 2 : 0;
    constexpr std::array<glm::vec3, 3> woods{{
        {.51f,.29f,.13f}, {.27f,.23f,.39f}, {.25f,.16f,.14f}
    }};
    constexpr std::array<glm::vec3, 3> trims{{
        {.76f,.46f,.19f}, {.18f,.66f,.70f}, {.62f,.24f,.16f}
    }};
    const auto wood = woods[preset], trim = trims[preset];
    for (const float x : {-.56f,.56f}) for (const float z : {-.32f,.32f}) {
        out.box({x-.13f,0,z-.13f}, {x+.13f,.22f,z+.13f}, Block::Slate, Metal);
        out.box({x-.145f,.02f,z-.145f}, {x+.145f,.09f,z+.145f}, Block::Slate, trim * .72f);
    }
    out.box({-.70f,.13f,-.43f}, {.70f,.74f,.43f}, Block::Planks, wood);
    out.box({-.73f,.14f,-.46f}, {.73f,.22f,.46f}, Block::Timber, wood * .67f);
    out.box({-.73f,.69f,-.46f}, {.73f,.755f,.46f}, Block::Slate, Metal);
    chestLid(out, -.72f, .72f, 0, Block::Planks, wood * 1.12f);

    for (const float x : {-.49f,.49f}) {
        // Full-height banding, lid straps, and bright inset rivets.
        out.box({x-.065f,.20f,-.445f}, {x+.065f,.75f,.445f}, Block::Slate, Metal);
        chestLid(out, x-.075f, x+.075f, .018f, Block::Slate, trim);
        for (const float y : {.28f,.61f}) for (const float z : {-.454f,.454f})
            out.box({x-.027f,y-.026f,z-.012f}, {x+.027f,y+.026f,z+.012f},
                    Block::Slate, trim * 1.15f);
        out.box({x-.10f,.62f,-.49f}, {x+.10f,.84f,-.45f}, Block::Slate, trim * .8f);
    }

    // The front lock has a readable dark keyhole and a colored inset badge.
    out.box({-.14f,.48f,.46f}, {.14f,.88f,.515f}, Block::Slate, trim);
    out.box({-.10f,.52f,.516f}, {.10f,.76f,.535f}, Block::Slate, Metal);
    out.box({-.042f,.59f,.536f}, {.042f,.68f,.545f}, Block::Slate, trim * 1.15f);
    out.box({-.018f,.56f,.546f}, {.018f,.62f,.553f}, Block::Slate, Metal);
    out.quad({0,.94f,.462f}, {-.075f,.87f,.488f}, {0,.80f,.492f}, {.075f,.87f,.488f},
             Block::Slate, preset == 1 ? glm::vec3(.60f,.86f,.91f) : Cream);
    for (const float x : {-.754f,.754f}) {
        out.box({x-.017f,.39f,-.15f}, {x+.017f,.50f,.15f}, Block::Slate, trim);
        out.box({x-.03f,.32f,-.15f}, {x+.03f,.41f,-.09f}, Block::Slate, Metal);
        out.box({x-.03f,.32f,.09f}, {x+.03f,.41f,.15f}, Block::Slate, Metal);
        out.box({x-.03f,.30f,-.15f}, {x+.03f,.35f,.15f}, Block::Slate, Metal);
    }
}

void caveKey(AssetBuilder& out)
{
    const glm::vec3 brass(.89f,.63f,.20f), edge(1.0f,.82f,.36f);
    // A broad squared bow, short shaft and two teeth make the pickup readable
    // without an animation or a collision box filling the hole in its handle.
    out.box({-.18f,.48f,-.055f}, {-.10f,.78f,.055f}, Block::Slate, brass);
    out.box({ .10f,.48f,-.055f}, { .18f,.78f,.055f}, Block::Slate, brass);
    out.box({-.10f,.48f,-.055f}, { .10f,.56f,.055f}, Block::Slate, edge);
    out.box({-.10f,.70f,-.055f}, { .10f,.78f,.055f}, Block::Slate, edge);
    out.box({-.045f,0,-.055f}, {.045f,.50f,.055f}, Block::Slate, brass);
    out.box({ .045f,.035f,-.055f}, {.19f,.12f,.055f}, Block::Slate, edge);
    out.box({ .045f,.19f,-.055f}, {.14f,.27f,.055f}, Block::Slate, edge);
}

} // namespace

void appendHubProp(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh, asset);
    switch (asset.kind) {
    case HubAssetKind::LanternCap: lanternCap(out, asset.variation); break;
    case HubAssetKind::WallBracket:
        lanternCap(out, asset.variation);
        out.box({-0.12f,-0.55f,-0.93f}, {0.12f,0.73f,-0.83f}, Block::Slate, Metal);
        out.beam({0,0.66f,-0.88f}, {0,0.99f,0}, 0.10f, Block::Slate, Copper);
        out.beam({0,-0.4f,-0.88f}, {0,0.57f,-0.48f}, 0.07f, Block::Slate, Metal);
        break;
    case HubAssetKind::HangingLamp: hangingLamp(out, asset.variation); break;
    case HubAssetKind::CrystalBeacon: beacon(out, asset.variation); break;
    case HubAssetKind::WindowBox: windowBox(out, asset.variation); break;
    case HubAssetKind::Shutters: shutters(out, asset.variation); break;
    case HubAssetKind::Awning: awning(out, asset.variation); break;
    case HubAssetKind::RoofBrace:
        out.box({-0.12f,-1.15f,-0.08f}, {0.12f,0.10f,0.13f}, Block::Timber, Wood);
        out.beam({0,-1.05f,0.06f}, {0,-0.03f,1.25f}, 0.18f, Block::Timber, LightWood);
        out.beam({0,0,0}, {0,0,1.4f}, 0.18f, Block::Timber, Wood);
        break;
    case HubAssetKind::Banner: banner(out, asset.variation); break;
    case HubAssetKind::CrateStack:
        crate(out, {0,0,0}, 1.10f, 0.82f);
        if (asset.variation % 2 == 0) crate(out, {0.14f,0.82f,0.08f}, 0.74f, 0.65f);
        break;
    case HubAssetKind::LootChest:
    case HubAssetKind::CrystalChest:
    case HubAssetKind::IronboundChest: lootChest(out, asset.kind); break;
    case HubAssetKind::KeyPickup: caveKey(out); break;
    default: break; // Plant kinds are built by appendHubFoliage.
    }
}

} // namespace voxel
