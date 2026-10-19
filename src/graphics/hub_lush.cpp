#include "graphics/asset_builder.h"

#include <array>
#include <cmath>
#include <span>

namespace voxel {
namespace {

constexpr float Pi = 3.14159265359f;
constexpr glm::vec3 Bark{.34f,.21f,.105f};
constexpr glm::vec3 Stem{.16f,.34f,.12f};
constexpr glm::vec3 Jade{.18f,.57f,.29f};
constexpr glm::vec3 Lime{.48f,.71f,.20f};
constexpr glm::vec3 Teal{.17f,.57f,.50f};
constexpr glm::vec3 Cream{.84f,.77f,.54f};
constexpr glm::vec3 Pink{.90f,.37f,.65f};

float noise(std::uint32_t seed, std::uint32_t part)
{
    auto v = seed ^ (part * 0x9e3779b9u + 0x85ebca6bu);
    v = (v ^ (v >> 16)) * 0x7feb352du;
    v = (v ^ (v >> 15)) * 0x846ca68bu;
    return float((v ^ (v >> 16)) & 0x00ffffffu) / 16777216.0f;
}

glm::vec3 radial(float angle) { return {std::cos(angle),0,std::sin(angle)}; }

// Folded surfaces have deliberately open silhouettes. The geometry itself
// forms each leaf, so the block atlas's cutout pattern is not applied here.
void leaf(AssetBuilder& b, glm::vec3 foot, glm::vec3 tip, float width,
          glm::vec3 color, glm::vec3 facing = {0,1,0}, Block material = Block::Leaves)
{
    color = glm::clamp(color, glm::vec3(0), glm::vec3(1));
    const auto along = glm::normalize(tip-foot);
    if (std::abs(glm::dot(along,facing)) > .95f) facing = {0,0,1};
    const auto across = glm::normalize(glm::cross(along,facing));
    const auto normal = glm::normalize(glm::cross(across,along));
    const auto ridge = glm::mix(foot,tip,.48f)+normal*width*.18f;
    const auto left = glm::mix(foot,tip,.41f)+across*width;
    const auto right = glm::mix(foot,tip,.55f)-across*width*.88f;
    b.triangle(foot,left,ridge,material,color,true);
    b.triangle(left,tip,ridge,material,glm::min(color*1.07f,glm::vec3(1)),true);
    b.triangle(tip,right,ridge,material,color*.84f,true);
    b.triangle(right,foot,ridge,material,color*.76f,true);
}

// Six painted faces taper along a branch, stem or root. Caps keep ends solid.
void taper(AssetBuilder& b, glm::vec3 a, glm::vec3 z, float radiusA, float radiusZ,
           glm::vec3 color, Block material = Block::RootWood)
{
    const auto along = glm::normalize(z-a);
    const auto ref = std::abs(along.y) > .9f ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
    const auto across = glm::normalize(glm::cross(ref,along));
    const auto up = glm::cross(along,across);
    std::array<glm::vec3,6> lo{},hi{};
    for (int i=0;i<6;++i) {
        const auto direction = across*std::cos(float(i)*Pi/3)+up*std::sin(float(i)*Pi/3);
        lo[i]=a+direction*radiusA;
        hi[i]=z+direction*radiusZ;
    }
    for (int i=0;i<6;++i) {
        const int next=(i+1)%6;
        b.quad(lo[i],lo[next],hi[next],hi[i],material,color*(.77f+.055f*float(i)));
        b.triangle(a,lo[next],lo[i],material,color*.76f);
        b.triangle(z,hi[i],hi[next],material,color*1.07f);
    }
}

void rosette(AssetBuilder& b, glm::vec3 center, float radius, float height,
             glm::vec3 color, std::uint32_t seed, int count=7)
{
    const float turn=noise(seed,1)*Pi*2;
    for (int i=0;i<count;++i) {
        const auto direction=radial(turn+float(i)*Pi*2/float(count));
        const auto tip=center+direction*radius*(.83f+.17f*noise(seed,i+10))
            +glm::vec3(0,height*(.75f+.25f*noise(seed,i+20)),0);
        leaf(b,center,tip,radius*.24f,color*(.86f+.20f*noise(seed,i+30)));
    }
}

void flower(AssetBuilder& b, glm::vec3 center, float radius, glm::vec3 color,
            float turn, bool upright=false)
{
    // Outer petals flare, inner petals cup the warm center.
    for (int ring=0;ring<2;++ring) for (int i=0;i<6;++i) {
        const float angle=turn+(float(i)+float(ring)*.5f)*Pi/3;
        const float reach=radius*(ring==0?1.0f:.64f);
        const auto direction=upright?glm::vec3(std::cos(angle),std::sin(angle),0):radial(angle);
        const auto lift=upright?glm::vec3(0,0,reach*.23f):glm::vec3(0,reach*.3f,0);
        leaf(b,center+lift*.3f,center+direction*reach+lift,reach*.35f,
             color*(ring==0?.87f:1.13f),upright?glm::vec3(0,0,1):glm::vec3(0,1,0));
    }
    const auto heart=center+(upright?glm::vec3(0,0,.03f):glm::vec3(0,.03f,0));
    b.box(heart-glm::vec3(radius*.13f),heart+glm::vec3(radius*.13f),Block::Leaves,{.98f,.76f,.25f});
}

void treeFern(AssetBuilder& b, std::uint32_t seed)
{
    const float turn=noise(seed,1)*2*Pi;
    taper(b,{0,.12f,0},{.09f,1.27f,.03f},.20f,.12f,Bark);
    for (int i=0;i<5;++i) {
        const auto outward=radial(turn+float(i)*2*Pi/5);
        taper(b,outward*.38f+glm::vec3(0,.035f,0),{0,.45f,0},.045f,.10f,Bark*.86f);
    }
    for (int frond=0;frond<9;++frond) {
        const auto out=radial(turn+float(frond)*2*Pi/9);
        const auto across=glm::vec3(-out.z,0,out.x);
        const float reach=1.38f+.22f*noise(seed,frond+11);
        const float arch=.88f+.39f*noise(seed,frond+21);
        const auto point=[&](float t) {
            return glm::vec3(.09f,1.27f,.03f)+out*reach*t+glm::vec3(0,arch*std::sin(t*Pi*.86f),0);
        };
        for (int joint=1;joint<=6;++joint) {
            const float t=float(joint)/6;
            taper(b,point(t-1.0f/6),point(t),.024f*(1-t*.6f),.019f*(1-t*.6f),Stem,Block::Leaves);
            const float wing=.31f*(1-t*.67f);
            for (const float side:{-1.0f,1.0f}) {
                const auto tip=point(t)+across*wing*side+out*.17f+glm::vec3(0,.03f-t*.14f,0);
                leaf(b,point(t),tip,wing*.33f,frond%3==0?Lime:Jade);
            }
        }
        leaf(b,point(.82f),point(1)+out*.13f,.065f,Lime);
    }
    rosette(b,{.09f,1.28f,.03f},.29f,.73f,Lime,seed+31,5);
}

void sapling(AssetBuilder& b, std::uint32_t seed)
{
    const float turn=noise(seed,1)*2*Pi;
    const glm::vec3 fork(.12f,1.64f,-.04f);
    taper(b,{0,.09f,0},fork,.15f,.08f,Bark);
    taper(b,fork,{.02f,3.10f,.05f},.08f,.022f,Bark);
    for (int i=0;i<5;++i) {
        const auto dir=radial(turn+float(i)*2.4f);
        taper(b,dir*.34f+glm::vec3(0,.025f,0),{0,.40f,0},.025f,.07f,Bark);
        const auto joint=glm::vec3(.08f,1.14f+float(i)*.30f,0);
        const auto tip=joint+dir*(.59f+.12f*noise(seed,i+10))+glm::vec3(0,.27f,0);
        taper(b,joint,tip,.043f,.012f,Bark);
        rosette(b,tip,.67f,.33f,i%2?Jade:Teal,seed+i*11,6);
        rosette(b,tip+glm::vec3(0,.18f,0),.44f,.30f,Lime*.91f,seed+i*13,5);
    }
    rosette(b,{.02f,3.08f,.05f},.63f,.39f,Jade,seed+60);
    rosette(b,{.02f,3.27f,.05f},.35f,.42f,Lime,seed+61,5);
}

void hangingRoots(AssetBuilder& b, std::uint32_t seed)
{
    // Ceiling attachments terminate exactly at y=0; the model grows downward.
    for (int root=0;root<5;++root) {
        const float x=(float(root)-2)*.36f;
        const float depth=1.90f+.98f*noise(seed,root+1);
        const float z=(noise(seed,root+11)-.5f)*.65f;
        glm::vec3 prev(x,-.075f,z);
        b.box({x-.16f,-.13f,z-.13f},{x+.16f,0,z+.13f},Block::RootWood,Bark*.75f);
        for (int i=1;i<=5;++i) {
            const float t=float(i)/5;
            const auto p=glm::vec3(x+std::sin(t*4.0f+float(root))*.19f,-depth*t,z+std::sin(t*3.0f)*.12f);
            taper(b,prev,p,.09f*(1-t*.77f),.075f*(1-t*.84f),Bark*(.85f+.07f*float(root)));
            if (i==2 || i==4) {
                const float side=root%2?1.0f:-1.0f;
                const auto branch=p+glm::vec3(side*.22f,-.27f,.04f);
                taper(b,p,branch,.033f,.009f,Bark);
                leaf(b,branch,branch+glm::vec3(side*.13f,-.18f,.08f),.08f,Jade,{0,0,1});
            }
            prev=p;
        }
        leaf(b,prev,prev+glm::vec3(.09f,-.24f,.10f),.075f,Lime,{0,0,1});
    }
}

void blossomVines(AssetBuilder& b, std::uint32_t seed)
{
    for (int vine=0;vine<2;++vine) {
        const float side=vine==0?-1.0f:1.0f;
        const float length=2.37f+.36f*noise(seed,vine+1);
        auto previous=glm::vec3(side*.15f,-.026f,.032f);
        b.box(previous-glm::vec3(.035f,.02f,.032f),previous+glm::vec3(.035f,.02f,.032f),Block::RootWood,Bark);
        for (int i=1;i<=8;++i) {
            const float t=float(i)/8;
            const auto p=glm::vec3(side*(.15f+.12f*std::sin(t*5+float(vine))),-length*t,.09f+.035f*std::sin(t*4));
            taper(b,previous,p,.018f,.014f,Stem,Block::Leaves);
            for (const float wing:{-1.0f,1.0f})
                leaf(b,p,p+glm::vec3(wing*.25f,-.14f,.045f),.095f,i%2?Jade:Lime,{0,0,1});
            if ((i+vine)%3==0) flower(b,p+glm::vec3(side*.12f,-.04f,.13f),.18f,Pink,float(i),true);
            previous=p;
        }
    }
}

// Tiered umbrella caps include a contrasting rim and radial gills underneath.
void mushroom(AssetBuilder& b, glm::vec3 foot, float height, float radius,
              glm::vec3 color, std::uint32_t seed, bool luminous)
{
    const auto lean=radial(noise(seed,1)*Pi*2)*height*.075f;
    const auto cap=foot+lean+glm::vec3(0,height,0);
    const auto elbow=foot+lean*.35f+glm::vec3(0,height*.48f,0);
    taper(b,foot+glm::vec3(0,radius*.10f,0),elbow,radius*.18f,radius*.13f,Cream,Block::Leaves);
    taper(b,elbow,cap,radius*.13f,radius*.11f,Cream,Block::Leaves);
    const float turn=noise(seed,2)*2*Pi;
    std::array<std::array<glm::vec3,10>,4> ring{};
    constexpr float widths[]{.91f,1.0f,.72f,.28f};
    constexpr float heights[]{-.035f,.02f,.26f,.39f};
    for (int tier=0;tier<4;++tier) for (int side=0;side<10;++side) {
        const auto dir=radial(turn+float(side)*Pi/5);
        ring[tier][side]=cap+dir*radius*widths[tier]*(.93f+.07f*noise(seed,side+11))
            +glm::vec3(0,radius*heights[tier],0);
    }
    for (int side=0;side<10;++side) {
        const int next=(side+1)%10;
        const float tint=.84f+.17f*noise(seed,side+30);
        for (int tier=0;tier<3;++tier)
            b.quad(ring[tier][side],ring[tier+1][side],ring[tier+1][next],ring[tier][next],
                   Block::Leaves,(tier==0?color*1.33f:color)*tint);
        b.triangle(cap+glm::vec3(0,radius*.40f,0),ring[3][next],ring[3][side],Block::Leaves,color*1.12f);
        b.triangle(cap-glm::vec3(0,radius*.12f,0),ring[0][side],ring[0][next],
                   luminous?Block::GlowLeaves:Block::Leaves,
                   luminous?glm::vec3(.22f,.78f,.68f):Cream*.82f);
        const auto edge=glm::mix(ring[0][side],ring[0][next],.50f);
        taper(b,cap-glm::vec3(0,radius*.125f,0),edge-glm::vec3(0,.009f,0),
              radius*.009f,radius*.014f,luminous?glm::vec3(.16f,.53f,.50f):Cream*.65f,Block::Leaves);
        if (side%3==0) {
            const auto patch=glm::mix(ring[2][side],ring[3][side],.47f)+glm::vec3(0,.008f,0);
            leaf(b,patch,patch+radial(turn+float(side)*Pi/5)*radius*.12f,.045f*radius,
                 color*1.38f);
        }
    }
}

void glowColony(AssetBuilder& b, std::uint32_t seed)
{
    for (int i=0;i<6;++i) {
        const auto dir=radial(float(i)*2.4f+noise(seed,1)*2*Pi);
        const auto foot=dir*(i==0?.08f:.43f);
        const float height=i==0?.83f:.32f+.30f*noise(seed,i+2);
        mushroom(b,foot,height,i==0?.38f:.17f+.10f*noise(seed,i+10),i%2?Teal:Jade*.72f,seed+i,true);
    }
    rosette(b,{.17f,.025f,-.29f},.24f,.18f,Lime,seed+55,5);
}

void giantMushroom(AssetBuilder& b, std::uint32_t seed)
{
    const auto color=seed%3==0?glm::vec3(.44f,.25f,.58f):glm::vec3(.15f,.49f,.47f);
    mushroom(b,{-.06f,0,.01f},2.65f,1.36f,color,seed,true);
    // The small companion and basal skirt make the footing feel rooted.
    mushroom(b,{-.72f,0,.64f},.59f,.34f,color*.91f,seed+10,false);
    rosette(b,{.12f,.025f,-.08f},.49f,.26f,Teal*.65f,seed+12,8);
}

void shelf(AssetBuilder& b, glm::vec3 origin, float radius, glm::vec3 color, std::uint32_t seed)
{
    constexpr int Count=9;
    std::array<glm::vec3,Count> rim{},inner{},bottom{};
    for (int i=0;i<Count;++i) {
        const float angle=Pi*float(i)/float(Count-1);
        const auto dir=glm::vec3(std::cos(angle),0,std::sin(angle));
        rim[i]=origin+dir*radius*(.94f+.06f*noise(seed,i+1));
        inner[i]=origin+dir*radius*.60f+glm::vec3(0,radius*.19f,0);
        bottom[i]=origin+dir*radius*.78f-glm::vec3(0,radius*.13f,0);
    }
    const auto crown=origin+glm::vec3(0,radius*.20f,.06f);
    const auto under=origin-glm::vec3(0,radius*.10f,0);
    for (int i=0;i<Count-1;++i) {
        b.triangle(crown,inner[i+1],inner[i],Block::Leaves,color*.90f);
        b.quad(inner[i],inner[i+1],rim[i+1],rim[i],Block::Leaves,color*1.08f);
        b.quad(rim[i],rim[i+1],bottom[i+1],bottom[i],Block::Leaves,Cream);
        b.triangle(under,bottom[i],bottom[i+1],Block::Leaves,Cream*.73f);
    }
}

void shelfFungi(AssetBuilder& b, std::uint32_t seed)
{
    shelf(b,{-.34f,.52f,.025f},.55f,{.66f,.34f,.22f},seed);
    shelf(b,{.28f,.10f,.025f},.69f,{.48f,.31f,.44f},seed+10);
    shelf(b,{-.22f,-.43f,.025f},.57f,{.75f,.46f,.24f},seed+20);
    shelf(b,{.37f,-.78f,.025f},.35f,{.56f,.34f,.46f},seed+30);
}

void pad(AssetBuilder& b, glm::vec3 center, float radius, float turn)
{
    // One sector is open, making the recognisable water-lily notch.
    for (int i=1;i<10;++i) {
        const auto a=center+radial(turn+float(i)*Pi/5)*radius;
        const auto z=center+radial(turn+float(i+1)*Pi/5)*radius;
        b.triangle(center+glm::vec3(0,.018f,0),z,a,Block::Leaves,Jade*(.78f+.035f*float(i)),true);
        taper(b,center+glm::vec3(0,.020f,0),glm::mix(a,z,.48f)+glm::vec3(0,.005f,0),
              .005f,.002f,Lime*.65f,Block::Leaves);
    }
}

void lotus(AssetBuilder& b, std::uint32_t seed)
{
    const float turn=noise(seed,1)*Pi*2;
    pad(b,{-.24f,.034f,-.19f},.55f,turn);
    pad(b,{.34f,.021f,.28f},.39f,turn+1.4f);
    taper(b,{-.17f,-.18f,-.11f},{-.17f,.24f,-.11f},.018f,.014f,Stem,Block::Leaves);
    flower(b,{-.17f,.25f,-.11f},.32f,Pink,turn);
    flower(b,{.34f,.075f,.28f},.16f,{.82f,.73f,.96f},turn+.8f);
    for (int i=0;i<5;++i) {
        const auto dir=radial(turn+float(i)*Pi*2/5);
        leaf(b,{-.17f,.26f,-.11f},glm::vec3(-.17f,.51f,-.11f)+dir*.10f,.07f,Pink*1.14f);
    }
}

void waterGrass(AssetBuilder& b, std::uint32_t seed)
{
    const float turn=noise(seed,1)*Pi*2;
    for (int i=0;i<10;++i) {
        const auto dir=radial(turn+float(i)*2.4f);
        const auto foot=dir*(.08f+.10f*noise(seed,i+10))-glm::vec3(0,.48f,0);
        const float height=1.21f+.69f*noise(seed,i+20);
        const auto joint=foot+dir*.17f+glm::vec3(0,height*.63f,0);
        const auto tip=foot+dir*.34f+glm::vec3(0,height,0);
        leaf(b,foot,joint,.055f,i%3==0?Lime:Teal);
        leaf(b,joint-glm::vec3(0,.11f,0),tip,.037f,i%3==0?Lime:Teal);
    }
}

void mossyLog(AssetBuilder& b, std::uint32_t seed)
{
    // Open-ended hollow log, with separate bark, pale growth ring and dark bore.
    std::array<glm::vec3,10> a{},z{},innerA{},innerZ{};
    for (int i=0;i<10;++i) {
        const float angle=float(i)*Pi/5;
        const auto radialYZ=glm::vec3(0,std::cos(angle),std::sin(angle));
        a[i]=glm::vec3(-1.44f,.38f,0)+radialYZ*.36f;
        z[i]=glm::vec3(1.41f,.38f,0)+radialYZ*(.33f+.015f*noise(seed,i));
        innerA[i]=glm::vec3(-1.44f,.38f,0)+radialYZ*.235f;
        innerZ[i]=glm::vec3(1.41f,.38f,0)+radialYZ*.22f;
    }
    for (int i=0;i<10;++i) {
        const int next=(i+1)%10;
        b.quad(a[i],a[next],z[next],z[i],Block::RootWood,Bark*(.79f+.07f*float(i%4)));
        b.quad(innerA[next],innerA[i],innerZ[i],innerZ[next],Block::RootWood,Bark*.40f);
        b.quad(a[next],a[i],innerA[i],innerA[next],Block::RootWood,Cream*.64f);
        b.quad(z[i],z[next],innerZ[next],innerZ[i],Block::RootWood,Cream*.64f);
        if (i==0 || i==1 || i==9) {
            const auto lift=glm::vec3(0,.008f,0);
            b.quad(glm::mix(a[i],z[i],.14f)+lift,glm::mix(a[next],z[next],.14f)+lift,
                   glm::mix(a[next],z[next],.81f)+lift,glm::mix(a[i],z[i],.81f)+lift,
                   Block::Leaves,Jade*(.68f+.07f*float(i%3)));
        }
    }
    taper(b,{.21f,.53f,.22f},{.42f,.82f,.40f},.11f,.075f,Bark);
    rosette(b,{-.55f,.735f,0},.29f,.29f,Lime,seed+11,5);
    mushroom(b,{.82f,.66f,-.12f},.22f,.18f,{.66f,.38f,.32f},seed+12,false);
}

void rootFan(AssetBuilder& b, std::uint32_t seed)
{
    const glm::vec3 center(0,-1.19f,.08f);
    for (int root=0;root<7;++root) {
        const float side=(float(root)-3)/3;
        const auto elbow=glm::vec3(side*.57f,-.20f,.14f+.025f*noise(seed,root));
        const auto tip=glm::vec3(side*1.19f,.78f+.39f*noise(seed,root+10),.06f);
        taper(b,center,elbow,.065f,.044f,Bark*.88f);
        taper(b,elbow,tip,.044f,.011f,Bark);
        const auto twig=glm::mix(elbow,tip,.52f);
        const auto twigEnd=twig+glm::vec3(side<0?-.18f:.18f,.10f,.04f);
        taper(b,twig,twigEnd,.018f,.006f,Bark);
        leaf(b,twigEnd,twigEnd+glm::vec3(.05f,.20f,.03f),.075f,Jade,{0,0,1});
        if (root%2==0) leaf(b,elbow,elbow+glm::vec3(side*.27f,.29f,.06f),.14f,Lime*.78f,{0,0,1});
    }
}

void stone(AssetBuilder& b, glm::vec3 foot, glm::vec3 size, std::uint32_t seed)
{
    std::array<glm::vec3,7> lower{},upper{};
    for (int i=0;i<7;++i) {
        const auto dir=radial(float(i)*Pi*2/7);
        lower[i]=foot+dir*size*(.86f+.14f*noise(seed,i));
        upper[i]=foot+dir*size*.72f+glm::vec3(.04f,size.y*(.77f+.15f*noise(seed,i+10)),0);
    }
    const auto crown=foot+glm::vec3(.04f,size.y*.91f,-.02f);
    for (int i=0;i<7;++i) {
        const int next=(i+1)%7;
        const auto color=glm::vec3(.36f,.48f,.43f)*(.83f+.22f*noise(seed,i+20));
        b.quad(lower[i],upper[i],upper[next],lower[next],Block::MossStone,color);
        b.triangle(crown,upper[next],upper[i],Block::MossStone,color*1.13f);
        b.triangle(foot,lower[i],lower[next],Block::MossStone,color*.67f);
        if ((i+seed)%3!=0) {
            const auto offset=glm::vec3(0,.005f,0);
            b.triangle(crown+offset,glm::mix(crown,upper[next],.86f)+offset,
                       glm::mix(crown,upper[i],.78f)+offset,Block::Leaves,Jade*.76f);
        }
    }
}

void mossyStones(AssetBuilder& b, std::uint32_t seed)
{
    stone(b,{-.28f,0,-.10f},{.64f,.71f,.57f},seed);
    stone(b,{.57f,0,.22f},{.46f,.44f,.43f},seed+1);
    stone(b,{-.45f,0,.50f},{.37f,.27f,.31f},seed+2);
    rosette(b,{.26f,.02f,-.48f},.27f,.30f,Lime,seed+20,5);
    flower(b,{-.27f,.68f,-.05f},.12f,{.82f,.77f,.48f},float(seed));
}

void crystal(AssetBuilder& b, glm::vec3 foot, glm::vec3 tip, float radius,
             glm::vec3 color, Block material)
{
    const auto along=glm::normalize(tip-foot);
    const auto ref=std::abs(along.y)>.9f?glm::vec3(1,0,0):glm::vec3(0,1,0);
    const auto across=glm::normalize(glm::cross(ref,along));
    const auto up=glm::cross(along,across);
    std::array<glm::vec3,6> low{},shoulder{};
    for (int i=0;i<6;++i) {
        const auto dir=across*std::cos(float(i)*Pi/3)+up*std::sin(float(i)*Pi/3);
        low[i]=foot+dir*radius*.69f;
        shoulder[i]=glm::mix(foot,tip,.69f)+dir*radius;
    }
    for (int i=0;i<6;++i) {
        const int next=(i+1)%6;
        const auto face=color*(.64f+.07f*float(i));
        b.quad(low[i],low[next],shoulder[next],shoulder[i],material,face);
        b.triangle(tip,shoulder[i],shoulder[next],material,glm::min(face*1.25f,glm::vec3(1)));
        b.triangle(foot,low[next],low[i],material,face*.70f);
    }
}

void crystalFlower(AssetBuilder& b, std::uint32_t seed)
{
    const float turn=noise(seed,1)*2*Pi;
    rosette(b,{0,.035f,0},.68f,.25f,Teal,seed,8);
    taper(b,{0,.06f,0},{.04f,.64f,.02f},.04f,.022f,Stem,Block::Leaves);
    crystal(b,{.04f,.49f,.02f},{.04f,1.29f,.02f},.17f,{.96f,.59f,.16f},Block::AmberCrystal);
    for (int i=0;i<5;++i) {
        const auto dir=radial(turn+float(i)*2*Pi/5);
        crystal(b,{.04f,.56f,.02f},glm::vec3(.04f,.89f,.02f)+dir*.36f,.105f,{.92f,.48f,.16f},Block::AmberCrystal);
        leaf(b,{.04f,.49f,.02f},glm::vec3(.04f,.81f,.02f)+dir*.39f,.12f,Jade);
    }
}

void sporeStalks(AssetBuilder& b, std::uint32_t seed)
{
    for (int i=0;i<4;++i) {
        const auto dir=radial(float(i)*2.4f+noise(seed,1)*2*Pi);
        const auto foot=dir*(i==0?.06f:.32f);
        const float height=i==0?2.03f:1.10f+.50f*noise(seed,i+10);
        const auto middle=foot+dir*.10f+glm::vec3(0,height*.55f,0);
        const auto bud=foot+dir*.17f+glm::vec3(0,height,0);
        taper(b,foot+glm::vec3(0,.02f,0),middle,.046f,.031f,Teal*.66f,Block::Leaves);
        taper(b,middle,bud,.031f,.020f,Teal*.77f,Block::Leaves);
        crystal(b,bud-glm::vec3(0,.09f,0),bud+glm::vec3(0,.28f,0),.115f,{.28f,.83f,.70f},Block::GlowLeaves);
        for (int wing=0;wing<3;++wing) {
            const auto fan=radial(float(wing)*Pi*2/3+float(i));
            leaf(b,bud-glm::vec3(0,.12f,0),bud+fan*.16f+glm::vec3(0,.08f,0),.07f,Teal);
        }
        leaf(b,middle,middle-dir*.24f+glm::vec3(0,.32f,0),.10f,Jade);
    }
    rosette(b,{0,.02f,0},.47f,.28f,Teal,seed+42,6);
}

void orchid(AssetBuilder& b, std::uint32_t seed)
{
    rosette(b,{0,.02f,0},.62f,.24f,Teal,seed,6);
    const float turn=noise(seed,1)*Pi*2;
    for (int i=0;i<3;++i) {
        const auto dir=radial(turn+float(i)*Pi*2/3);
        const auto middle=dir*.09f+glm::vec3(0,.48f,0);
        const auto tip=dir*.27f+glm::vec3(0,.72f+float(i)*.09f,0);
        taper(b,{0,.035f,0},middle,.017f,.014f,Stem,Block::Leaves);
        taper(b,middle,tip,.014f,.011f,Stem,Block::Leaves);
        flower(b,tip,.20f,i%2?Pink:glm::vec3(.69f,.49f,.91f),float(i),true);
        // A long lower lip distinguishes orchid flowers from ground blossoms.
        leaf(b,tip+glm::vec3(0,0,.035f),tip+glm::vec3(.04f,-.22f,.12f),.084f,
             {.96f,.75f,.50f},{0,0,1});
    }
}

void hangingPlanter(AssetBuilder& b, std::uint32_t seed)
{
    const float turn=noise(seed,1)*Pi*2;
    // Handwoven seed-pod bowl hangs from three visible straps, all under anchor.
    b.box({-.045f,-.10f,-.045f},{.045f,0,.045f},Block::RootWood,Bark);
    for (int i=0;i<3;++i) {
        const auto edge=radial(turn+float(i)*2*Pi/3)*.34f;
        taper(b,{0,-.08f,0},edge+glm::vec3(0,-1.04f,0),.015f,.018f,Cream*.60f,Block::RootWood);
    }
    std::array<glm::vec3,8> rim{},base{};
    for (int i=0;i<8;++i) {
        const auto dir=radial(turn+float(i)*Pi/4);
        rim[i]=dir*.40f+glm::vec3(0,-1.05f,0);
        base[i]=dir*.22f+glm::vec3(0,-1.40f,0);
    }
    for (int i=0;i<8;++i) {
        const int next=(i+1)%8;
        b.quad(rim[i],rim[next],base[next],base[i],Block::Clay,{.60f,.33f,.19f});
        b.triangle({0,-1.41f,0},base[i],base[next],Block::Clay,{.38f,.20f,.12f});
        b.triangle({0,-1.075f,0},rim[next]-glm::vec3(0,.012f,0),rim[i]-glm::vec3(0,.012f,0),Block::Dirt,{.19f,.16f,.09f});
        taper(b,rim[i],rim[next],.028f,.028f,Cream*.64f,Block::RootWood);
    }
    rosette(b,{0,-1.04f,0},.57f,.37f,Jade,seed+3,7);
    for (int vine=0;vine<3;++vine) {
        const auto dir=radial(turn+float(vine)*2*Pi/3);
        auto prev=dir*.31f+glm::vec3(0,-1.05f,0);
        for (int node=1;node<=4;++node) {
            const auto next=dir*(.39f+.035f*std::sin(float(node)))+glm::vec3(0,-1.05f-float(node)*.31f,0);
            taper(b,prev,next,.014f,.012f,Stem,Block::Leaves);
            const auto side=glm::vec3(-dir.z,0,dir.x)*(node%2?.16f:-.16f);
            leaf(b,next,next+side+dir*.04f-glm::vec3(0,.14f,0),.085f,Lime,{dir.x,0,dir.z});
            if (node==2) flower(b,next+dir*.08f,.13f,Pink,turn);
            prev=next;
        }
    }
}

} // namespace

void appendHubLush(MeshData& mesh, const HubAsset& asset)
{
    AssetBuilder b(mesh,asset);
    switch (asset.kind) {
    case HubAssetKind::TreeFern: treeFern(b,asset.variation); break;
    case HubAssetKind::LushSapling: sapling(b,asset.variation); break;
    case HubAssetKind::HangingRoots: hangingRoots(b,asset.variation); break;
    case HubAssetKind::BlossomVines: blossomVines(b,asset.variation); break;
    case HubAssetKind::GlowMushroomColony: glowColony(b,asset.variation); break;
    case HubAssetKind::GiantMushroom: giantMushroom(b,asset.variation); break;
    case HubAssetKind::ShelfFungi: shelfFungi(b,asset.variation); break;
    case HubAssetKind::CaveLotus: lotus(b,asset.variation); break;
    case HubAssetKind::WaterGrass: waterGrass(b,asset.variation); break;
    case HubAssetKind::MossyLog: mossyLog(b,asset.variation); break;
    case HubAssetKind::RootFan: rootFan(b,asset.variation); break;
    case HubAssetKind::MossyStones: mossyStones(b,asset.variation); break;
    case HubAssetKind::CrystalFlower: crystalFlower(b,asset.variation); break;
    case HubAssetKind::SporeStalks: sporeStalks(b,asset.variation); break;
    case HubAssetKind::CaveOrchid: orchid(b,asset.variation); break;
    case HubAssetKind::HangingPlanter: hangingPlanter(b,asset.variation); break;
    default: break;
    }
}

} // namespace voxel
