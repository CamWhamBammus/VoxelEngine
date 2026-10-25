#include "graphics/jungle_shapes.h"

#include <array>
#include <cmath>

namespace voxel {
namespace {

using jungle::leaf;
using jungle::noise;
using jungle::stem;
constexpr float Pi=jungle::Pi;
constexpr glm::vec3 Bark{.31f,.23f,.12f};
constexpr glm::vec3 Stem{.24f,.36f,.12f};
constexpr glm::vec3 Jade{.19f,.49f,.26f};
constexpr glm::vec3 Lime{.46f,.63f,.22f};
constexpr glm::vec3 Moss{.29f,.45f,.17f};
constexpr glm::vec3 DeepGreen{.12f,.32f,.23f};

glm::vec3 radial(float turn) { return {std::cos(turn),0,std::sin(turn)}; }

void groundCrown(AssetBuilder& b, float width)
{
    b.box({-width,0,-width},{width,.09f,width},Block::RootWood,Bark*.78f);
    stem(b,{0,.07f,0},{0,.22f,0},width*1.13f,width*.65f,Bark);
}

// A split blade has actual gaps between its lobes, rather than painted holes.
void splitLeaf(AssetBuilder& b, glm::vec3 foot, glm::vec3 tip, float width, glm::vec3 tint)
{
    const auto along=glm::normalize(tip-foot);
    const auto side=glm::normalize(glm::cross(along,glm::vec3(0,1,0)));
    const auto up=glm::normalize(glm::cross(side,along));
    leaf(b,foot,tip,width*.66f,tint);
    for (int lobe=0;lobe<3;++lobe) {
        const float t=.15f+float(lobe)*.22f;
        const float span=width*(lobe==1?1.0f:.87f);
        for (const float sign:{-1.0f,1.0f}) {
            const auto start=glm::mix(foot,tip,t);
            const auto end=glm::mix(foot,tip,t+.33f)+side*span*sign+up*(.055f-float(lobe)*.020f);
            leaf(b,start,end,width*.40f,tint*(sign>0?.93f:1.0f));
            stem(b,start+up*.011f,glm::mix(start,end,.72f)+up*.023f,.010f,.004f,Lime*.75f,Block::Leaves);
        }
    }
    stem(b,foot+up*.012f,glm::mix(foot,tip,.88f)+up*.026f,.015f,.005f,Lime*.76f,Block::Leaves);
}

void monstera(AssetBuilder& b, std::uint32_t seed)
{
    groundCrown(b,.11f);
    const float turn=noise(seed,1)*2*Pi;
    for (int frond=0;frond<5;++frond) {
        const auto out=radial(turn+float(frond)*2.4f);
        const float height=.48f+.15f*float(frond)+noise(seed,frond+10)*.12f;
        const auto foot=glm::vec3(0,.12f,0)+out*.04f;
        const auto elbow=out*.19f+glm::vec3(0,height*.66f,0);
        const auto blade=out*.28f+glm::vec3(0,height,0);
        stem(b,foot,elbow,.038f,.025f,Stem,Block::Leaves);
        stem(b,elbow,blade,.025f,.018f,Stem,Block::Leaves);
        splitLeaf(b,blade,blade+out*(.90f+.12f*noise(seed,frond+20))+glm::vec3(0,-.19f,.0f),
                  .47f+noise(seed,frond+30)*.07f,frond%2?Jade:DeepGreen*1.15f);
    }
    // Younger broad leaves fill the crown below the tall stalks. Offset them
    // from the upper leaves so the split edges layer without hiding every gap.
    for (int frond=0;frond<3;++frond) {
        const auto out=radial(turn+.71f+float(frond)*2*Pi/3);
        const auto blade=out*.15f+glm::vec3(0,.25f+.055f*float(frond),0);
        stem(b,{0,.13f,0},blade,.027f,.017f,Stem,Block::Leaves);
        splitLeaf(b,blade,blade+out*.73f+glm::vec3(0,-.075f,0),.36f,Jade*.91f);
    }
    leaf(b,{.02f,.18f,0},{.20f,.73f,.12f},.13f,Lime,{0,0,1});
}

void cycad(AssetBuilder& b, std::uint32_t seed)
{
    groundCrown(b,.17f);
    stem(b,{0,.12f,0},{.02f,.77f,0},.22f,.15f,Bark);
    const float turn=noise(seed,1)*Pi*2;
    for (int ring=0;ring<3;++ring) for (int scale=0;scale<6;++scale) {
        const auto out=radial(turn+(float(scale)+float(ring)*.5f)*Pi/3);
        const auto foot=out*.19f+glm::vec3(0,.18f+float(ring)*.16f,0);
        leaf(b,foot,foot+out*.045f+glm::vec3(0,.21f,0),.075f,Bark*1.28f,out);
    }
    for (int frond=0;frond<7;++frond) {
        const auto out=radial(turn+float(frond)*2*Pi/7);
        const auto across=glm::vec3(-out.z,0,out.x);
        const float reach=1.17f+noise(seed,frond+10)*.22f;
        const auto point=[&](float t) {
            return glm::vec3(.02f,.77f,0)+out*reach*t
                +glm::vec3(0,.70f*std::sin(t*Pi*.88f)-.43f*t*t,0);
        };
        for (int section=1;section<=6;++section) {
            const float t=float(section)/6;
            stem(b,point(t-1.0f/6),point(t),.025f*(1-t*.65f),.019f*(1-t*.65f),Stem,Block::Leaves);
            for (const float side:{-1.0f,1.0f}) {
                const float wing=.43f*(1-t*.60f);
                leaf(b,point(t),point(t)+across*side*wing+out*.24f+glm::vec3(0,-.095f,0),
                     wing*.42f,frond%3?Jade:Lime*.86f);
            }
        }
        leaf(b,point(.86f),point(1)+out*.20f+glm::vec3(0,-.085f,0),.10f,Lime*.89f);
    }
    for (int i=0;i<3;++i) {
        const auto out=radial(turn+float(i)*2*Pi/3);
        leaf(b,{.02f,.77f,0},out*.21f+glm::vec3(0,1.25f,0),.085f,Lime);
    }
    for (int i=0;i<4;++i) {
        const auto out=radial(turn+.40f+float(i)*Pi/2);
        leaf(b,{0,.32f,0},out*.55f+glm::vec3(0,.23f,0),.15f,Jade*.92f);
    }
}

void bamboo(AssetBuilder& b, std::uint32_t seed)
{
    groundCrown(b,.17f);
    const float turn=noise(seed,1)*2*Pi;
    for (int stalk=0;stalk<4;++stalk) {
        const auto out=radial(turn+float(stalk)*2.4f);
        const auto foot=out*(stalk==0?.04f:.28f);
        const float height=1.72f+noise(seed,stalk+3)*1.10f;
        const auto point=[&](float t) { return foot+out*t*.23f+glm::vec3(0,.08f+height*t,0); };
        stem(b,foot,point(0),.082f,.078f,Moss,Block::RootWood);
        for (int joint=0;joint<5;++joint) {
            const float t=float(joint)/5;
            const float radius=.078f*(1-t*.34f);
            const auto p=point(t);
            stem(b,p,point(t+.2f),radius,radius*.94f,stalk%2?Moss:Jade*.85f,Block::RootWood);
            stem(b,p+glm::vec3(0,.018f,0),p+glm::vec3(0,.055f,0),radius*1.18f,radius*1.10f,Lime*.85f,Block::RootWood);
            if (joint<2) continue;
            const auto direction=radial(turn+float(stalk)*2.4f+float(joint)*1.7f);
            const auto branch=p+direction*.37f+glm::vec3(0,.12f,0);
            stem(b,p,branch,.018f,.008f,Stem,Block::Leaves);
            for (int blade=0;blade<3;++blade) {
                const auto spread=radial(turn+float(stalk)*2.4f+float(joint)*1.7f+(float(blade)-1)*.62f);
                leaf(b,glm::mix(p,branch,.52f+.21f*float(blade)),branch+spread*.40f+glm::vec3(0,.15f-float(blade)*.14f,0),
                     .075f,blade==0?Lime*.86f:Jade);
            }
        }
        leaf(b,point(.96f),point(1)+out*.12f+glm::vec3(0,.21f,0),.07f,Lime);
    }
}

void fiddleheads(AssetBuilder& b, std::uint32_t seed)
{
    groundCrown(b,.10f);
    const float turn=noise(seed,1)*2*Pi;
    for (int frond=0;frond<5;++frond) {
        const auto out=radial(turn+float(frond)*2.4f);
        const auto across=glm::vec3(-out.z,0,out.x);
        const float height=.63f+noise(seed,frond+10)*.55f;
        const auto bottom=out*.14f+glm::vec3(0,.07f,0);
        const auto neck=out*.20f+glm::vec3(0,height,0);
        const auto elbow=glm::mix(bottom,neck,.5f)-out*.07f;
        stem(b,bottom,elbow,.035f,.028f,Stem,Block::Leaves);
        stem(b,elbow,neck,.028f,.023f,Lime*.78f,Block::Leaves);
        // Growing fronds end in a curled solid spiral, not a round sprite.
        auto previous=neck;
        for (int curl=1;curl<=9;++curl) {
            const float t=float(curl)/9;
            const float angle=t*Pi*1.72f;
            const float radius=.17f*(1-t*.61f);
            const auto p=neck+out*(.17f-radius*std::cos(angle))+glm::vec3(0,radius*std::sin(angle),0);
            stem(b,previous,p,.024f-t*.009f,.023f-t*.009f,Lime*.91f,Block::Leaves);
            previous=p;
        }
        for (int tier=1;tier<=3;++tier) {
            const float t=float(tier)/4;
            const auto center=glm::mix(bottom,neck,t);
            for (float side:{-1.0f,1.0f})
                leaf(b,center,center+across*(.27f-t*.09f)*side+out*.14f+glm::vec3(0,.11f,0),
                     .082f,Jade);
        }
    }
}

void hangingFern(AssetBuilder& b, std::uint32_t seed)
{
    b.box({-.15f,-.13f,-.15f},{.15f,0,.15f},Block::RootWood,Bark);
    const float turn=noise(seed,1)*2*Pi;
    for (int frond=0;frond<7;++frond) {
        const auto out=radial(turn+float(frond)*2*Pi/7);
        const auto across=glm::vec3(-out.z,0,out.x);
        const float reach=.66f+noise(seed,frond+10)*.21f;
        const float depth=1.25f+noise(seed,frond+20)*.78f;
        const auto point=[&](float t) {
            return out*reach*std::sin(t*Pi*.64f)+glm::vec3(0,-.13f-depth*t,0);
        };
        for (int joint=1;joint<=6;++joint) {
            const float t=float(joint)/6;
            stem(b,point(t-1.0f/6),point(t),.022f*(1-t*.55f),.016f*(1-t*.55f),Stem,Block::Leaves);
            const float wing=.28f*(1-t*.66f);
            for (float side:{-1.0f,1.0f})
                leaf(b,point(t),point(t)+across*wing*side+out*.035f+glm::vec3(0,-.16f,0),
                     wing*.28f,frond%3?Jade:Lime*.84f,out);
        }
        leaf(b,point(.92f),point(1)+glm::vec3(0,-.21f,0),.07f,Lime*.85f,out);
    }
}

void mossDrape(AssetBuilder& b, std::uint32_t seed)
{
    // A mossy wall attachment lies flush on local Z=0. Ragged ropes spread
    // over its face and hang below it, with little three-dimensional shoots.
    b.box({-.52f,-.13f,0},{.52f,0,.07f},Block::RootWood,DeepGreen);
    for (int rope=0;rope<9;++rope) {
        const float x=(float(rope)-4)*.12f;
        const float depth=.68f+noise(seed,rope+1)*1.18f;
        const auto point=[&](float t) {
            return glm::vec3(x+std::sin(t*5+float(rope))*.055f,-.08f-depth*t,.08f+std::sin(t*Pi)*.07f);
        };
        for (int joint=1;joint<=5;++joint) {
            const float t=float(joint)/5;
            stem(b,point(t-.2f),point(t),.035f*(1-t*.65f),.028f*(1-t*.65f),Moss,Block::Leaves);
            for (float side:{-1.0f,1.0f})
                leaf(b,point(t),point(t)+glm::vec3(side*(.08f+noise(seed,rope*7+joint)*.04f),-.10f,.05f),
                     .045f,(rope+joint)%3?Moss:Lime*.78f,{0,0,1});
        }
    }
    for (int tuft=0;tuft<7;++tuft) {
        const auto foot=glm::vec3((float(tuft)-3)*.15f,-.09f,.10f);
        leaf(b,foot,foot+glm::vec3(.04f,-.17f,.24f),.085f,Moss*1.05f,{0,0,1});
    }
}

// Lathed polygonal body, thick rim and dark inner bowl. The hollow mouth
// remains visible from above; the plant has no transparent or flat surfaces.
void pitcher(AssetBuilder& b, glm::vec3 foot, float height, float width, float turn)
{
    constexpr int Sides=8;
    constexpr std::array<float,5> radii{.38f,.92f,1.0f,.63f,.72f};
    constexpr std::array<float,5> heights{0,.21f,.52f,.81f,1};
    std::array<std::array<glm::vec3,Sides>,5> rings{};
    const auto lean=radial(turn)*height*.12f;
    for (int tier=0;tier<5;++tier) for (int side=0;side<Sides;++side)
        rings[tier][side]=foot+lean*heights[tier]+radial(turn+float(side)*Pi/4)*width*radii[tier]
            +glm::vec3(0,height*heights[tier],0);
    for (int tier=0;tier<4;++tier) for (int side=0;side<Sides;++side) {
        const int next=(side+1)%Sides;
        const glm::vec3 tint=tier<2?glm::vec3(.39f,.47f,.21f):glm::vec3(.49f,.32f,.26f);
        b.quad(rings[tier][side],rings[tier+1][side],rings[tier+1][next],rings[tier][next],
               Block::Leaves,tint*(side%2?.91f:1.0f));
    }
    const auto mouth=foot+lean+glm::vec3(0,height,0);
    for (int side=0;side<Sides;++side) {
        const int next=(side+1)%Sides;
        const auto dir=radial(turn+float(side)*Pi/4), nextDir=radial(turn+float(next)*Pi/4);
        const auto inner=mouth+dir*width*.52f, innerNext=mouth+nextDir*width*.52f;
        b.quad(rings[4][side],inner,innerNext,rings[4][next],Block::Leaves,{.65f,.46f,.28f});
        const auto bowl=mouth-glm::vec3(0,height*.23f,0);
        b.triangle(bowl,innerNext,inner,Block::Leaves,{.14f,.20f,.12f});
        b.triangle(foot,rings[0][side],rings[0][next],Block::Leaves,Moss*.70f);
        if (side%2==0)
            stem(b,glm::mix(rings[1][side],rings[2][side],.15f),rings[3][side],width*.025f,width*.013f,
                 Lime*.78f,Block::Leaves);
    }
    const auto back=mouth-radial(turn)*width*.61f;
    const auto lid=back+glm::vec3(0,height*.18f,0)-radial(turn)*width*.15f;
    stem(b,back,lid,.015f,.012f,Stem,Block::Leaves);
    leaf(b,lid,lid+radial(turn)*width*1.4f+glm::vec3(0,-.045f,0),width*.69f,Lime*.88f);
}

void pitcherPlants(AssetBuilder& b, std::uint32_t seed)
{
    groundCrown(b,.13f);
    const float turn=noise(seed,1)*Pi*2;
    for (int plant=0;plant<3;++plant) {
        const auto out=radial(turn+float(plant)*2*Pi/3);
        const auto foot=out*.29f+glm::vec3(0,.10f,0);
        pitcher(b,foot,.65f+noise(seed,plant+10)*.38f,.19f+noise(seed,plant+20)*.035f,turn+float(plant)*2*Pi/3);
        stem(b,{0,.12f,0},foot+glm::vec3(0,.13f,0),.024f,.014f,Stem,Block::Leaves);
    }
    for (int blade=0;blade<7;++blade) {
        const auto out=radial(turn+float(blade)*2*Pi/7);
        leaf(b,{0,.16f,0},out*(.65f+noise(seed,blade+30)*.12f)+glm::vec3(0,.10f,0),.17f,Jade);
    }
}

void pod(AssetBuilder& b, glm::vec3 center, float height, float width, float turn, glm::vec3 tint)
{
    constexpr int Sides=6;
    std::array<glm::vec3,Sides> lower{},middle{},upper{};
    for (int side=0;side<Sides;++side) {
        const auto out=radial(turn+float(side)*Pi/3);
        lower[side]=center+out*width*.63f-glm::vec3(0,height*.30f,0);
        middle[side]=center+out*width;
        upper[side]=center+out*width*.62f+glm::vec3(0,height*.30f,0);
    }
    for (int side=0;side<Sides;++side) {
        const int next=(side+1)%Sides;
        b.triangle(center-glm::vec3(0,height*.55f,0),lower[side],lower[next],Block::Leaves,tint*.80f);
        b.quad(lower[side],middle[side],middle[next],lower[next],Block::Leaves,tint*(side%2?.92f:1.0f));
        b.quad(middle[side],upper[side],upper[next],middle[next],Block::Leaves,tint);
        b.triangle(center+glm::vec3(0,height*.55f,0),upper[next],upper[side],Block::Leaves,tint*.92f);
        // Raised seams catch light independently of the flat pod facets.
        const auto out=radial(turn+float(side)*Pi/3);
        stem(b,lower[side]+out*.005f,middle[side]+out*.008f,.012f,.016f,Lime*.76f,Block::Leaves);
        stem(b,middle[side]+out*.008f,upper[side]+out*.005f,.016f,.010f,Lime*.76f,Block::Leaves);
    }
}

void seedPods(AssetBuilder& b, std::uint32_t seed)
{
    groundCrown(b,.12f);
    const float turn=noise(seed,1)*Pi*2;
    stem(b,{0,.10f,0},{.03f,1.18f,0},.065f,.031f,Stem);
    for (int fruit=0;fruit<4;++fruit) {
        const auto out=radial(turn+float(fruit)*2.4f);
        const auto fork=glm::vec3(0,.51f+float(fruit)*.18f,0);
        const auto tip=fork+out*.30f+glm::vec3(0,.29f,0);
        stem(b,fork,tip,.026f,.016f,Stem);
        const float height=.40f+noise(seed,fruit+10)*.12f;
        const auto center=tip-glm::vec3(0,height*.51f,0);
        pod(b,center,height,.15f+noise(seed,fruit+20)*.045f,turn+float(fruit),
            fruit%2?glm::vec3(.46f,.47f,.22f):glm::vec3(.61f,.42f,.19f));
        leaf(b,fork,tip+out*.28f+glm::vec3(0,.15f,0),.18f,Jade);
    }
    for (int i=0;i<5;++i) {
        const auto out=radial(turn+float(i)*2*Pi/5);
        leaf(b,{0,.15f,0},out*.55f+glm::vec3(0,.27f,0),.16f,Moss*1.15f);
    }
}

} // namespace

void appendJunglePlants(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder builder(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::JungleMonstera: monstera(builder,asset.variation); break;
    case HubAssetKind::JungleCycad: cycad(builder,asset.variation); break;
    case HubAssetKind::BambooClump: bamboo(builder,asset.variation); break;
    case HubAssetKind::FiddleheadFerns: fiddleheads(builder,asset.variation); break;
    case HubAssetKind::HangingFern: hangingFern(builder,asset.variation); break;
    case HubAssetKind::MossDrape: mossDrape(builder,asset.variation); break;
    case HubAssetKind::PitcherPlants: pitcherPlants(builder,asset.variation); break;
    case HubAssetKind::SeedPodCluster: seedPods(builder,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
