#include "graphics/jungle_shapes.h"

#include <array>
#include <cmath>

namespace voxel {
namespace {

using jungle::leaf;
using jungle::noise;
using jungle::stem;
constexpr float Pi=jungle::Pi;
constexpr glm::vec3 Bark{.37f,.24f,.12f};
constexpr glm::vec3 Wood{.64f,.46f,.24f};
constexpr glm::vec3 Stone{.39f,.48f,.43f};
constexpr glm::vec3 Moss{.28f,.52f,.16f};
constexpr glm::vec3 YoungMoss{.47f,.64f,.22f};
constexpr glm::vec3 Jade{.16f,.45f,.24f};

// Clipped corners and a sloping shoulder give every stone a solid, chipped
// silhouette. The walking face stays level, including on the ruin pieces.
void chippedStone(AssetBuilder& b, glm::vec3 lo, glm::vec3 hi, float bevel,
                  glm::vec3 color, std::uint32_t seed, Block material=Block::Stone)
{
    const float x=lo.x,z=lo.z,X=hi.x,Z=hi.z;
    const float cut=std::min(hi.x-lo.x,hi.z-lo.z)*.16f;
    const std::array<glm::vec2,8> ring{{
        {x+cut,z},{X-cut*.8f,z},{X,z+cut},{X,Z-cut*.8f},
        {X-cut,Z},{x+cut*.8f,Z},{x,Z-cut},{x,z+cut*.8f}}};
    std::array<glm::vec3,8> foot{},shoulder{},crown{};
    const auto center=(lo+hi)*.5f;
    const float inset=std::min(bevel,std::min(hi.x-lo.x,hi.z-lo.z)*.18f);
    for (int i=0;i<8;++i) {
        const auto p=ring[i];
        const auto toward=glm::normalize(glm::vec2(center.x-p.x,center.z-p.y));
        foot[i]={p.x,lo.y,p.y};
        shoulder[i]={p.x,hi.y-bevel,p.y};
        const auto q=p+toward*inset*(.8f+.2f*noise(seed,i));
        crown[i]={q.x,hi.y,q.y};
    }
    const glm::vec3 top(center.x,hi.y,center.z), bottom(center.x,lo.y,center.z);
    for (int i=0;i<8;++i) {
        const int next=(i+1)%8;
        const auto tint=color*(.87f+.15f*noise(seed,i+17));
        b.quad(foot[next],foot[i],shoulder[i],shoulder[next],material,tint*.81f);
        b.quad(shoulder[next],shoulder[i],crown[i],crown[next],material,tint);
        b.triangle(top,crown[next],crown[i],material,tint*1.04f);
        b.triangle(bottom,foot[i],foot[next],material,tint*.66f);
    }
}

// Moss is a small closed cushion, with visible tapered edges rather than a
// green decal hovering over the underlying rock.
void mossPad(AssetBuilder& b, glm::vec3 base, glm::vec2 radius, float height,
             std::uint32_t seed, glm::vec3 color=Moss)
{
    std::array<glm::vec3,7> lower{},upper{};
    for (int i=0;i<7;++i) {
        const float angle=2*Pi*float(i)/7;
        const float uneven=.84f+.16f*noise(seed,i+4);
        const glm::vec3 side(std::cos(angle)*radius.x*uneven,0,std::sin(angle)*radius.y*uneven);
        lower[i]=base+side;
        upper[i]=base+side*.84f+glm::vec3(0,height,0);
    }
    for (int i=0;i<7;++i) {
        const int next=(i+1)%7;
        const auto colorSide=color*(.82f+.16f*noise(seed,i+18));
        b.quad(lower[next],lower[i],upper[i],upper[next],Block::Moss,colorSide);
        b.triangle(base+glm::vec3(0,height,0),upper[next],upper[i],Block::Moss,colorSide*1.06f);
        b.triangle(base,lower[i],lower[next],Block::Moss,colorSide*.65f);
    }
}

void sprig(AssetBuilder& b, glm::vec3 base, float size, float turn, std::uint32_t seed)
{
    const glm::vec3 direction(std::cos(turn),0,std::sin(turn));
    stem(b,base,base+glm::vec3(0,size*.70f,0),size*.045f,size*.025f,Jade,Block::Leaves);
    for (int i=0;i<3;++i) {
        const float a=turn+float(i)*2.35f;
        const glm::vec3 out(std::cos(a),0,std::sin(a));
        leaf(b,base+glm::vec3(0,size*.22f,0),base+out*size*.58f+glm::vec3(0,size*(.62f+.1f*noise(seed,i)),0),size*.18f,
             i%2?Jade:YoungMoss);
    }
    leaf(b,base+glm::vec3(0,size*.32f,0),base+direction*size*.17f+glm::vec3(0,size,0),size*.15f,YoungMoss);
}

void wallVine(AssetBuilder& b, glm::vec3 top, float length, std::uint32_t seed)
{
    glm::vec3 previous=top;
    for (int i=1;i<=6;++i) {
        const float t=float(i)/6;
        const auto p=top+glm::vec3(std::sin(t*5.1f)*.10f,-length*t,.015f*std::sin(t*4));
        stem(b,previous,p,.025f-.013f*t,.023f-.013f*t,Jade,Block::RootWood);
        const float side=i%2?1.f:-1.f;
        leaf(b,p,p+glm::vec3(side*(.15f+.04f*noise(seed,i)),.08f,.06f),.075f,
             i%2?Moss:YoungMoss,{0,0,1});
        previous=p;
    }
}

void rootBridge(AssetBuilder& b, std::uint32_t seed)
{
    // Three interlocked roots have a broad, level crown. Facets and bark run
    // along the sides while the center remains a continuous walkable lane.
    constexpr std::array<float,5> sections{-2,-1.2f,0,1.2f,2};
    for (int root=0;root<3;++root) {
        const float center=float(root-1)*.41f;
        std::array<std::array<glm::vec3,8>,5> rings{};
        for (int joint=0;joint<5;++joint) {
            const float bulge=joint==0 || joint==4?.95f:1.0f;
            const float width=.285f*bulge;
            const float offset=(joint==0 || joint==4)?0:.025f*std::sin(float(joint*2+root));
            const float x=center+offset,z=sections[joint];
            rings[joint]={glm::vec3{x-width*.72f,.55f,z},{x+width*.72f,.55f,z},
                {x+width,.43f,z},{x+width,.20f,z},{x+width*.62f,0,z},
                {x-width*.62f,0,z},{x-width,.20f,z},{x-width,.43f,z}};
        }
        for (int joint=0;joint<4;++joint) for (int side=0;side<8;++side) {
            const int next=(side+1)%8;
            const auto color=(side==0?Wood*.79f:Bark)*(.87f+.15f*noise(seed,root*41+side));
            b.quad(rings[joint][next],rings[joint][side],rings[joint+1][side],rings[joint+1][next],Block::RootWood,color);
        }
        for (int side=0;side<8;++side) {
            const int next=(side+1)%8;
            b.triangle({center,.28f,-2},rings[0][side],rings[0][next],Block::RootWood,Wood*.78f);
            b.triangle({center,.28f,2},rings[4][next],rings[4][side],Block::RootWood,Wood*.78f);
        }
    }
    for (const float side:{-1.f,1.f}) {
        for (int i=0;i<4;++i) {
            const float z=-1.78f+float(i)*.92f;
            stem(b,{side*.64f,.30f,z},{side*.65f,.38f,z+.57f},.043f,.024f,Bark*.68f);
            if (i%2==0) mossPad(b,{side*.58f,.55f,z+.20f},{.12f,.28f},.045f,seed+i,Moss);
        }
    }
}

void rootStump(AssetBuilder& b, std::uint32_t seed)
{
    // A nine-sided trunk with a broad broken crown, visible heartwood and
    // concentric ridges. The radial buttresses finish on the same ground.
    constexpr int Sides=9;
    std::array<glm::vec3,Sides> foot{},waist{},rim{};
    for (int i=0;i<Sides;++i) {
        const float a=float(i)*2*Pi/Sides;
        const glm::vec3 out(std::cos(a),0,std::sin(a));
        foot[i]=out*(.58f+.04f*noise(seed,i));
        waist[i]=out*.43f+glm::vec3(0,.54f,0);
        rim[i]=out*.46f+glm::vec3(0,.94f+.12f*noise(seed,i+30),0);
    }
    for (int i=0;i<Sides;++i) {
        const int next=(i+1)%Sides;
        const auto color=Bark*(.77f+.28f*noise(seed,i+10));
        b.quad(foot[next],foot[i],waist[i],waist[next],Block::RootWood,color);
        b.quad(waist[next],waist[i],rim[i],rim[next],Block::RootWood,color*.94f);
        b.triangle({0,0,0},foot[i],foot[next],Block::RootWood,color*.70f);
        b.triangle({0,.93f,0},rim[next],rim[i],Block::RootWood,Wood*(.84f+.1f*noise(seed,i+20)));
    }
    for (int ring=0;ring<3;++ring) {
        const float r=.12f+float(ring)*.105f;
        for (int i=0;i<Sides;++i) {
            const float a=float(i)*2*Pi/Sides, next=float(i+1)*2*Pi/Sides;
            stem(b,{r*std::cos(a),.955f+float(ring)*.012f,r*std::sin(a)},
                 {r*std::cos(next),.955f+float(ring)*.012f,r*std::sin(next)},.013f,.013f,Bark*.82f);
        }
    }
    for (int i=0;i<5;++i) {
        const float a=float(i)*2*Pi/5+.2f;
        const glm::vec3 out(std::cos(a),0,std::sin(a));
        const auto elbow=out*.59f+glm::vec3(0,.17f,0);
        stem(b,out*.32f+glm::vec3(0,.52f,0),elbow,.15f,.105f,Bark);
        stem(b,elbow,out*(.88f+.12f*noise(seed,i+50))+glm::vec3(0,.046f,0),.105f,.025f,Bark*.86f);
    }
    mossPad(b,{-.22f,.97f,.19f},{.20f,.15f},.10f,seed+70);
    sprig(b,{-.27f,1.07f,.20f},.27f,.6f,seed);
    // Raised bark strips catch side lighting, but stay attached to the trunk.
    for (int i=0;i<6;++i) {
        const float a=float(i)*Pi/3;
        const glm::vec3 out(std::cos(a),0,std::sin(a));
        stem(b,out*.48f+glm::vec3(0,.27f,0),out*.452f+glm::vec3(0,.84f,0),.028f,.020f,Bark*.65f);
    }
}

void mossyPlatform(AssetBuilder& b, std::uint32_t seed)
{
    chippedStone(b,{-1,0,-1},{1,.60f,1},.12f,Stone,seed);
    mossPad(b,{-.68f,.60f,-.68f},{.23f,.23f},.065f,seed);
    mossPad(b,{.68f,.60f,.68f},{.23f,.23f},.065f,seed+1,YoungMoss*.94f);
    mossPad(b,{-.68f,.60f,.68f},{.23f,.23f},.06f,seed+2);
    sprig(b,{-.61f,.66f,.55f},.24f,.8f,seed+3);
    // A low mineral band and broken corner add relief to the vertical face.
    chippedStone(b,{-.74f,.18f,.92f},{-.14f,.25f,1.015f},.018f,Stone*.79f,seed+4);
    chippedStone(b,{.22f,.22f,.92f},{.70f,.31f,1.012f},.022f,Stone*.88f,seed+5);
}

void mossyColumn(AssetBuilder& b, std::uint32_t seed)
{
    chippedStone(b,{-.5f,0,-.5f},{.5f,.24f,.5f},.05f,Stone*.85f,seed);
    chippedStone(b,{-.43f,.19f,-.42f},{.40f,.92f,.41f},.08f,Stone,seed+1);
    chippedStone(b,{-.39f,.89f,-.40f},{.44f,1.62f,.40f},.07f,Stone*.93f,seed+2);
    chippedStone(b,{-.5f,1.57f,-.5f},{.5f,2,.5f},.09f,Stone*1.04f,seed+3);
    mossPad(b,{-.40f,2,-.40f},{.07f,.07f},.05f,seed+4);
    mossPad(b,{.40f,2,.40f},{.07f,.07f},.05f,seed+5,YoungMoss);
    wallVine(b,{-.27f,1.94f,.47f},1.37f,seed+6);
    stem(b,{.36f,.12f,.40f},{.46f,.63f,.31f},.036f,.024f,Bark*.88f);
    stem(b,{.46f,.63f,.31f},{.43f,1.18f,.14f},.024f,.016f,Bark*.88f);
}

// One extruded arch stone. A visible joint separates adjacent wedges.
void archStone(AssetBuilder& b, float angleA, float angleB, glm::vec3 color)
{
    constexpr float Inner=1.10f, Outer=1.55f, Spring=2.10f, Depth=.36f;
    const auto p=[](float angle,float radius,float z) {
        return glm::vec3(std::cos(angle)*radius,Spring+std::sin(angle)*radius,z);
    };
    const std::array<glm::vec3,4> face{p(angleA,Inner,Depth),p(angleA,Outer,Depth),
        p(angleB,Outer,Depth),p(angleB,Inner,Depth)};
    b.quad(face[0],face[1],face[2],face[3],Block::MossBrick,color);
    b.quad(face[3]-glm::vec3(0,0,2*Depth),face[2]-glm::vec3(0,0,2*Depth),
           face[1]-glm::vec3(0,0,2*Depth),face[0]-glm::vec3(0,0,2*Depth),Block::MossBrick,color*.91f);
    for (int i=0;i<4;++i) {
        const int next=(i+1)%4;
        b.quad(face[next],face[i],face[i]-glm::vec3(0,0,2*Depth),face[next]-glm::vec3(0,0,2*Depth),
               Block::MossBrick,color*(i==2?.76f:1.02f));
    }
}

void overgrownArch(AssetBuilder& b, std::uint32_t seed)
{
    for (const float side:{-1.f,1.f}) {
        const float x=side*1.34f;
        for (int row=0;row<5;++row)
            chippedStone(b,{x-.25f,float(row)*.45f,-.37f},{x+.25f,float(row)*.45f+.44f,.37f},
                         .04f,Stone*(row%2?.91f:1.f),seed+row,Block::MossBrick);
        chippedStone(b,{x-.29f,0,-.43f},{x+.29f,.18f,.43f},.035f,Stone*.87f,seed+7);
    }
    for (int i=0;i<9;++i)
        archStone(b,float(i)*Pi/9+.009f,float(i+1)*Pi/9-.009f,Stone*(.91f+.13f*noise(seed,i+31)));
    // Roots cling to the outside of the opening, never cross its travel lane.
    stem(b,{-1.55f,.08f,.39f},{-1.54f,1.32f,.40f},.055f,.040f,Bark);
    stem(b,{-1.54f,1.32f,.40f},{-1.48f,2.22f,.39f},.040f,.032f,Bark);
    stem(b,{-1.48f,2.22f,.39f},{-1.04f,3.09f,.38f},.032f,.021f,Bark);
    wallVine(b,{1.34f,2.45f,.40f},1.36f,seed+60);
    mossPad(b,{0,3.64f,0},{.29f,.27f},.08f,seed+61);
    mossPad(b,{-.75f,3.36f,-.04f},{.24f,.25f},.09f,seed+62);
    sprig(b,{-.71f,3.43f,.03f},.30f,.2f,seed+63);
}

void ruinedPillar(AssetBuilder& b, std::uint32_t seed)
{
    chippedStone(b,{-.50f,0,-.50f},{.50f,.24f,.50f},.05f,Stone*.9f,seed,Block::MossBrick);
    for (int row=0;row<4;++row) {
        const float y=.24f+float(row)*.41f;
        chippedStone(b,{-.34f,y,-.34f},{.34f,y+.40f,.34f},.045f,
                     Stone*(row%2?.9f:1.02f),seed+row+1,Block::MossBrick);
    }
    chippedStone(b,{-.48f,1.87f,-.47f},{.48f,2.04f,.47f},.045f,Stone,seed+11,Block::MossBrick);
    // Two broken remnants retain the silhouette of a taller ancient column.
    chippedStone(b,{-.40f,2.01f,-.35f},{-.11f,2.32f,.31f},.06f,Stone*.92f,seed+12,Block::MossBrick);
    chippedStone(b,{.02f,2.01f,-.31f},{.33f,2.20f,.18f},.05f,Stone,seed+13,Block::MossBrick);
    mossPad(b,{-.25f,2.32f,.02f},{.14f,.20f},.055f,seed+14);
    wallVine(b,{-.18f,1.92f,.37f},1.28f,seed+15);
    // Narrow inset channels are formed between raised weathered face strips.
    for (const float x:{-.22f,.16f})
        chippedStone(b,{x,.57f,.335f},{x+.065f,1.19f,.37f},.009f,Stone*1.10f,seed+20,Block::MossBrick);
    sprig(b,{.36f,.24f,.30f},.28f,.5f,seed+21);
}

void ruinedWall(AssetBuilder& b, std::uint32_t seed)
{
    // A two-block wall with a broken stair-step crown. Individual stones have
    // actual recessed joints and chipped corners on every exposed side.
    constexpr std::array<int,4> rowCount{4,4,3,2};
    for (int row=0;row<4;++row) for (int column=0;column<rowCount[row];++column) {
        const float x=-1+float(column)*.50f;
        const float y=float(row)*.39f;
        chippedStone(b,{x,y,-.27f},{x+.49f,y+.38f,.27f},.04f,
                     Stone*(.89f+.16f*noise(seed,row*8+column)),seed+row*8+column,Block::MossBrick);
    }
    mossPad(b,{-.69f,1.55f,.02f},{.25f,.20f},.065f,seed+40);
    mossPad(b,{.24f,1.16f,0},{.20f,.21f},.06f,seed+41,YoungMoss);
    wallVine(b,{-.69f,1.53f,.29f},1.15f,seed+42);
    stem(b,{.88f,.04f,.30f},{.50f,.39f,.30f},.042f,.028f,Bark);
    stem(b,{.50f,.39f,.30f},{.49f,.78f,.29f},.028f,.018f,Bark);
    sprig(b,{.80f,.77f,-.05f},.27f,.3f,seed+43);
}

void ruinSlab(AssetBuilder& b, std::uint32_t seed)
{
    chippedStone(b,{-1,0,-1},{1,.26f,1},.06f,Stone*.94f,seed,Block::MossBrick);
    // The recessed square motif is surrounded by raised, broken stone bars.
    // Keep the center clear so a player stands directly on the slab itself.
    chippedStone(b,{-.57f,.26f,-.57f},{.17f,.285f,-.48f},.008f,Stone*1.08f,seed+1,Block::MossBrick);
    chippedStone(b,{.30f,.26f,-.57f},{.57f,.285f,-.48f},.008f,Stone*1.08f,seed+2,Block::MossBrick);
    chippedStone(b,{-.57f,.26f,-.44f},{-.48f,.285f,.57f},.008f,Stone*1.08f,seed+3,Block::MossBrick);
    chippedStone(b,{.48f,.26f,-.44f},{.57f,.285f,.57f},.008f,Stone*1.08f,seed+4,Block::MossBrick);
    chippedStone(b,{-.44f,.26f,.48f},{.44f,.285f,.57f},.008f,Stone*1.08f,seed+5,Block::MossBrick);
    mossPad(b,{-.73f,.26f,-.70f},{.20f,.21f},.045f,seed+6);
    mossPad(b,{.72f,.26f,.69f},{.23f,.20f},.045f,seed+7);
    sprig(b,{-.76f,.30f,-.68f},.22f,.4f,seed+8);
}

} // namespace

void appendJungleStructures(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder builder(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::RootBridge: rootBridge(builder,asset.variation); break;
    case HubAssetKind::RootStump: rootStump(builder,asset.variation); break;
    case HubAssetKind::MossyPlatform: mossyPlatform(builder,asset.variation); break;
    case HubAssetKind::MossyColumn: mossyColumn(builder,asset.variation); break;
    case HubAssetKind::OvergrownArch: overgrownArch(builder,asset.variation); break;
    case HubAssetKind::RuinedPillar: ruinedPillar(builder,asset.variation); break;
    case HubAssetKind::RuinedWall: ruinedWall(builder,asset.variation); break;
    case HubAssetKind::RuinSlab: ruinSlab(builder,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
