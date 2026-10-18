#include "asset_builder.h"

#include <array>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

constexpr glm::vec3 Timber{0.32f, 0.17f, 0.08f};
constexpr glm::vec3 Oak{0.61f, 0.39f, 0.19f};
constexpr glm::vec3 Iron{0.08f, 0.14f, 0.16f};
constexpr glm::vec3 Copper{0.56f, 0.32f, 0.15f};
constexpr glm::vec3 Glass{0.16f, 0.31f, 0.36f};

glm::vec3 roofColor(std::uint32_t variation)
{
    constexpr std::array<glm::vec3, 3> colors{{
        {0.10f,0.35f,0.38f}, {0.27f,0.20f,0.34f}, {0.27f,0.37f,0.21f}
    }};
    return colors[variation % colors.size()];
}

// Four corners run around an upward-facing surface. A small vertical thickness
// gives roofs visible eaves and an underside instead of a single paper plane.
void roofPanel(AssetBuilder& out, const std::array<glm::vec3,4>& top,
               float thickness, glm::vec3 color)
{
    std::array<glm::vec3,4> bottom;
    for (int i=0; i<4; ++i) bottom[i] = top[i] - glm::vec3(0,thickness,0);
    out.quad(top[0],top[1],top[2],top[3],Block::RoofTiles,color);
    out.quad(bottom[3],bottom[2],bottom[1],bottom[0],Block::Timber,Timber);
    for (int i=0; i<4; ++i) {
        const int next=(i+1)%4;
        out.quad(top[next],top[i],bottom[i],bottom[next],Block::RoofTiles,color*0.8f);
    }
}

void pitchedRoof(AssetBuilder& out, float halfWidth, float eave, float ridge,
                 float back, float front, glm::vec3 color)
{
    roofPanel(out, {{{-halfWidth,eave,back},{-halfWidth,eave,front},
                     {0,ridge,front},{0,ridge,back}}},0.10f,color);
    roofPanel(out, {{{0,ridge,back},{0,ridge,front},
                     {halfWidth,eave,front},{halfWidth,eave,back}}},0.10f,color*0.91f);
}

// A clipped, three-sided bay projects outward from the facade. Its fourth side
// lies against the building; the polygon order also gives upward-facing caps.
constexpr std::array<glm::vec2,4> Bay{{{-1.12f,0.04f},{-0.76f,1.02f},
                                     {0.76f,1.02f},{1.12f,0.04f}}};

void bayBand(AssetBuilder& out, float bottom, float top, Block material, glm::vec3 color)
{
    const auto point=[](glm::vec2 p,float y) { return glm::vec3(p.x,y,p.y); };
    for (int i=0; i<4; ++i) {
        const int next=(i+1)%4;
        out.quad(point(Bay[i],bottom),point(Bay[next],bottom),
                 point(Bay[next],top),point(Bay[i],top),material,color);
    }
    out.quad(point(Bay[0],top),point(Bay[1],top),point(Bay[2],top),point(Bay[3],top),material,color);
    out.quad(point(Bay[3],bottom),point(Bay[2],bottom),point(Bay[1],bottom),point(Bay[0],bottom),material,color*0.8f);
}

void dormer(AssetBuilder& out, std::uint32_t variation)
{
    const auto roof=roofColor(variation);
    out.box({-1.05f,0,-0.42f},{1.05f,1.55f,0.87f},Block::Planks,Oak*0.85f);
    out.triangle({-1.05f,1.55f,0.87f},{1.05f,1.55f,0.87f},{0,2.27f,0.87f},Block::Planks,Oak);
    out.triangle({1.05f,1.55f,-0.42f},{-1.05f,1.55f,-0.42f},{0,2.27f,-0.42f},Block::Planks,Oak);
    // Cool opaque glass is painted Slate, never an emissive lantern material.
    for (int x=0; x<2; ++x) for (int y=0; y<2; ++y) {
        const float left=-0.76f+x*0.80f, bottom=0.29f+y*0.56f;
        out.box({left,bottom,0.88f},{left+0.72f,bottom+0.48f,0.92f},Block::Slate,
                Glass*(1.0f+0.06f*((x+y+variation)%3)));
    }
    for (const float x : {-0.96f,0.96f})
        out.box({x-0.09f,0.08f,0.88f},{x+0.09f,1.56f,1.06f},Block::Timber,Timber);
    for (const float y : {0.10f,1.45f})
        out.box({-1.08f,y,0.87f},{1.08f,y+0.14f,1.09f},Block::Timber,Oak);
    out.box({-0.045f,0.25f,0.92f},{0.045f,1.45f,1.00f},Block::Timber,Oak);
    out.box({-0.85f,0.77f,0.92f},{0.85f,0.85f,1.00f},Block::Timber,Oak);
    out.beam({0,1.55f,0.94f},{0,2.19f,0.94f},0.11f,Block::Timber,Timber);
    pitchedRoof(out,1.34f,1.49f,2.36f,-0.60f,1.16f,roof);
    for (const float side : {-1.0f,1.0f}) {
        out.beam({side*1.27f,1.45f,1.08f},{0,2.36f,1.08f},0.11f,Block::Timber,Oak);
        out.box({side<0 ? -1.36f : 1.17f,1.33f,-0.57f},
                {side<0 ? -1.17f : 1.36f,1.48f,1.18f},Block::Timber,Timber);
    }
    out.beam({0,2.36f,-0.56f},{0,2.36f,1.13f},0.10f,Block::RoofTiles,roof*1.15f);
}

void bayWindow(AssetBuilder& out, std::uint32_t variation)
{
    for (const float x : {-0.65f,0.65f})
        out.beam({x,0.09f,0.08f},{x,0.48f,0.90f},0.12f,Block::Timber,Timber);
    bayBand(out,0.45f,0.62f,Block::Timber,Oak);
    bayBand(out,0.62f,0.88f,Block::Planks,Timber);
    for (int side=0; side<3; ++side) {
        const auto a=Bay[side], b=Bay[side+1];
        out.quad({a.x,0.88f,a.y},{b.x,0.88f,b.y},
                 {b.x,2.04f,b.y},{a.x,2.04f,a.y},Block::Slate,Glass*(0.94f+side*0.07f),true);
        const auto middle=(a+b)*0.5f;
        out.beam({middle.x,0.84f,middle.y},{middle.x,2.08f,middle.y},0.065f,Block::Timber,Oak);
        out.beam({a.x,1.48f,a.y},{b.x,1.48f,b.y},0.06f,Block::Timber,Oak);
    }
    for (const auto p : Bay)
        out.box({p.x-0.06f,0.57f,p.y-0.06f},{p.x+0.06f,2.13f,p.y+0.06f},Block::Timber,Timber);
    bayBand(out,2.04f,2.18f,Block::Timber,Oak);
    // A broad hip cap keeps the bay distinct from the dormer's pointed gable.
    constexpr std::array<glm::vec3,4> low{{{-1.28f,2.13f,0.01f},{-0.84f,2.13f,1.18f},
                                         {0.84f,2.13f,1.18f},{1.28f,2.13f,0.01f}}};
    constexpr std::array<glm::vec3,4> high{{{-0.93f,2.45f,0.01f},{-0.60f,2.45f,0.81f},
                                          {0.60f,2.45f,0.81f},{0.93f,2.45f,0.01f}}};
    const auto roof=roofColor(variation);
    for (int i=0; i<4; ++i) {
        const int next=(i+1)%4;
        out.quad(low[i],low[next],high[next],high[i],Block::RoofTiles,roof*(i%2==0 ? 1.0f : 0.9f));
    }
    out.quad(high[0],high[1],high[2],high[3],Block::RoofTiles,roof);
    out.quad(low[3],low[2],low[1],low[0],Block::Timber,Timber);
}

void chimney(AssetBuilder& out, std::uint32_t variation)
{
    const glm::vec3 stone=variation%2==0 ? glm::vec3(0.57f,0.53f,0.43f) : glm::vec3(0.44f,0.48f,0.47f);
    for (int course=0; course<7; ++course) {
        const float y=course*0.35f, half=0.49f+0.025f*((variation+course)%3);
        const float joint=((variation+course)%2==0 ? -0.13f : 0.15f);
        const auto paint=stone*(0.90f+0.045f*((variation+course*7)%4));
        if (course%2==0) {
            out.box({-half,y,-half},{joint-0.012f,y+0.34f,half},Block::Limestone,paint);
            out.box({joint+0.012f,y,-half},{half,y+0.34f,half},Block::Limestone,paint*1.06f);
        } else {
            out.box({-half,y,-half},{half,y+0.34f,joint-0.012f},Block::Limestone,paint);
            out.box({-half,y,joint+0.012f},{half,y+0.34f,half},Block::Limestone,paint*1.06f);
        }
    }
    for (const float x : {-0.62f,0.44f})
        out.box({x,2.40f,-0.62f},{x+0.18f,2.66f,0.62f},Block::Slate,stone*0.73f);
    for (const float z : {-0.62f,0.44f})
        out.box({-0.44f,2.40f,z},{0.44f,2.66f,z+0.18f},Block::Slate,stone*0.73f);
    out.quad({-0.44f,2.49f,-0.44f},{-0.44f,2.49f,0.44f},
             {0.44f,2.49f,0.44f},{0.44f,2.49f,-0.44f},Block::Slate,{0.035f,0.045f,0.05f});
    for (const float x : {-0.47f,0.47f}) for (const float z : {-0.47f,0.47f})
        out.box({x-0.03f,2.62f,z-0.03f},{x+0.03f,3.08f,z+0.03f},Block::Slate,Iron);
    pitchedRoof(out,0.64f,3.05f,3.40f,-0.64f,0.64f,Iron);
}

void balcony(AssetBuilder& out, std::uint32_t variation)
{
    const auto wood=Oak*(variation%2==0 ? 1.0f : 0.88f);
    for (int board=0; board<6; ++board) {
        const float x=-1.94f+board*0.65f;
        out.box({x,-0.10f,0.01f},{x+0.63f,0.03f,1.32f},Block::Planks,wood*(board%2==0 ? 1.0f : 0.94f));
    }
    out.box({-1.98f,-0.22f,1.23f},{1.98f,0.04f,1.39f},Block::Timber,Timber);
    for (const float x : {-1.96f,1.80f})
        out.box({x,-0.20f,0.01f},{x+0.16f,0.04f,1.36f},Block::Timber,Timber);
    for (const float x : {-1.86f,-0.64f,0.64f,1.86f}) {
        out.box({x-0.06f,0,1.23f},{x+0.06f,1.18f,1.35f},Block::Timber,Timber);
        out.box({x-0.08f,1.13f,1.21f},{x+0.08f,1.24f,1.37f},Block::Slate,Copper);
    }
    out.box({-1.98f,1.08f,1.22f},{1.98f,1.20f,1.38f},Block::Timber,wood);
    for (const float x : {-1.94f,1.82f})
        out.box({x,1.08f,0.02f},{x+0.12f,1.20f,1.35f},Block::Timber,wood);
    for (int rail=0; rail<8; ++rail) {
        const float x=-1.62f+rail*0.46f;
        out.box({x-0.018f,0.05f,1.27f},{x+0.018f,1.10f,1.31f},Block::Slate,Iron);
    }
    for (const float x : {-1.88f,1.88f})
        out.box({x-0.018f,0.05f,0.63f},{x+0.018f,1.10f,0.67f},Block::Slate,Iron);
    for (const float x : {-1.30f,1.30f})
        out.beam({x,-0.57f,0.08f},{x,-0.16f,1.19f},0.12f,Block::Timber,Timber);
}

void porchCanopy(AssetBuilder& out, std::uint32_t variation)
{
    const auto roof=roofColor(variation);
    // Three broad courses on each slope break up the roof without tiny shingles.
    for (int side=0; side<2; ++side) for (int course=0; course<3; ++course) {
        const float x0=-2.13f+(side*3+course)*0.71f, x1=x0+0.71f;
        const auto height=[](float x) { return 1.10f-std::abs(x)*0.40f; };
        roofPanel(out, {{{x0,height(x0),0.03f},{x0,height(x0),2.13f},
                         {x1,height(x1),2.13f},{x1,height(x1),0.03f}}},
                  0.10f,roof*(1.0f-0.045f*((course+variation)%3)));
    }
    for (const float side : {-1.0f,1.0f}) {
        out.beam({side*2.10f,0.23f,2.08f},{0,1.10f,2.08f},0.13f,Block::Timber,Oak);
        const float x=side*1.65f;
        out.box({x-0.10f,-0.58f,0.01f},{x+0.10f,0.25f,0.17f},Block::Timber,Timber);
        out.beam({x,-0.48f,0.12f},{x,0.15f,1.64f},0.13f,Block::Timber,Oak);
        out.box({x-0.11f,-0.34f,0.17f},{x+0.11f,-0.22f,0.20f},Block::Slate,Iron);
        const float edge=side<0 ? -2.18f : 2.02f;
        out.box({edge,0.09f,0.03f},{edge+0.16f,0.27f,2.16f},Block::Timber,Timber);
    }
    out.beam({0,1.10f,0.04f},{0,1.10f,2.15f},0.12f,Block::RoofTiles,roof*1.12f);
    out.box({-1.78f,0.03f,1.72f},{1.78f,0.18f,1.90f},Block::Timber,Timber);
}

} // namespace

void appendHubArchitecture(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::Dormer: dormer(out,asset.variation); break;
    case HubAssetKind::BayWindow: bayWindow(out,asset.variation); break;
    case HubAssetKind::Chimney: chimney(out,asset.variation); break;
    case HubAssetKind::Balcony: balcony(out,asset.variation); break;
    case HubAssetKind::PorchCanopy: porchCanopy(out,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
