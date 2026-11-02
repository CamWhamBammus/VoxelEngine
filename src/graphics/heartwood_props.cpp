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

void heartSeed(AssetBuilder& out, std::uint32_t variation)
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
        const glm::vec3 amber{.98f+.01f*float(variation%3),.45f+.055f*float((side+row+variation%3)%4),.035f};
        out.quad(ring[row][next],ring[row][side],ring[row+1][side],ring[row+1][next],Block::Lantern,amber);
        out.beam(ring[row][side],ring[row+1][side],.027f,Block::Slate,Brass*1.25f);
        if (row>0) out.beam(ring[row][side],ring[row][next],.025f,Block::Slate,Brass*1.25f);
    }
    for (int side=0;side<8;++side) {
        const int next=(side+1)%8;
        out.triangle({0,heights[0],0},ring[0][side],ring[0][next],Block::Lantern,{.86f,.44f,.04f});
        out.triangle({0,heights[5],0},ring[5][next],ring[5][side],Block::Lantern,{1.f,.81f,.22f});
    }
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

// A low miniature landscape makes the survey table readable as a map from the
// doorway. Each contour, river tile and route marker stands above the parchment.
void reliefMapTable(AssetBuilder& out, std::uint32_t variation)
{
    const auto oak=Oak*(.93f+.04f*float(variation%3));
    for (const float x:{-1.55f,1.55f}) for (const float z:{-.94f,.94f}) {
        bevel(out,{x-.14f,0,z-.14f},{x+.14f,.19f,z+.14f},.04f,Block::Timber,Bark);
        out.box({x-.10f,.19f,z-.10f},{x+.10f,1.28f,z+.10f},Block::Timber,oak);
    }
    for (const float z:{-.94f,.94f})
        out.beam({-1.55f,.38f,z},{1.55f,.38f,z},.14f,Block::Timber,oak*.8f);
    bevel(out,{-2.02f,1.22f,-1.30f},{2.02f,1.43f,1.30f},.065f,Block::Planks,oak);
    out.box({-1.90f,1.43f,-1.18f},{1.90f,1.465f,1.18f},Block::Limestone,{.72f,.64f,.42f});
    for (const float z:{-1.24f,1.24f})
        out.box({-1.98f,1.43f,z-.04f},{1.98f,1.51f,z+.04f},Block::Slate,Brass);
    for (const float x:{-1.96f,1.96f})
        out.box({x-.04f,1.43f,-1.24f},{x+.04f,1.51f,1.24f},Block::Slate,Brass);
    // Separated contour slabs keep readable relief and real lighting, without
    // becoming another dense block pile in the middle of the room.
    bevel(out,{-1.57f,1.465f,-.84f},{-.32f,1.60f,.57f},.06f,Block::Moss,{.30f,.40f,.17f});
    bevel(out,{-1.39f,1.60f,-.69f},{-.55f,1.75f,.30f},.05f,Block::Stone,{.39f,.43f,.29f});
    bevel(out,{-1.20f,1.75f,-.52f},{-.76f,1.95f,-.05f},.05f,Block::Slate,{.51f,.51f,.39f});
    bevel(out,{.33f,1.465f,-.34f},{1.59f,1.61f,.86f},.06f,Block::Moss,{.27f,.39f,.17f});
    bevel(out,{.64f,1.61f,-.12f},{1.38f,1.78f,.64f},.05f,Block::Stone,{.39f,.43f,.29f});
    for (int reach=0;reach<7;++reach) {
        const float z=-1.01f+float(reach)*.30f;
        const float x=std::sin(float(reach)*.84f)*.18f;
        out.box({x-.105f,1.468f,z},{x+.105f,1.49f,z+.29f},Block::Slate,{.12f,.43f,.44f});
    }
    const std::array<glm::vec3,5> route{{{-1.48f,1.62f,.41f},{-.63f,1.62f,.48f},
        {.12f,1.50f,.61f},{.48f,1.63f,.69f},{1.19f,1.80f,.38f}}};
    for (const auto point:route)
        bevel(out,point-glm::vec3(.035f,0,.035f),point+glm::vec3(.035f,.055f,.035f),.012f,Block::Slate,Brass*1.2f);
    for (const auto flag:std::array<glm::vec3,2>{{{-1.03f,1.95f,-.28f},{1.10f,1.78f,.19f}}}) {
        out.beam(flag,flag+glm::vec3(0,.49f,0),.034f,Block::Slate,Brass);
        const auto color=flag.x<0 ? glm::vec3(.56f,.22f,.14f) : glm::vec3(.19f,.39f,.45f);
        out.quad(flag+glm::vec3(.02f,.25f,0),flag+glm::vec3(.29f,.29f,0),
                 flag+glm::vec3(.26f,.47f,0),flag+glm::vec3(.02f,.47f,0),Block::Planks,color,true);
    }
    // A rolled chart is tied on the near ledge, clear of the terrain.
    jungle::stem(out,{-1.10f,1.57f,.99f},{-.32f,1.57f,.99f},.075f,.075f,{.76f,.69f,.49f},Block::Limestone);
    jungle::stem(out,{-.76f,1.57f,.99f},{-.70f,1.57f,.99f},.084f,.084f,Brass,Block::Slate);
}

void surveyInstrument(AssetBuilder& out, std::uint32_t variation)
{
    const auto brass=Brass*(.93f+.04f*float(variation%3));
    const glm::vec3 pivot{0,1.82f,0};
    for (int leg=0;leg<3;++leg) {
        const float a=float(leg)*jungle::Pi*2/3+.3f;
        const glm::vec3 foot{std::cos(a)*.76f,.045f,std::sin(a)*.76f};
        jungle::stem(out,foot,pivot,.095f,.065f,Oak);
        out.box(foot-glm::vec3(.10f,.045f,.10f),foot+glm::vec3(.10f,.055f,.10f),Block::Slate,Iron);
        jungle::stem(out,glm::mix(foot,pivot,.37f),{0,.96f,0},.028f,.028f,brass,Block::Slate);
    }
    jungle::stem(out,{0,1.73f,0},{0,2.04f,0},.21f,.21f,brass,Block::Slate);
    for (const float x:{-.29f,.29f})
        out.box({x-.055f,1.97f,-.10f},{x+.055f,2.37f,.10f},Block::Slate,brass*.8f);
    const glm::vec3 back{0,2.15f,.59f},front{0,2.55f,-1.03f};
    const auto along=glm::normalize(front-back);
    jungle::stem(out,back,front,.17f,.25f,brass,Block::Slate);
    jungle::stem(out,back-along*.25f,back,.09f,.13f,Iron,Block::Slate);
    for (const float fraction:{.08f,.56f,.93f}) {
        const auto center=glm::mix(back,front,fraction);
        const float radius=glm::mix(.17f,.25f,fraction)+.025f;
        jungle::stem(out,center-along*.045f,center+along*.045f,radius,radius,brass*1.17f,Block::Slate);
    }
    jungle::stem(out,front+along*.008f,front+along*.026f,.205f,.205f,{.12f,.42f,.47f},Block::Slate);
    out.beam({.31f,2.28f,0},{.49f,2.28f,0},.07f,Block::Slate,Iron);
    jungle::stem(out,{.48f,2.28f,0},{.55f,2.28f,0},.12f,.12f,brass,Block::Slate);
}

void seedling(AssetBuilder& out, glm::vec3 origin, float height, std::uint32_t seed)
{
    const glm::vec3 top=origin+glm::vec3(.06f,height,0);
    jungle::stem(out,origin,top,.027f,.012f,{.28f,.36f,.12f});
    for (int leaf=0;leaf<5;++leaf) {
        const float a=float(leaf)*2.4f+float(seed%5)*.53f;
        const float h=height*(.26f+.13f*float(leaf));
        const float reach=height*(.57f-.05f*float(leaf));
        const auto base=origin+glm::vec3(.06f*h/height,h,0);
        const auto tip=base+glm::vec3(std::cos(a)*reach,.18f*height,std::sin(a)*reach);
        jungle::stem(out,base,glm::mix(base,tip,.29f),.014f,.009f,{.29f,.41f,.11f});
        jungle::leaf(out,glm::mix(base,tip,.17f),tip,.14f*height,
            {.20f+.02f*float(leaf%3),.43f+.018f*float(seed%3),.11f});
    }
}

void flowerPot(AssetBuilder& out, glm::vec3 center, float radius, float height)
{
    const glm::vec3 clay{.52f,.26f,.13f};
    // Hollow rim and recessed soil leave the plant visibly inside a pot.
    jungle::stem(out,center+glm::vec3(0,.02f,0),center+glm::vec3(0,height-.065f,0),
                 radius*.70f,radius,clay,Block::Brick);
    const int sides=8;
    for (int side=0;side<sides;++side) {
        const float a=float(side)*jungle::Pi/4,b=float(side+1)*jungle::Pi/4;
        const auto radial=[&](float angle,float r,float y) {return center+glm::vec3(std::cos(angle)*r,y,std::sin(angle)*r);};
        out.quad(radial(a,radius,height-.07f),radial(b,radius,height-.07f),
                 radial(b,radius,height),radial(a,radius,height),Block::Brick,clay*1.10f);
        out.quad(radial(a,radius,height),radial(b,radius,height),
                 radial(b,radius*.80f,height),radial(a,radius*.80f,height),Block::Brick,clay*1.25f);
        out.triangle(center+glm::vec3(0,height-.045f,0),radial(b,radius*.8f,height-.045f),
                     radial(a,radius*.8f,height-.045f),Block::Dirt,{.19f,.13f,.07f});
    }
}

void nurseryBed(AssetBuilder& out, std::uint32_t variation)
{
    constexpr glm::vec3 wood{.33f,.25f,.12f};
    out.box({-1.35f,.08f,-.94f},{1.35f,.45f,.94f},Block::Dirt,{.19f,.14f,.07f});
    for (const float z:{-1.02f,1.02f})
        for (int course=0;course<2;++course)
            bevel(out,{-1.48f,.07f+course*.24f,z-.09f},{1.48f,.30f+course*.24f,z+.09f},.025f,Block::Planks,wood);
    for (const float x:{-1.44f,1.44f}) {
        bevel(out,{x-.09f,0,-1.11f},{x+.09f,.62f,1.11f},.035f,Block::Timber,Oak);
        for (const float z:{-.88f,.88f})
            out.box({x-.10f,.19f,z-.045f},{x+.10f,.27f,z+.045f},Block::Slate,Iron);
    }
    for (int plant=0;plant<6;++plant) {
        const float x=-.91f+float(plant%3)*.91f,z=plant<3 ? -.43f : .43f;
        const float height=.70f+.17f*float((plant+variation)%3);
        seedling(out,{x,.45f,z},height,variation+plant);
        // A little wooden row label belongs to the bed, not the walking lane.
        if (plant<3) {
            out.beam({x,.46f,.82f},{x,.82f,.82f},.035f,Block::Timber,Oak);
            bevel(out,{x-.12f,.72f,.79f},{x+.12f,.88f,.85f},.015f,Block::Planks,{.63f,.53f,.31f});
        }
    }
}

void pottingBench(AssetBuilder& out, std::uint32_t variation)
{
    for (const float x:{-1.25f,1.25f}) for (const float z:{-.51f,.51f})
        out.box({x-.075f,0,z-.075f},{x+.075f,1.18f,z+.075f},Block::Timber,Oak*.82f);
    bevel(out,{-1.47f,1.13f,-.72f},{1.47f,1.31f,.72f},.035f,Block::Planks,Oak);
    bevel(out,{-1.30f,.30f,-.54f},{1.30f,.42f,.54f},.025f,Block::Planks,Oak*.76f);
    for (const float x:{-1.27f,1.27f})
        out.box({x-.07f,1.25f,-.64f},{x+.07f,3.22f,-.50f},Block::Timber,Oak);
    for (const float y:{2.15f,3.10f})
        bevel(out,{-1.44f,y,-.76f},{1.44f,y+.13f,-.12f},.025f,Block::Planks,Oak);
    flowerPot(out,{-.85f,1.31f,.10f},.28f,.47f);
    seedling(out,{-.85f,1.72f,.10f},.93f,variation);
    flowerPot(out,{.62f,1.31f,.15f},.23f,.35f);
    for (const float x:{-.90f,-.26f,.44f,1.f})
        flowerPot(out,{x,2.28f,-.44f},.19f,.32f);
    for (const float x:{-.84f,.05f,.90f}) {
        flowerPot(out,{x,3.23f,-.44f},.18f,.31f);
        seedling(out,{x,3.50f,-.44f},.30f,variation+3);
    }
    for (int sack=0;sack<2;++sack)
        bevel(out,{-1.07f+float(sack)*.77f,.42f,-.36f},{-.44f+float(sack)*.77f,.88f,.37f},.09f,Block::Limestone,{.51f,.45f,.29f});
    // A hand trowel and packets give the worktop its own purpose.
    out.beam({.12f,1.35f,.42f},{.45f,1.35f,.42f},.065f,Block::Timber,Oak);
    out.box({-.16f,1.33f,.31f},{.12f,1.37f,.53f},Block::Slate,{.40f,.44f,.39f});
    out.box({.94f,1.31f,.14f},{1.28f,1.34f,.52f},Block::Limestone,{.71f,.61f,.38f});
    out.box({1.04f,1.34f,.25f},{1.16f,1.348f,.39f},Block::Planks,{.27f,.40f,.13f});
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
            out.triangle(foot,ring[side],ring[next],Block::Crystal,cyan*(.83f+.02f*float(side)));
            out.triangle(ring[next],ring[side],peak,Block::Crystal,cyan*(.9f+.02f*float(side)));
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
    const auto first=mesh.vertices.size();
    AssetBuilder out(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::HeartSeed: heartSeed(out,asset.variation); break;
    case HubAssetKind::ChainLantern: chainLantern(out,asset.variation); break;
    case HubAssetKind::LibraryBookcase: bookcase(out,asset.variation); break;
    case HubAssetKind::ReadingDesk: readingDesk(out,asset.variation); break;
    case HubAssetKind::ReadingSeat: readingSeat(out,asset.variation); break;
    case HubAssetKind::SapCluster: sapCluster(out,asset.variation); break;
    case HubAssetKind::RootCorbel: rootCorbel(out,asset.variation); break;
    case HubAssetKind::ReliefMapTable: reliefMapTable(out,asset.variation); break;
    case HubAssetKind::SurveyInstrument: surveyInstrument(out,asset.variation); break;
    case HubAssetKind::NurseryBed: nurseryBed(out,asset.variation); break;
    case HubAssetKind::PottingBench: pottingBench(out,asset.variation); break;
    default: break;
    }
    if (asset.kind==HubAssetKind::HeartSeed) {
        // The centerpiece needs a broad base in the enlarged hall. Transform
        // normals with the inverse scale so its facets retain correct lighting.
        constexpr glm::vec3 scale{1.5f,1.1f,1.5f};
        for (std::size_t i=first;i<mesh.vertices.size();++i) {
            auto& vertex=mesh.vertices[i];
            vertex.position=asset.position+(vertex.position-asset.position)*scale;
            vertex.normal=glm::normalize(vertex.normal/scale);
        }
    }
}

} // namespace voxel
