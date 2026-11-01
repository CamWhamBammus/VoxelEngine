#include "asset_builder.h"
#include "jungle_shapes.h"

#include <array>

namespace voxel {
namespace {

constexpr glm::vec3 Oak{.40f,.23f,.095f};
constexpr glm::vec3 Bark{.24f,.13f,.055f};
constexpr glm::vec3 Brass{.69f,.43f,.14f};
constexpr glm::vec3 Iron{.14f,.17f,.16f};
constexpr glm::vec3 Stone{.40f,.43f,.30f};

void bevel(AssetBuilder& out, glm::vec3 lo, glm::vec3 hi, float edge,
           Block material, glm::vec3 color)
{
    out.box(lo,hi-glm::vec3(0,edge,0),material,color*.88f);
    const glm::vec3 lower[]{{lo.x,hi.y-edge,lo.z},{lo.x,hi.y-edge,hi.z},
        {hi.x,hi.y-edge,hi.z},{hi.x,hi.y-edge,lo.z}};
    const glm::vec3 upper[]{{lo.x+edge,hi.y,lo.z+edge},{lo.x+edge,hi.y,hi.z-edge},
        {hi.x-edge,hi.y,hi.z-edge},{hi.x-edge,hi.y,lo.z+edge}};
    for (int i=0;i<4;++i) out.quad(lower[i],lower[(i+1)%4],upper[(i+1)%4],upper[i],material,color);
    out.quad(upper[0],upper[1],upper[2],upper[3],material,color*1.08f);
}

void heartSeed(AssetBuilder& out)
{
    // Separate masonry courses leave actual recessed mortar under the root bowl.
    bevel(out,{-1.72f,0,-1.72f},{1.72f,.25f,1.72f},.07f,Block::Slate,Stone*.74f);
    for (int course=0;course<2;++course) for (int side=0;side<4;++side) {
        for (int brick=0;brick<4;++brick) {
            const float along=-1.58f+float(brick)*.79f;
            const float y=.25f+float(course)*.31f;
            glm::vec3 low{along+.015f,y,-1.60f},high{along+.77f,y+.29f,-1.22f};
            const auto turn=[&](glm::vec3 p) {
                for (int i=0;i<side;++i) p={p.z,p.y,-p.x};
                return p;
            };
            const auto a=turn(low), b=turn(high);
            bevel(out,glm::min(a,b),glm::max(a,b),.035f,Block::Brick,
                Stone*(.87f+.045f*float((course+brick+side)%4)));
        }
    }
    bevel(out,{-1.76f,.87f,-1.76f},{1.76f,1.02f,1.76f},.06f,Block::Path,Stone*1.15f);
    // Eight rooted supports cradle the seed; the diagonal grain is geometry.
    for (int arm=0;arm<8;++arm) {
        const float angle=float(arm)*jungle::Pi/4;
        const glm::vec3 radial{std::cos(angle),0,std::sin(angle)};
        const auto foot=radial*1.28f+glm::vec3(0,1.02f,0);
        const auto knee=radial*.69f+glm::vec3(0,1.43f,0);
        const auto tip=radial*.96f+glm::vec3(0,2.05f,0);
        jungle::stem(out,foot,knee,.22f,.21f,Bark);
        jungle::stem(out,knee,tip,.21f,.095f,Oak);
        jungle::stem(out,foot+radial*.15f,knee+radial*.13f,.055f,.035f,Oak*1.2f);
        out.box(tip-glm::vec3(.12f,.09f,.12f),tip+glm::vec3(.12f,.06f,.12f),Block::Slate,Brass);
    }
    bevel(out,{-.78f,1.43f,-.78f},{.78f,1.62f,.78f},.09f,Block::Timber,Oak);

    // A broad faceted amber seed tapers to a point. Narrow brass seams break up
    // the glow so the silhouette still reads as a crystal in a brightly lit room.
    constexpr std::array<float,6> heights{1.62f,1.91f,2.38f,2.88f,3.32f,3.58f};
    constexpr std::array<float,6> radii{.43f,.77f,.88f,.71f,.40f,.06f};
    std::array<std::array<glm::vec3,8>,6> ring{};
    for (int row=0;row<6;++row) for (int side=0;side<8;++side) {
        const float angle=float(side)*jungle::Pi/4;
        ring[row][side]={std::cos(angle)*radii[row],heights[row],std::sin(angle)*radii[row]};
    }
    for (int row=0;row<5;++row) for (int side=0;side<8;++side) {
        const int next=(side+1)%8;
        const glm::vec3 amber{1.0f,.45f+.055f*float((side+row)%4),.035f};
        out.quad(ring[row][side],ring[row][next],ring[row+1][next],ring[row+1][side],Block::Lantern,amber);
        out.beam(ring[row][side],ring[row+1][side],.027f,Block::Slate,Brass*1.25f);
        if (row>0) out.beam(ring[row][side],ring[row][next],.025f,Block::Slate,Brass*1.25f);
    }
    out.quad(ring[5][0],ring[5][2],ring[5][4],ring[5][6],Block::Lantern,{1.f,.81f,.22f});
    for (int leaf=0;leaf<8;++leaf) {
        const float angle=float(leaf)*jungle::Pi/4;
        const glm::vec3 base{std::cos(angle)*1.4f,1.03f,std::sin(angle)*1.4f};
        jungle::leaf(out,base,base+glm::vec3(std::cos(angle)*.23f,.18f,std::sin(angle)*.23f),.12f,{.28f,.42f,.10f});
    }
}

void chainLantern(AssetBuilder& out, std::uint32_t variation)
{
    const float center=-heartwoodLanternDrop(variation);
    // The first plate meets the ceiling. Alternating rectangular links connect
    // continuously to the lifting ring instead of leaving the light suspended.
    out.box({-.14f,-.06f,-.14f},{.14f,.025f,.14f},Block::Slate,Iron);
    for (float y=-.08f;y>center+.76f;y-=.19f) {
        const bool alongX=int(std::lround(-y/.19f))%2==0;
        const glm::vec3 span=alongX ? glm::vec3(.085f,0,0) : glm::vec3(0,0,.085f);
        const glm::vec3 top{0,y,0},bottom{0,std::max(y-.22f,center+.72f),0};
        out.beam(top-span,top+span,.035f,Block::Slate,Iron);
        out.beam(bottom-span,bottom+span,.035f,Block::Slate,Iron);
        out.beam(top-span,bottom-span,.035f,Block::Slate,Iron);
        out.beam(top+span,bottom+span,.035f,Block::Slate,Iron);
    }
    const auto p=[&](glm::vec3 v) {return v+glm::vec3(0,center,0);};
    out.beam(p({-.10f,.54f,0}),p({-.10f,.75f,0}),.055f,Block::Slate,Brass);
    out.beam(p({.10f,.54f,0}),p({.10f,.75f,0}),.055f,Block::Slate,Brass);
    out.beam(p({-.10f,.75f,0}),p({.10f,.75f,0}),.055f,Block::Slate,Brass);
    bevel(out,p({-.45f,-.51f,-.45f}),p({.45f,-.38f,.45f}),.05f,Block::Slate,Brass);
    out.box(p({-.34f,-.38f,-.34f}),p({.34f,.38f,.34f}),Block::Lantern,{1.f,.70f,.26f});
    for (const float x:{-.38f,.38f}) for (const float z:{-.38f,.38f})
        out.box(p({x-.045f,-.41f,z-.045f}),p({x+.045f,.43f,z+.045f}),Block::Timber,Oak);
    for (const float side:{-1.f,1.f}) {
        out.box(p({-.035f,-.38f,side*.355f-.025f}),p({.035f,.38f,side*.355f+.025f}),Block::Slate,Brass*.7f);
        out.box(p({side*.355f-.025f,-.38f,-.035f}),p({side*.355f+.025f,.38f,.035f}),Block::Slate,Brass*.7f);
    }
    bevel(out,p({-.48f,.38f,-.48f}),p({.48f,.52f,.48f}),.10f,Block::Slate,Brass);
    out.box(p({-.20f,.52f,-.20f}),p({.20f,.59f,.20f}),Block::Slate,Iron);
}

} // namespace

void appendHeartwoodProp(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::HeartSeed: heartSeed(out); break;
    case HubAssetKind::ChainLantern: chainLantern(out,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
