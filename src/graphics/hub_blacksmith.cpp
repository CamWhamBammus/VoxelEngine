#include "asset_builder.h"

#include <array>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

constexpr glm::vec3 Iron{0.12f,0.17f,0.20f};
constexpr glm::vec3 Steel{0.43f,0.53f,0.57f};
constexpr glm::vec3 Oak{0.48f,0.29f,0.13f};
constexpr glm::vec3 Soot{0.055f,0.065f,0.07f};
constexpr glm::vec3 Copper{0.68f,0.38f,0.16f};

// Clockwise XZ corners when seen from below give an upward-facing top.
void taper(AssetBuilder& out, glm::vec3 bottom, glm::vec3 top,
           glm::vec2 lower, glm::vec2 upper, Block material, glm::vec3 color)
{
    constexpr glm::vec2 corners[]{{-1,-1},{-1,1},{1,1},{1,-1}};
    std::array<glm::vec3,4> a, b;
    for (int i=0;i<4;++i) {
        a[i]=bottom+glm::vec3(corners[i].x*lower.x,0,corners[i].y*lower.y);
        b[i]=top+glm::vec3(corners[i].x*upper.x,0,corners[i].y*upper.y);
    }
    for (int i=0;i<4;++i) {
        const int next=(i+1)%4;
        out.quad(a[i],a[next],b[next],b[i],material,color*(i%2 ? 0.92f : 1.0f));
    }
    out.quad(a[3],a[2],a[1],a[0],material,color*0.75f);
    out.quad(b[0],b[1],b[2],b[3],material,color);
}

void ironAnvil(AssetBuilder& out, glm::vec3 origin, float scale, glm::vec3 color)
{
    const auto p=[&](glm::vec3 point) { return origin+point*scale; };
    taper(out,p({0,0,0}),p({0,0.12f,0}),{0.66f*scale,0.38f*scale},
          {0.54f*scale,0.31f*scale},Block::Slate,color*0.72f);
    taper(out,p({0,0.12f,0}),p({0,0.36f,0}),{0.43f*scale,0.29f*scale},
          {0.28f*scale,0.22f*scale},Block::Slate,color*0.80f);
    taper(out,p({0,0.36f,0}),p({0,0.50f,0}),{0.28f*scale,0.22f*scale},
          {0.66f*scale,0.28f*scale},Block::Slate,color);
    out.box(p({-0.66f,0.50f,-0.28f}),p({0.78f,0.68f,0.28f}),Block::Slate,color*1.12f);
    out.box(p({0.78f,0.53f,-0.23f}),p({1.00f,0.67f,0.23f}),Block::Slate,color);
    // Four broad facets form a true tapered horn, not another cuboid.
    const glm::vec3 ring[]{{-0.66f,0.50f,-0.26f},{-0.66f,0.50f,0.26f},
                          {-0.66f,0.67f,0.26f},{-0.66f,0.67f,-0.26f}};
    for (int i=0;i<4;++i)
        out.triangle(p(ring[i]),p(ring[(i+1)%4]),p({-1.28f,0.62f,0}),Block::Slate,color*(i==2 ? 1.15f : 0.86f));
    // A small dark hardy hole and polished face make the tool recognizable.
    out.quad(p({0.49f,0.681f,-0.10f}),p({0.49f,0.681f,0.10f}),
             p({0.67f,0.681f,0.10f}),p({0.67f,0.681f,-0.10f}),Block::Slate,Soot);
}

void forge(AssetBuilder& out, std::uint32_t variant)
{
    const auto stone=glm::vec3(0.34f,0.29f,0.26f)*(0.93f+0.035f*(variant%3));
    // These courses enclose the scene's solid three-by-three collision base.
    out.box({-1.51f,0,-1.51f},{1.51f,1.02f,1.51f},Block::Brick,stone);
    for (int course=0;course<3;++course) {
        const float y=0.04f+course*0.32f;
        for (int brick=0;brick<5;++brick) {
            const float x=-1.49f+brick*0.6f;
            out.box({x,y,1.512f},{x+0.58f,y+0.29f,1.55f},Block::Brick,stone*(0.9f+0.045f*((brick+course)%4)));
        }
    }
    out.box({-1.55f,1.02f,-1.52f},{1.55f,1.16f,1.55f},Block::Slate,Iron);
    out.box({-1.26f,1.16f,-0.98f},{1.26f,1.20f,1.16f},Block::CoalOre,Soot);
    // Dark coal surrounds small amber-hot facets; this uses the normal lantern
    // emission and one matching asset light, with no extra fake fire billboard.
    for (int row=0;row<3;++row) for (int lump=0;lump<5;++lump) {
        const float x=-0.99f+lump*0.49f, z=-0.62f+row*0.53f;
        const bool glowing=(row+lump+variant)%3!=0;
        taper(out,{x,1.20f,z},{x+0.025f,1.31f+0.035f*((lump+row)%3),z},
              {0.23f,0.24f},{0.15f,0.16f},glowing ? Block::Lantern : Block::CoalOre,
              glowing ? glm::vec3(1.0f,0.31f+0.05f*((lump+row)%3),0.035f) : Soot);
    }
    out.box({-1.50f,1.16f,-1.48f},{1.50f,2.72f,-1.21f},Block::Brick,stone*0.7f);
    for (const float side : {-1.0f,1.0f}) {
        const float x=side<0 ? -1.50f : 1.22f;
        out.box({x,1.16f,-1.25f},{x+0.28f,2.68f,1.40f},Block::Brick,stone);
        out.box({x-0.02f,2.52f,-1.30f},{x+0.30f,2.70f,1.43f},Block::Slate,Iron);
    }
    taper(out,{0,2.68f,-0.02f},{0,4.0f,-0.75f},{1.57f,1.48f},{0.94f,0.69f},Block::Slate,Iron*0.72f);
    out.box({-1.59f,2.61f,1.37f},{1.59f,2.78f,1.52f},Block::Slate,Copper*0.64f);
    for (const float x : {-1.31f,-0.65f,0.0f,0.65f,1.31f})
        out.box({x-0.035f,2.64f,1.525f},{x+0.035f,2.72f,1.56f},Block::Slate,Copper);
}

void anvil(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=Oak*(0.88f+0.055f*(variant%3));
    out.box({-0.53f,0,-0.53f},{0.53f,1.01f,0.53f},Block::Timber,wood);
    for (const float y : {0.12f,0.78f}) {
        out.box({-0.55f,y,-0.55f},{0.55f,y+0.11f,0.55f},Block::Slate,Iron);
        for (const float x : {-0.38f,0.38f})
            out.box({x-0.035f,y+0.025f,0.552f},{x+0.035f,y+0.085f,0.58f},Block::Slate,Copper);
    }
    ironAnvil(out,{0,1.01f,0},1.0f,Steel*(0.92f+0.025f*(variant%3)));
}

void toolRack(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=Oak*(0.85f+0.05f*(variant%3));
    for (const float y : {0.18f,1.62f})
        out.box({-2.28f,y,0},{2.28f,y+0.23f,0.18f},Block::Timber,wood);
    for (const float x : {-2.13f,2.13f})
        out.box({x-0.07f,0.08f,0.02f},{x+0.07f,1.98f,0.23f},Block::Slate,Iron);
    for (int tool=0;tool<4;++tool) {
        const float x=-1.64f+tool*1.08f;
        out.beam({x,0.38f,0.28f},{x,1.72f,0.28f},0.105f,Block::Timber,Oak);
        out.beam({x,1.70f,0.18f},{x,1.70f,0.43f},0.07f,Block::Slate,Copper);
        if (tool%2==0) {
            out.box({x-0.32f,1.45f,0.21f},{x+0.32f,1.76f,0.50f},Block::Slate,tool==0 ? Steel : Copper);
        } else {
            out.beam({x-0.49f,1.40f,0.34f},{x,1.73f,0.34f},0.14f,Block::Slate,Steel);
            out.beam({x,1.73f,0.34f},{x+0.49f,1.48f,0.34f},0.13f,Block::Slate,Steel);
        }
    }
}

void workbench(AssetBuilder& out, std::uint32_t variant)
{
    const auto wood=Oak*(0.89f+0.04f*(variant%3));
    out.box({-2.52f,0,-0.52f},{2.52f,1.02f,0.52f},Block::Timber,wood*0.73f);
    for (int board=0;board<10;++board) {
        const float x=-2.5f+board*0.5f;
        out.box({x,0.16f,0.525f},{x+0.48f,0.90f,0.57f},Block::Planks,wood*(board%2 ? 0.94f : 1.0f));
    }
    out.box({-2.60f,1.02f,-0.62f},{2.60f,1.18f,0.65f},Block::Planks,wood*1.12f);
    for (const float x : {-2.34f,0.0f,2.34f})
        out.box({x-0.055f,0.02f,0.575f},{x+0.055f,1.00f,0.63f},Block::Slate,Iron);
    for (int row=0;row<2;++row) for (int bar=0;bar<3-row;++bar) {
        const float x=0.25f+bar*0.61f+row*0.30f, y=1.18f+row*0.19f;
        taper(out,{x,y,0.02f},{x,y+0.18f,0.02f},{0.27f,0.31f},{0.22f,0.25f},Block::Slate,row ? Copper : Steel);
    }
    out.beam({-1.98f,1.25f,0.28f},{-1.13f,1.25f,-0.18f},0.105f,Block::Timber,Oak*1.15f);
    out.box({-1.35f,1.20f,-0.41f},{-0.92f,1.47f,-0.07f},Block::Slate,Steel);
    out.beam({-0.75f,1.23f,0.43f},{-0.27f,1.23f,-0.34f},0.055f,Block::Slate,Iron);
    out.beam({-0.98f,1.23f,0.43f},{-0.45f,1.23f,-0.34f},0.055f,Block::Slate,Iron);
}

void sign(AssetBuilder& out, std::uint32_t variant)
{
    const glm::vec3 paint=variant%2 ? glm::vec3(0.22f,0.14f,0.28f) : glm::vec3(0.08f,0.26f,0.28f);
    out.box({-0.12f,-0.48f,-0.02f},{0.12f,0.13f,0.15f},Block::Slate,Iron);
    out.beam({0,0,0.08f},{0,0,1.55f},0.11f,Block::Slate,Iron);
    out.beam({0,-0.38f,0.10f},{0,-0.02f,1.12f},0.07f,Block::Slate,Iron);
    out.beam({-0.72f,-0.18f,1.45f},{0.72f,-0.18f,1.45f},0.07f,Block::Slate,Iron);
    for (const float x : {-0.65f,0.65f})
        out.beam({x,-0.18f,1.45f},{x,-0.66f,1.45f},0.045f,Block::Slate,Copper);
    out.box({-1.05f,-1.91f,1.32f},{1.05f,-0.63f,1.48f},Block::Planks,paint);
    for (const float y : {-1.92f,-0.67f})
        out.box({-1.08f,y,1.30f},{1.08f,y+0.07f,1.51f},Block::Slate,Copper);
    ironAnvil(out,{0,-1.72f,1.61f},0.70f,{0.74f,0.49f,0.22f});
}

} // namespace

void appendHubBlacksmith(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::Forge: forge(out,asset.variation); break;
    case HubAssetKind::Anvil: anvil(out,asset.variation); break;
    case HubAssetKind::ToolRack: toolRack(out,asset.variation); break;
    case HubAssetKind::SmithWorkbench: workbench(out,asset.variation); break;
    case HubAssetKind::AnvilSign: sign(out,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
