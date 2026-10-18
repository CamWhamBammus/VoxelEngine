#include "asset_builder.h"

#include <array>
#include <cmath>
#include <span>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

constexpr float Pi=3.14159265359f;
constexpr glm::vec3 Rock{0.36f,0.42f,0.44f};
constexpr glm::vec3 Moss{0.25f,0.46f,0.19f};
constexpr glm::vec3 Bark{0.40f,0.27f,0.15f};

float variation(std::uint32_t seed,std::uint32_t part)
{
    auto value=seed^(part*0x9e3779b9u+0x85ebca6bu);
    value=(value^(value>>16))*0x7feb352du;
    value=(value^(value>>15))*0x846ca68bu;
    return float((value^(value>>16))&0x00ffffffu)/16777216.0f;
}

// Moss follows the actual stone facet, with just enough separation to avoid
// depth fighting. It never becomes a floating patch above a sloping rock.
void mossPatch(AssetBuilder& out,glm::vec3 a,glm::vec3 b,glm::vec3 c,float shade)
{
    const auto center=(a+b+c)/3.0f;
    const auto offset=glm::normalize(glm::cross(b-a,c-a))*0.006f;
    a=glm::mix(center,a,.78f)+offset;
    b=glm::mix(center,b,.88f)+offset;
    c=glm::mix(center,c,.72f)+offset;
    out.triangle(a,b,c,Block::Leaves,Moss*shade);
}

// Unequal straight edges and a sloping crown create broad broken plates rather
// than a rounded sphere. Every fragment has a flat grounded footprint.
void rockMass(AssetBuilder& out,glm::vec3 base,glm::vec3 size,std::uint32_t seed,bool moss)
{
    constexpr std::array<glm::vec2,7> outline{{
        {-1,-.42f},{-.50f,-.91f},{.43f,-.83f},{.98f,-.12f},
        {.77f,.67f},{-.05f,.96f},{-.82f,.65f}
    }};
    std::array<glm::vec3,7> foot,shoulder,crown;
    for (std::size_t i=0;i<outline.size();++i) {
        const auto p=outline[i]*(.90f+.10f*variation(seed,static_cast<std::uint32_t>(i)));
        foot[i]=base+glm::vec3(p.x*size.x,0,p.y*size.z);
        shoulder[i]=base+glm::vec3(p.x*size.x*.95f,size.y*(.40f+.09f*variation(seed,i+11)),p.y*size.z*.95f);
        crown[i]=base+glm::vec3((p.x*.62f+.08f)*size.x,
            size.y*(.76f+p.x*.10f-p.y*.07f), (p.y*.66f-.06f)*size.z);
    }
    const auto top=base+glm::vec3(.08f*size.x,.76f*size.y,-.06f*size.z);
    for (std::size_t i=0;i<outline.size();++i) {
        const auto next=(i+1)%outline.size();
        const auto color=Rock*(.82f+.26f*variation(seed,i+23));
        // This outline runs counterclockwise in X/Z, hence reversed top order.
        out.quad(foot[next],foot[i],shoulder[i],shoulder[next],Block::Slate,color*.82f);
        out.quad(shoulder[next],shoulder[i],crown[i],crown[next],Block::Stone,color);
        out.triangle(base,foot[i],foot[next],Block::Slate,color*.65f);
        out.triangle(top,crown[next],crown[i],Block::Stone,color*1.10f);
        if (moss && (i+seed)%3!=0) mossPatch(out,top,crown[next],crown[i],.88f+.13f*float(i%3));
    }
}

void boulder(AssetBuilder& out,std::uint32_t seed)
{
    rockMass(out,{.10f,0,-.08f},{1.32f,2.14f+.20f*variation(seed,1),1.22f},seed,true);
    rockMass(out,{-1.04f,0,.54f},{.66f,.79f+.25f*variation(seed,2),.76f},seed+1,true);
    rockMass(out,{.95f,0,.77f},{.69f,1.05f,.65f},seed+2,false);
}

void ledgeLayer(AssetBuilder& out,float width,float depth,float bottom,float top,
                std::uint32_t seed,bool moss)
{
    const std::array<glm::vec2,7> outline{{
        {-width,0},{-width*.94f,depth*.53f},{-width*.48f,depth*.91f},
        {width*.12f,depth},{width*.66f,depth*.82f},{width,depth*.35f},{width*.96f,0}
    }};
    std::array<glm::vec3,7> low,high;
    glm::vec3 center{};
    for (std::size_t i=0;i<outline.size();++i) {
        const auto p=outline[i];
        low[i]={p.x*.96f,bottom+.04f*variation(seed,i),p.y*.85f};
        high[i]={p.x,top-.08f*variation(seed,i+11)-p.y*.06f,p.y};
        center+=high[i]/float(outline.size());
    }
    const glm::vec3 underside(0,bottom,depth*.40f);
    for (std::size_t i=0;i<outline.size();++i) {
        const auto next=(i+1)%outline.size();
        const auto color=Rock*(.81f+.25f*variation(seed,i+19));
        out.quad(low[i],low[next],high[next],high[i],Block::Slate,color*.83f);
        out.triangle(underside,low[next],low[i],Block::Slate,color*.65f);
        out.triangle(center,high[i],high[next],Block::Stone,color*1.10f);
        if (moss && i>0 && i<5) mossPatch(out,center,high[i],high[next],.95f+.08f*float(i%2));
    }
}

void rockLedge(AssetBuilder& out,std::uint32_t seed)
{
    const float width=.92f+.08f*variation(seed,1);
    ledgeLayer(out,2.12f*width,1.02f,-1.0f,-.32f,seed,false);
    ledgeLayer(out,2.65f*width,1.42f,-.55f,.02f,seed+1,false);
    ledgeLayer(out,2.34f*width,1.22f,-.20f,.28f,seed+2,true);
}

void stalactite(AssetBuilder& out,glm::vec3 base,float length,float radius,std::uint32_t seed)
{
    std::array<std::array<glm::vec3,6>,3> rings;
    constexpr float heights[]{0,-.24f,-.69f},widths[]{1,.70f,.31f};
    const glm::vec3 lean(.14f*(variation(seed,1)-.5f),0,.12f*(variation(seed,2)-.5f));
    for (int level=0;level<3;++level) for (int side=0;side<6;++side) {
        const float angle=(float(side)+.13f*variation(seed,3))*Pi/3.0f;
        const float size=radius*widths[level]*(.90f+.10f*variation(seed,side+9));
        rings[level][side]=base+glm::vec3(std::cos(angle)*size,heights[level]*length,std::sin(angle)*size)
            +lean*float(level);
    }
    const auto tip=base+glm::vec3(0,-length,0)+lean*2.6f;
    for (int side=0;side<6;++side) {
        const int next=(side+1)%6;
        const auto color=glm::vec3(.51f,.55f,.52f)*(.80f+.19f*variation(seed,side+21));
        for (int level=0;level<2;++level)
            out.quad(rings[level][side],rings[level][next],rings[level+1][next],rings[level+1][side],
                     Block::Limestone,color*(1.0f-.11f*float(level)));
        out.triangle(rings[2][side],rings[2][next],tip,Block::Limestone,color*.81f);
        out.triangle(base,rings[0][next],rings[0][side],Block::Stone,color*.72f);
    }
}

void stalactiteCluster(AssetBuilder& out,std::uint32_t seed)
{
    constexpr std::array<glm::vec3,5> bases{{
        {-.72f,0,-.48f},{.57f,0,-.49f},{.07f,0,.57f},{1.05f,0,.54f},{-1.03f,0,.62f}
    }};
    for (std::size_t i=0;i<bases.size();++i) {
        const float length=i==0?3.35f+.40f*variation(seed,7):1.50f+1.48f*variation(seed,i+11);
        stalactite(out,bases[i],length,.33f+.14f*variation(seed,i+21),seed+static_cast<std::uint32_t>(i));
    }
}

struct RootPoint { glm::vec3 position; float width; };

// Six-sided sections turn with each bend. Painted longitudinal facets show
// bark ridges without adding a noisy mesh of individual grooves.
void root(AssetBuilder& out,std::span<const RootPoint> points,float depth,std::uint32_t seed,
          glm::vec3 color=Bark)
{
    std::array<glm::vec3,6> first{},previous{};
    for (std::size_t i=0;i<points.size();++i) {
        const auto before=points[i==0?i:i-1].position;
        const auto after=points[i+1==points.size()?i:i+1].position;
        const auto tangent=glm::normalize(glm::vec3(after.x-before.x,after.y-before.y,0));
        const glm::vec3 across(-tangent.y,tangent.x,0);
        std::array<glm::vec3,6> ring;
        for (int side=0;side<6;++side) {
            const float angle=float(side)*Pi/3.0f;
            ring[side]=points[i].position+across*(std::cos(angle)*points[i].width)
                +glm::vec3(0,0,std::sin(angle)*points[i].width*depth);
        }
        if (i==0) first=ring;
        else for (int side=0;side<6;++side) {
            const int next=(side+1)%6;
            const float shade=.72f+.12f*float(side%3)+.10f*variation(seed,side+5);
            out.quad(previous[side],previous[next],ring[next],ring[side],Block::Timber,color*shade);
        }
        previous=ring;
    }
    for (int side=0;side<6;++side) {
        const int next=(side+1)%6;
        out.triangle(points.front().position,first[next],first[side],Block::Timber,color*.65f);
        out.triangle(points.back().position,previous[side],previous[next],Block::Timber,color*.76f);
    }
}

void rootArch(AssetBuilder& out,std::uint32_t seed)
{
    constexpr glm::vec3 darkBark{.21f,.16f,.10f};
    std::array<RootPoint,11> spine{{
        {{-3.30f,0,0},.62f},{{-3.30f,1.10f,-.08f},.50f},{{-3.05f,2.65f,.03f},.43f},
        {{-2.48f,3.35f,.10f},.40f},{{-1.35f,3.53f,0},.34f},{{-.10f,3.48f,.13f},.28f},
        {{1.20f,3.28f,.03f},.23f},{{2.55f,3.10f,-.05f},.22f},{{3.12f,2.55f,-.05f},.22f},
        {{3.30f,1.15f,0},.25f},{{3.30f,0,0},.33f}
    }};
    for (std::size_t i=2;i+2<spine.size();++i) {
        spine[i].position.y-=.07f*variation(seed,i);
        spine[i].position.z+=.07f*(variation(seed,i+17)-.5f);
    }
    root(out,spine,1.0f,seed,darkBark);
    const auto across=[&](std::size_t i) {
        const auto delta=spine[i+1==spine.size()?i:i+1].position-spine[i==0?i:i-1].position;
        return glm::normalize(glm::vec3(-delta.y,delta.x,0));
    };
    // Narrow raised ridges follow the broad front bark facet, embedding into
    // it slightly. Their uneven spacing reads as growth rather than joinery.
    for (const float side : {-.23f,.18f}) {
        auto ridge=spine;
        for (std::size_t i=0;i<ridge.size();++i) {
            ridge[i].position+=across(i)*spine[i].width*side
                +glm::vec3(0,0,spine[i].width*.8660254f+.005f);
            if (i==0 || i+1==ridge.size()) ridge[i].position.y=.03f;
            ridge[i].width=.018f+.010f*variation(seed,i+31);
        }
        root(out,ridge,.9f,seed+1,darkBark*(side<0?.75f:1.08f));
    }
    // Flared toes share the main roots, spread along the ground, and taper to
    // small flat feet. None enter the clear walking space under the middle.
    for (const auto& toe : std::array<std::array<RootPoint,3>,3>{{
        {{{{-3.30f,.45f,0},.23f},{{-3.43f,.17f,.47f},.18f},{{-3.43f,0,.69f},.10f}}},
        {{{{-3.30f,.35f,-.05f},.18f},{{-3.70f,.12f,-.34f},.15f},{{-3.70f,0,-.64f},.08f}}},
        {{{{3.30f,.30f,0},.15f},{{3.52f,.10f,.40f},.12f},{{3.52f,0,.62f},.07f}}}
    }}) root(out,toe,1.0f,seed+2,darkBark*.91f);
    for (const std::size_t i : {3u,5u,7u}) {
        const auto front=[&](std::size_t point,float edge) {
            return spine[point].position+across(point)*spine[point].width*edge
                +glm::vec3(0,0,spine[point].width*.8660254f);
        };
        mossPatch(out,front(i,-.5f),front(i+1,-.5f),front(i,.5f),.80f);
    }
}

void leaf(AssetBuilder& out,glm::vec3 base,glm::vec3 tip,float width,glm::vec3 color)
{
    const auto delta=tip-base;
    const auto across=glm::normalize(glm::vec3(-delta.y,delta.x,0))*width;
    const auto ridge=base+delta*.48f+glm::vec3(0,0,.026f);
    const auto left=base+delta*.42f+across,right=base+delta*.53f-across*.85f;
    out.triangle(base,left,ridge,Block::Leaves,color,true);
    out.triangle(left,tip,ridge,Block::Leaves,color,true);
    out.triangle(tip,right,ridge,Block::Leaves,color*.82f,true);
    out.triangle(right,base,ridge,Block::Leaves,color*.82f,true);
}

void rootCurtain(AssetBuilder& out,std::uint32_t seed)
{
    for (int stem=0;stem<3;++stem) {
        const float x=float(stem-1)*.72f;
        const float sway=.16f*(variation(seed,stem+3)-.5f);
        const float length=2.95f+.72f*variation(seed,stem+11);
        const std::array<RootPoint,5> spine{{
            {{x,0,.18f},.18f},{{x,-.70f,.20f},.17f},
            {{x+.17f+sway,-1.48f,.25f},.13f},{{x-.12f+sway,-2.38f,.23f},.085f},
            {{x+.08f,-length,.25f},.025f}
        }};
        // The two rear vertices of each top section meet the wall at Z=0.
        root(out,spine,1.1547005f,seed+stem);
        for (int branch=1;branch<=2;++branch) {
            const auto start=spine[branch].position;
            const float sign=(stem+branch)%2==0?1.0f:-1.0f;
            const auto end=start+glm::vec3(sign*.38f,-.64f,.07f);
            const std::array<RootPoint,3> twig{{
                {start,.065f},{start+glm::vec3(sign*.26f,-.30f,.05f),.045f},{end,.018f}
            }};
            root(out,twig,.8f,seed+stem*7+branch);
            leaf(out,end,end+glm::vec3(sign*.15f,-.18f,.03f),.10f,Moss*1.15f);
        }
    }
}

} // namespace

void appendHubNature(MeshData& mesh,const HubAsset& asset)
{
    AssetBuilder out(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::Boulder: boulder(out,asset.variation); break;
    case HubAssetKind::RockLedge: rockLedge(out,asset.variation); break;
    case HubAssetKind::StalactiteCluster: stalactiteCluster(out,asset.variation); break;
    case HubAssetKind::RootArch: rootArch(out,asset.variation); break;
    case HubAssetKind::RootCurtain: rootCurtain(out,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
