#include "asset_builder.h"

#include <array>

namespace voxel {
namespace {

constexpr glm::vec3 Iron{0.08f,0.15f,0.17f};
constexpr glm::vec3 Brass{0.61f,0.38f,0.16f};

glm::vec3 timber(std::uint32_t variation)
{
    constexpr std::array<glm::vec3,3> colors{{
        {0.54f,0.32f,0.15f}, {0.39f,0.24f,0.13f}, {0.64f,0.43f,0.23f}
    }};
    return colors[variation % colors.size()];
}

glm::vec3 trim(std::uint32_t variation)
{
    constexpr std::array<glm::vec3,3> colors{{
        {0.12f,0.35f,0.33f}, {0.34f,0.40f,0.20f}, {0.54f,0.31f,0.16f}
    }};
    return colors[variation % colors.size()];
}

// End posts and their caps stop exactly at the two-metre module boundary.
// Neighbouring pieces can touch without overlapping geometry or protruding feet.
void endPosts(AssetBuilder& out, glm::vec3 wood, glm::vec3 paint)
{
    for (const float side : {-1.0f,1.0f}) {
        const float left=side < 0 ? -1.0f : 0.82f;
        const float right=side < 0 ? -0.82f : 1.0f;
        out.box({left,0,-0.13f},{right,1.32f,0.13f},Block::Timber,wood*0.80f);
        out.box({left,0.08f,-0.145f},{right,0.18f,0.145f},Block::Slate,Iron);
        out.box({left,1.23f,-0.17f},{right,1.34f,0.17f},Block::Planks,paint);
        out.box({left+0.02f,1.34f,-0.12f},{right-0.02f,1.43f,0.12f},Block::Timber,wood*1.13f);
        const float x=(left+right)*0.5f;
        for (const float z : {-0.145f,0.13f}) for (const float y : {0.43f,0.96f})
            out.box({x-0.027f,y,z},{x+0.027f,y+0.055f,z+0.015f},Block::Slate,Brass);
    }
}

// A module fills one voxel along its rail direction. Two neighbouring half
// posts meet at the shared cell face instead of colliding across two cells.
void fencePosts(AssetBuilder& out, glm::vec3 wood, glm::vec3 paint)
{
    for (const float side : {-1.0f,1.0f}) {
        const float left=side<0 ? -0.5f : 0.39f;
        const float right=side<0 ? -0.39f : 0.5f;
        out.box({left,0,-0.10f},{right,0.91f,0.10f},Block::Timber,wood*0.80f);
        out.box({left,0.06f,-0.11f},{right,0.14f,0.11f},Block::Slate,Iron);
        out.box({left,0.86f,-0.12f},{right,0.94f,0.12f},Block::Planks,paint);
        out.box({left+0.01f,0.94f,-0.085f},{right-0.01f,1.0f,0.085f},Block::Timber,wood*1.13f);
        const float x=(left+right)*0.5f;
        for (const float z : {-0.11f,0.10f}) for (const float y : {0.32f,0.70f})
            out.box({x-0.018f,y,z},{x+0.018f,y+0.036f,z+0.01f},Block::Slate,Brass);
    }
}

void timberFence(AssetBuilder& out, std::uint32_t variation)
{
    const auto wood=timber(variation);
    fencePosts(out,wood,trim(variation));
    for (const float y : {0.28f,0.66f})
        out.box({-0.40f,y,-0.06f},{0.40f,y+0.11f,0.06f},Block::Timber,wood);
    out.beam({-0.36f,0.38f,0},{0.36f,0.69f,0},0.065f,Block::Timber,wood*0.86f);
    out.box({-0.40f,0.77f,-0.07f},{0.40f,0.805f,0.07f},Block::Planks,wood*1.18f);
}

void pointedBoard(AssetBuilder& out, float x, float base, float height,
                  float width, float depth, glm::vec3 color)
{
    const float left=x-width*0.5f, right=x+width*0.5f;
    const float back=-depth*0.5f, front=depth*0.5f, shoulder=height-0.12f;
    out.box({left,base,back},{right,shoulder,front},Block::Planks,color);
    out.triangle({left,shoulder,front},{right,shoulder,front},{x,height,front},Block::Planks,color*1.10f);
    out.triangle({right,shoulder,back},{left,shoulder,back},{x,height,back},Block::Planks,color);
    out.quad({left,shoulder,back},{left,shoulder,front},{x,height,front},{x,height,back},Block::Timber,color*1.15f);
    out.quad({right,shoulder,front},{right,shoulder,back},{x,height,back},{x,height,front},Block::Timber,color*1.05f);
}

void picketFence(AssetBuilder& out, std::uint32_t variation)
{
    const auto wood=timber(variation), paint=trim(variation);
    fencePosts(out,wood,paint);
    for (const float y : {0.24f,0.64f})
        out.box({-0.40f,y,-0.09f},{0.40f,y+0.08f,0.01f},Block::Timber,wood*0.80f);
    for (int i=0; i<5; ++i) {
        const float x=-0.30f+i*0.15f;
        const float height=0.84f+0.07f*(1.0f-std::abs(x)/0.30f);
        pointedBoard(out,x,0.10f,height,0.10f,0.09f,
                     (variation%3==1 ? paint*1.25f : wood)*(i%2==0 ? 1.0f : 0.93f));
        for (const float y : {0.27f,0.67f})
            out.box({x-0.012f,y,0.045f},{x+0.012f,y+0.024f,0.055f},Block::Slate,Iron);
    }
}

void cornerFence(AssetBuilder& out, std::uint32_t variation)
{
    const auto wood=timber(variation), paint=trim(variation);
    // The corner is the anchor; both legs extend two metres toward +X / +Z.
    for (const glm::vec3 center : {glm::vec3(0,0,0),glm::vec3(1.85f,0,0),glm::vec3(0,0,1.85f)}) {
        out.box(center+glm::vec3(-0.12f,0,-0.12f),center+glm::vec3(0.12f,1.32f,0.12f),Block::Timber,wood*0.80f);
        out.box(center+glm::vec3(-0.15f,1.23f,-0.15f),center+glm::vec3(0.15f,1.34f,0.15f),Block::Planks,paint);
        out.box(center+glm::vec3(-0.10f,1.34f,-0.10f),center+glm::vec3(0.10f,1.43f,0.10f),Block::Timber,wood*1.13f);
        out.box(center+glm::vec3(-0.13f,0.08f,-0.13f),center+glm::vec3(0.13f,0.18f,0.13f),Block::Slate,Iron);
    }
    for (const float y : {0.39f,0.92f}) {
        out.box({0.10f,y,-0.08f},{1.85f,y+0.15f,0.08f},Block::Timber,wood);
        out.box({-0.08f,y,0.10f},{0.08f,y+0.15f,1.85f},Block::Timber,wood);
    }
    out.beam({0.12f,0.51f,0},{1.76f,0.95f,0},0.10f,Block::Timber,wood*0.86f);
    out.beam({0,0.51f,0.12f},{0,0.95f,1.76f},0.10f,Block::Timber,wood*0.86f);
    out.box({0.10f,1.07f,-0.095f},{1.85f,1.115f,0.095f},Block::Planks,wood*1.18f);
    out.box({-0.095f,1.07f,0.10f},{0.095f,1.115f,1.85f},Block::Planks,wood*1.18f);
}

void gardenGate(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh,asset);
    // Y cycles the fixed open/closed pose, with wood changing every two variants.
    const auto wood=timber(asset.variation/2), paint=trim(asset.variation/2);
    endPosts(out,wood,paint);
    for (const float y : {0.32f,0.85f})
        out.box({-0.87f,y,-0.07f},{-0.70f,y+0.14f,0.07f},Block::Slate,Iron);
    auto leaf=asset;
    leaf.position=out.point({-0.74f,0,0});
    if (asset.variation%2!=0) leaf.facing=(asset.facing+3)&3;
    AssetBuilder gate(mesh,leaf);
    for (int i=0; i<6; ++i) {
        const float x=0.12f+i*0.24f;
        const float height=1.13f+0.17f*(1.0f-std::abs(x-0.72f)/0.72f);
        pointedBoard(gate,x,0.15f,height,0.20f,0.13f,wood*(i%2==0 ? 1.05f : 0.94f));
    }
    for (const float y : {0.30f,0.84f})
        gate.box({0,y,-0.13f},{1.44f,y+0.13f,-0.055f},Block::Timber,wood*0.73f);
    gate.beam({0.08f,0.39f,-0.10f},{1.36f,0.88f,-0.10f},0.085f,Block::Timber,wood*0.73f);
    for (const float y : {0.32f,0.85f}) {
        gate.box({-0.045f,y,-0.04f},{0.04f,y+0.14f,0.10f},Block::Slate,Iron);
        gate.box({0.015f,y+0.03f,0.065f},{0.35f,y+0.10f,0.095f},Block::Slate,Iron);
        gate.box({0.27f,y+0.05f,0.095f},{0.30f,y+0.08f,0.11f},Block::Slate,Brass);
    }
    gate.box({1.20f,0.74f,0.065f},{1.40f,0.82f,0.10f},Block::Slate,Iron);
    gate.box({1.27f,0.68f,0.10f},{1.32f,0.88f,0.16f},Block::Slate,Brass);
}

void ironRailing(AssetBuilder& out, std::uint32_t variation)
{
    const auto wood=timber(variation), paint=trim(variation);
    for (const float side : {-1.0f,1.0f}) {
        const float left=side<0 ? -1.0f : 0.84f, right=side<0 ? -0.84f : 1.0f;
        out.box({left,0,-0.12f},{right,0.10f,0.12f},Block::Slate,Iron);
        out.box({left+0.025f,0.10f,-0.065f},{right-0.025f,1.26f,0.065f},Block::Slate,Iron*1.18f);
        out.box({left,1.20f,-0.11f},{right,1.32f,0.11f},Block::Slate,Brass);
    }
    out.box({-0.96f,0.19f,-0.055f},{0.96f,0.28f,0.055f},Block::Slate,Iron);
    out.box({-1.0f,1.12f,-0.105f},{1.0f,1.24f,0.105f},Block::Timber,wood);
    out.box({-1.0f,1.24f,-0.09f},{1.0f,1.28f,0.09f},Block::Planks,paint*1.12f);
    for (int i=0; i<7; ++i) {
        const float x=-0.72f+i*0.24f;
        out.box({x-0.025f,0.25f,-0.025f},{x+0.025f,1.14f,0.025f},Block::Slate,Iron*1.12f);
        if (i%2==1) {
            const glm::vec3 a{x,0.51f,0},b{x-0.09f,0.67f,0},c{x,0.83f,0},d{x+0.09f,0.67f,0};
            out.beam(a,b,0.035f,Block::Slate,Brass);
            out.beam(b,c,0.035f,Block::Slate,Brass);
            out.beam(c,d,0.035f,Block::Slate,Brass);
            out.beam(d,a,0.035f,Block::Slate,Brass);
        }
    }
}

} // namespace

void appendHubFence(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::TimberFence: timberFence(out,asset.variation); break;
    case HubAssetKind::PicketFence: picketFence(out,asset.variation); break;
    case HubAssetKind::FenceCorner: cornerFence(out,asset.variation); break;
    case HubAssetKind::GardenGate: gardenGate(mesh,asset); break;
    case HubAssetKind::IronRailing: ironRailing(out,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
