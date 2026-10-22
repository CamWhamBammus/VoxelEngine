#include "asset_builder.h"

#include <array>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

constexpr glm::vec3 Iron{0.12f,0.18f,0.19f};
constexpr glm::vec3 Rivet{0.43f,0.38f,0.25f};
constexpr glm::vec3 Mortar{0.25f,0.29f,0.28f};

glm::vec3 timber(std::uint32_t variant)
{
    constexpr std::array<glm::vec3,3> shades{{
        {0.53f,0.32f,0.16f}, {0.43f,0.29f,0.18f}, {0.58f,0.39f,0.20f}
    }};
    return shades[variant % shades.size()];
}

void pillar(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=timber(variant);
    // Matching end caps make this a repeatable one-block column segment.
    // Stack three for a tall porch instead of hiding a three-block height in
    // a single menu item.
    out.box({-0.25f,0,-0.25f},{0.25f,0.08f,0.25f},Block::Timber,wood*1.10f);
    out.box({-0.21f,0.08f,-0.21f},{0.21f,0.16f,0.21f},Block::Timber,wood*0.82f);
    out.box({-0.18f,0.14f,-0.18f},{0.18f,0.86f,0.18f},Block::Timber,wood);
    for (const float y : {0.18f,0.74f}) {
        out.box({-0.20f,y,-0.20f},{0.20f,y+0.08f,0.20f},Block::Slate,Iron);
        for (const float side : {-1.0f,1.0f}) {
            const float z=side*0.20f;
            out.box({-0.025f,y+0.02f,z-0.008f},{0.025f,y+0.06f,z+0.008f},Block::Slate,Rivet);
            const float x=side*0.20f;
            out.box({x-0.008f,y+0.02f,-0.025f},{x+0.008f,y+0.06f,0.025f},Block::Slate,Rivet);
        }
    }
    out.box({-0.21f,0.84f,-0.21f},{0.21f,0.92f,0.21f},Block::Timber,wood*0.82f);
    out.box({-0.25f,0.92f,-0.25f},{0.25f,1,0.25f},Block::Timber,wood*1.10f);
}

void timberBeam(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=timber(variant);
    out.box({-0.5f,0,-0.14f},{0.5f,0.25f,0.14f},Block::Timber,wood);
    for (const float x : {-0.36f,0.28f}) {
        out.box({x,0,-0.15f},{x+0.08f,0.25f,0.15f},Block::Slate,Iron);
        for (const float z : {-0.15f,0.15f})
            out.box({x+0.02f,0.10f,z-0.008f},{x+0.06f,0.15f,z+0.008f},Block::Slate,Rivet);
    }
}

void diagonalBrace(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=timber(variant);
    // The brace occupies the same centered cell as a straight beam; turning it
    // changes its direction without shifting it half a block off the grid.
    out.box({-0.5f,0,-0.13f},{-0.34f,1,0.13f},Block::Timber,wood*0.83f);
    out.box({-0.5f,0.84f,-0.13f},{0.5f,1,0.13f},Block::Timber,wood*0.92f);
    out.beam({-0.38f,0.14f,0},{0.36f,0.88f,0},0.18f,Block::Timber,wood*1.12f);
    for (const glm::vec2 p : {glm::vec2(-0.42f,0.18f),glm::vec2(0.32f,0.92f)}) {
        for (const float z : {-0.13f,0.13f})
            out.box({p.x-0.027f,p.y-0.027f,z-0.005f},
                    {p.x+0.027f,p.y+0.027f,z+0.005f},Block::Slate,Rivet);
    }
}

// Extrude a clockwise XY outline with a chipped front and back rim. The wide
// faces remain planar, while the bevel catches light along the masonry joints.
void masonry(AssetBuilder& out, std::array<glm::vec2,4> outline, glm::vec3 color)
{
    glm::vec2 center{};
    for (const auto p : outline) center+=p*0.25f;
    std::array<glm::vec2,8> rim;
    for (int i=0;i<4;++i) {
        rim[i*2]=glm::mix(outline[i],outline[(i+3)%4],0.06f);
        rim[i*2+1]=glm::mix(outline[i],outline[(i+1)%4],0.06f);
    }
    const auto point=[](glm::vec2 p,float z) { return glm::vec3(p.x,p.y,z); };
    for (int i=0;i<8;++i) {
        const int next=(i+1)%8;
        const auto a=rim[i], b=rim[next];
        const auto insetA=glm::mix(a,center,0.055f), insetB=glm::mix(b,center,0.055f);
        out.quad(point(b,-0.28f),point(a,-0.28f),point(a,0.28f),point(b,0.28f),Block::Limestone,color*0.76f);
        out.quad(point(b,0.28f),point(a,0.28f),point(insetA,0.33f),point(insetB,0.33f),Block::Limestone,color*0.93f);
        out.quad(point(a,-0.28f),point(b,-0.28f),point(insetB,-0.33f),point(insetA,-0.33f),Block::Limestone,color*0.88f);
        out.triangle(point(center,0.33f),point(insetB,0.33f),point(insetA,0.33f),Block::Limestone,color);
        out.triangle(point(center,-0.33f),point(insetA,-0.33f),point(insetB,-0.33f),Block::Limestone,color);
    }
}

void stoneDoorway(AssetBuilder& out, std::uint32_t variant)
{
    const glm::vec3 stone=variant%2 ? glm::vec3(0.50f,0.55f,0.48f) : glm::vec3(0.63f,0.60f,0.49f);
    for (const float side : {-1.0f,1.0f}) {
        const float left=side<0 ? -1.5f : 0.8f;
        // A recessed core keeps the jamb solid between the individual stones.
        out.box({left+0.025f,0,-0.25f},{left+0.675f,2.5f,0.25f},Block::Limestone,Mortar);
        for (int course=0;course<5;++course) {
            const float y=course*0.5f;
            masonry(out,{{{left,y},{left,y+0.49f},{left+0.7f,y+0.49f},{left+0.7f,y}}},
                    stone*(0.91f+0.035f*((course+variant)%4)));
        }
    }
    // Segment the arch instead of filling the opening with a rectangular wall.
    constexpr std::array<glm::vec2,6> outer{{
        {-1.5f,2.5f},{-1.05f,3.04f},{-0.38f,3.3f},
        {0.38f,3.3f},{1.05f,3.04f},{1.5f,2.5f}
    }};
    constexpr std::array<glm::vec2,6> inner{{
        {-0.8f,2.5f},{-0.57f,2.72f},{-0.20f,2.9f},
        {0.20f,2.9f},{0.57f,2.72f},{0.8f,2.5f}
    }};
    for (std::size_t i=0;i+1<outer.size();++i) {
        const float joint=i==0 ? 0 : 0.012f;
        const auto a=glm::mix(inner[i],inner[i+1],joint);
        const auto b=glm::mix(outer[i],outer[i+1],joint);
        const auto c=glm::mix(outer[i+1],outer[i],i+2==outer.size() ? 0 : 0.012f);
        const auto d=glm::mix(inner[i+1],inner[i],i+2==outer.size() ? 0 : 0.012f);
        masonry(out,{{a,b,c,d}},stone*(i==2 ? 1.13f : (i%2 ? 0.96f : 1.04f)));
    }
}

void windowFrame(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=timber(variant);
    out.box({-0.9f,0,0},{0.9f,0.12f,0.36f},Block::Timber,wood*1.10f);
    out.box({-0.8f,0.12f,0.025f},{0.8f,0.26f,0.27f},Block::Timber,wood);
    for (const float left : {-0.8f,0.64f})
        out.box({left,0.12f,0.025f},{left+0.16f,1.55f,0.27f},Block::Timber,wood);
    out.box({-0.86f,1.47f,0.015f},{0.86f,1.65f,0.29f},Block::Timber,wood*1.06f);
    for (const float x : {-0.22f,0.22f})
        out.box({x-0.018f,0.25f,0.11f},{x+0.018f,1.48f,0.145f},Block::Slate,Iron);
    out.box({-0.65f,0.84f,0.11f},{0.65f,0.88f,0.145f},Block::Slate,Iron);
    for (const float x : {-0.73f,0.73f}) {
        out.box({x-0.045f,1.48f,0.29f},{x+0.045f,1.64f,0.305f},Block::Slate,Iron);
        out.box({x-0.022f,1.54f,0.303f},{x+0.022f,1.58f,0.32f},Block::Slate,Rivet);
    }
}

void woodenStairs(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=timber(variant);
    for (int step=0;step<6;++step) {
        const float front=1.5f-step*0.5f, back=front-0.5f, height=(step+1)*0.25f;
        // Each closed riser reaches the floor; no hollow faces or floating treads.
        out.box({-1,0,back},{1,height-0.045f,front},Block::Timber,wood*0.80f);
        out.box({-1,height-0.045f,back},{1,height,front},Block::Planks,
                wood*(step%2 ? 1.10f : 1.02f));
    }
}

glm::vec3 barrelPoint(int corner, float y, float radius)
{
    constexpr float Pi=3.14159265358979323846f;
    const float angle=corner*Pi*0.25f;
    return {std::cos(angle)*radius,y,std::sin(angle)*radius};
}

void barrelHoop(AssetBuilder& out, float bottom, float top, float lower, float upper)
{
    for (int side=0;side<8;++side) {
        const auto a=barrelPoint(side,bottom,lower), b=barrelPoint(side+1,bottom,lower);
        const auto c=barrelPoint(side+1,top,upper), d=barrelPoint(side,top,upper);
        const auto ai=barrelPoint(side,bottom,lower-0.025f), bi=barrelPoint(side+1,bottom,lower-0.025f);
        const auto ci=barrelPoint(side+1,top,upper-0.025f), di=barrelPoint(side,top,upper-0.025f);
        out.quad(b,a,d,c,Block::Slate,Iron);
        out.quad(a,b,bi,ai,Block::Slate,Iron*0.75f);
        out.quad(d,di,ci,c,Block::Slate,Iron*1.22f);
        out.quad(ai,bi,ci,di,Block::Slate,Iron*0.8f);
    }
}

void barrel(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=timber(variant);
    constexpr std::array<float,5> heights{{0,0.18f,0.65f,1.12f,1.30f}};
    constexpr std::array<float,5> radii{{0.44f,0.48f,0.52f,0.48f,0.44f}};
    for (int side=0;side<8;++side) {
        for (std::size_t ring=0;ring+1<heights.size();++ring) {
            const auto a=barrelPoint(side,heights[ring],radii[ring]);
            const auto b=barrelPoint(side+1,heights[ring],radii[ring]);
            const auto c=barrelPoint(side+1,heights[ring+1],radii[ring+1]);
            const auto d=barrelPoint(side,heights[ring+1],radii[ring+1]);
            // Two staves on every broad octagonal facet, divided by a dark joint.
            for (int stave=0;stave<2;++stave) {
                const float left=stave==0 ? 0 : 0.515f, right=stave==0 ? 0.485f : 1;
                out.quad(glm::mix(b,a,left),glm::mix(b,a,right),glm::mix(c,d,right),glm::mix(c,d,left),
                         Block::Timber,wood*(0.88f+0.045f*((side*2+stave+variant)%4)));
            }
            out.quad(glm::mix(b,a,0.485f),glm::mix(b,a,0.515f),glm::mix(c,d,0.515f),glm::mix(c,d,0.485f),
                     Block::Timber,wood*0.48f);
        }
        out.triangle({0,0,0},barrelPoint(side,0,0.44f),barrelPoint(side+1,0,0.44f),Block::Timber,wood*0.7f);
        out.triangle({0,1.30f,0},barrelPoint(side+1,1.30f,0.44f),barrelPoint(side,1.30f,0.44f),Block::Planks,wood*1.10f);
    }
    barrelHoop(out,0.12f,0.25f,0.482f,0.504f);
    barrelHoop(out,1.05f,1.18f,0.504f,0.482f);
    // Lid battens remain inside its rim and give the top a readable silhouette.
    for (const float x : {-0.25f,0.17f})
        out.box({x,1.30f,-0.30f},{x+0.08f,1.35f,0.30f},Block::Timber,wood*0.8f);
}

} // namespace

void appendHubBuildingKit(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::TimberPillar: pillar(out,asset.variation); break;
    case HubAssetKind::TimberBeam: timberBeam(out,asset.variation); break;
    case HubAssetKind::DiagonalBrace: diagonalBrace(out,asset.variation); break;
    case HubAssetKind::StoneDoorway: stoneDoorway(out,asset.variation); break;
    case HubAssetKind::WindowFrame: windowFrame(out,asset.variation); break;
    case HubAssetKind::WoodenStairs: woodenStairs(out,asset.variation); break;
    case HubAssetKind::Barrel: barrel(out,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
