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

void standingBook(AssetBuilder& out, glm::vec3 origin, float width, float height,
                  glm::vec3 cover)
{
    const auto p=[&](glm::vec3 v) {return origin+v;};
    constexpr float depth=.41f;
    out.box(p({.016f,.018f,0}),p({width-.016f,height-.018f,depth-.028f}),Block::Limestone,{.68f,.62f,.43f});
    for (const float x:{0.f,width-.024f})
        out.box(p({x,0,-.014f}),p({x+.024f,height,depth}),Block::Planks,cover);
    out.box(p({0,0,depth-.035f}),p({width,height,depth+.015f}),Block::Planks,cover);
    for (const float y:{.06f,height-.09f})
        out.box(p({.012f,y,depth+.016f}),p({width-.012f,y+.018f,depth+.025f}),Block::Slate,Brass);
    out.box(p({width*.28f,height*.55f,depth+.016f}),p({width*.72f,height*.68f,depth+.025f}),Block::Limestone,{.68f,.58f,.36f});
}

void bookcase(AssetBuilder& out, std::uint32_t variation)
{
    const auto oak=Oak*(.94f+.03f*float(variation%3));
    out.box({-1.08f,.08f,-.35f},{1.08f,3.30f,-.25f},Block::Planks,oak*.62f);
    for (const float x:{-1.12f,.98f}) {
        bevel(out,{x,0,-.43f},{x+.14f,3.42f,.43f},.04f,Block::Timber,oak);
        out.box({x+.035f,.25f,.43f},{x+.10f,3.22f,.475f},Block::Timber,oak*1.25f);
    }
    for (int shelf=0;shelf<5;++shelf) {
        const float y=.15f+float(shelf)*.77f;
        bevel(out,{-1.10f,y,-.37f},{1.10f,y+.12f,.44f},.025f,Block::Planks,oak*1.15f);
    }
    bevel(out,{-1.18f,3.32f,-.46f},{1.18f,3.47f,.47f},.055f,Block::Timber,oak*1.15f);
    bevel(out,{-1.17f,0,-.45f},{1.17f,.16f,.46f},.04f,Block::Timber,oak*.8f);
    constexpr std::array<glm::vec3,6> covers{{{.26f,.36f,.14f},{.43f,.16f,.12f},
        {.17f,.29f,.34f},{.42f,.30f,.15f},{.29f,.19f,.36f},{.53f,.39f,.19f}}};
    for (int shelf=0;shelf<4;++shelf) for (int book=0;book<8;++book) {
        const auto seed=variation+std::uint32_t(shelf*19+book*7);
        const float height=.47f+.045f*float(seed%4);
        const auto origin=glm::vec3(-.95f+float(book)*.235f,.27f+float(shelf)*.77f,-.15f+.02f*float(seed%3));
        standingBook(out,origin,.18f+.01f*float(seed%3),height,covers[seed%covers.size()]);
    }
    // A carved twig crest keeps the case's trim from reading as another crate.
    for (const float side:{-1.f,1.f}) {
        out.beam({0,3.48f,.12f},{side*.55f,3.55f,.12f},.07f,Block::RootWood,oak);
        out.beam({side*.55f,3.55f,.12f},{side*.79f,3.48f,.12f},.055f,Block::RootWood,oak);
    }
}

void readingDesk(AssetBuilder& out, std::uint32_t variation)
{
    const auto oak=Oak*(.96f+.03f*float(variation%3));
    for (const float x:{-1.f,1.f}) for (const float z:{-.45f,.45f}) {
        out.box({x-.07f,0,z-.07f},{x+.07f,1.05f,z+.07f},Block::Timber,oak*.8f);
        bevel(out,{x-.11f,0,z-.11f},{x+.11f,.13f,z+.11f},.035f,Block::Timber,oak);
    }
    for (const float z:{-.45f,.45f}) out.beam({-1,.3f,z},{1,.3f,z},.09f,Block::Timber,oak*.7f);
    for (int board=0;board<5;++board) {
        const float x=-1.22f+float(board)*.49f;
        bevel(out,{x,1.03f,-.65f},{x+.475f,1.18f,.65f},.025f,Block::Planks,oak*(1.02f+.035f*float(board%3)));
    }
    out.box({-.74f,.77f,.47f},{.74f,1.03f,.55f},Block::Planks,oak*.8f);
    out.box({-.08f,.86f,.55f},{.08f,.93f,.59f},Block::Slate,Brass);
    // The open volume has raised pages, a real spine and a leather cover.
    out.box({-.58f,1.18f,-.29f},{.38f,1.22f,.40f},Block::Planks,{.29f,.20f,.10f});
    const glm::vec3 paper{.79f,.72f,.52f};
    out.quad({-.55f,1.25f,-.26f},{-.55f,1.25f,.37f},{-.10f,1.32f,.37f},{-.10f,1.32f,-.26f},Block::Limestone,paper);
    out.quad({-.10f,1.32f,-.26f},{-.10f,1.32f,.37f},{.35f,1.25f,.37f},{.35f,1.25f,-.26f},Block::Limestone,paper*.95f);
    out.box({-.12f,1.22f,-.27f},{-.08f,1.33f,.38f},Block::Planks,oak);
    for (int line=0;line<6;++line) {
        const float z=-.17f+float(line)*.077f;
        out.beam({-.49f,1.261f,z},{-.18f,1.309f,z},.008f,Block::Slate,Iron*.9f);
        out.beam({-.02f,1.309f,z},{.29f,1.261f,z},.008f,Block::Slate,Iron*.9f);
    }
    bevel(out,{.70f,1.18f,-.34f},{.94f,1.43f,-.10f},.05f,Block::Slate,{.10f,.18f,.17f});
    out.beam({.82f,1.42f,-.22f},{1.02f,1.80f,-.28f},.025f,Block::Timber,{.71f,.63f,.42f});
    jungle::leaf(out,{.86f,1.49f,-.23f},{1.07f,1.87f,-.29f},.065f,{.67f,.68f,.47f},{0,0,1});
}

void readingSeat(AssetBuilder& out, std::uint32_t variation)
{
    for (const float x:{-.34f,.34f}) for (const float z:{-.31f,.31f})
        out.box({x-.055f,0,z-.055f},{x+.055f,.57f,z+.055f},Block::Timber,Oak*.8f);
    bevel(out,{-.44f,.48f,-.42f},{.44f,.64f,.42f},.04f,Block::Planks,Oak);
    const auto cloth=variation%2 ? glm::vec3(.30f,.19f,.33f) : glm::vec3(.23f,.33f,.14f);
    bevel(out,{-.36f,.64f,-.34f},{.36f,.73f,.34f},.05f,Block::Planks,cloth);
    for (const float x:{-.34f,.34f})
        out.box({x-.06f,.55f,-.39f},{x+.06f,1.40f,-.27f},Block::Timber,Oak);
    for (const float y:{.89f,1.19f})
        bevel(out,{-.37f,y,-.41f},{.37f,y+.17f,-.25f},.03f,Block::Planks,Oak*1.1f);
}

void sapCluster(AssetBuilder& out, std::uint32_t variation)
{
    // The bark backing and thin gold-brown rims hold irregular glowing sap
    // plates against a real wall. The crystals project only a little past it.
    for (int row=0;row<7;++row) {
        const float x=.16f*std::sin(float(row)*1.32f+float(variation%3));
        const float y=float(row)*.48f;
        bevel(out,{x-.30f,y,-.055f},{x+.30f,y+.50f,.11f},.045f,Block::RootWood,Bark);
        bevel(out,{x-.22f,y+.025f,.08f},{x+.24f,y+.475f,.25f},.06f,Block::Crystal,
            {.09f,.62f+.035f*float(row%3),.57f});
        if (row%2==0)
            bevel(out,{x+.10f,y+.12f,.23f},{x+.35f,y+.38f,.40f},.05f,Block::Crystal,{.14f,.78f,.70f});
    }
    for (int crystal=0;crystal<3;++crystal) {
        const float x=-.50f+float(crystal)*.46f;
        const float y=.12f+float((crystal+variation)%3)*.88f;
        const float height=.46f+.12f*float(crystal%2);
        const glm::vec3 foot{x,y,.25f},peak{x+.045f,y+height,.30f};
        constexpr glm::vec3 cyan{.10f,.73f,.73f};
        std::array<glm::vec3,6> ring{};
        for (int side=0;side<6;++side) {
            const float angle=float(side)*jungle::Pi/3;
            ring[side]=foot+glm::vec3(std::cos(angle)*.14f,height*.48f,std::sin(angle)*.14f);
        }
        for (int side=0;side<6;++side) {
            const int next=(side+1)%6;
            out.triangle(foot,ring[next],ring[side],Block::Crystal,cyan*(.83f+.02f*float(side)));
            out.triangle(ring[side],ring[next],peak,Block::Crystal,cyan*(.9f+.02f*float(side)));
        }
    }
}

void rootCorbel(AssetBuilder& out, std::uint32_t variation)
{
    const auto wood=Oak*(.9f+.04f*float(variation%3));
    out.box({-.24f,-2.18f,-.05f},{.24f,0,.19f},Block::Timber,Bark);
    // Broad branched knees run continuously from the wall to the underside of
    // the projecting beam. Their top face is exactly the anchor's Y plane.
    constexpr std::array<glm::vec3,4> points{{{0,-2.02f,.10f},{0,-1.18f,.52f},
        {0,-.59f,1.03f},{0,-.19f,1.73f}}};
    for (int section=0;section<3;++section) {
        jungle::stem(out,points[section],points[section+1],.23f-.025f*float(section),
            .205f-.025f*float(section),wood);
        jungle::stem(out,points[section]+glm::vec3(.17f,0,0),points[section+1]+glm::vec3(.14f,0,0),
            .05f,.045f,wood*1.2f);
    }
    out.box({-.24f,-.19f,.04f},{.24f,0,1.95f},Block::Timber,wood);
    jungle::stem(out,{0,-1.65f,.23f},{-.58f,-.55f,.49f},.13f,.09f,wood*.87f);
    jungle::stem(out,{-.58f,-.55f,.49f},{-.60f,-.11f,.51f},.09f,.08f,wood);
    out.box({-.69f,-.15f,.34f},{-.51f,0,.65f},Block::Timber,wood);
    for (const float y:{-.30f,-1.80f})
        out.box({-.27f,y,-.01f},{.27f,y+.12f,.22f},Block::Slate,Brass*.65f);
}

} // namespace

void appendHeartwoodProp(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder out(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::HeartSeed: heartSeed(out); break;
    case HubAssetKind::ChainLantern: chainLantern(out,asset.variation); break;
    case HubAssetKind::LibraryBookcase: bookcase(out,asset.variation); break;
    case HubAssetKind::ReadingDesk: readingDesk(out,asset.variation); break;
    case HubAssetKind::ReadingSeat: readingSeat(out,asset.variation); break;
    case HubAssetKind::SapCluster: sapCluster(out,asset.variation); break;
    case HubAssetKind::RootCorbel: rootCorbel(out,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
