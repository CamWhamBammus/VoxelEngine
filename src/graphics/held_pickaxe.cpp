#include "graphics/held_pickaxe.h"
#include "graphics/asset_builder.h"
#include "equipment.h"

#include <array>

namespace voxel {
namespace {

constexpr glm::vec3 Brass{.67f,.43f,.19f};
constexpr glm::vec3 Wood{.42f,.24f,.12f};
constexpr glm::vec3 Leather{.23f,.105f,.055f};
constexpr glm::vec3 Binding{.36f,.19f,.09f};

// An eight-sided solid gives the handle, metal socket and glove broad faces
// that catch the same cave lights as the pick's sharpened edge.
void bevelBox(AssetBuilder& b, glm::vec3 lo, glm::vec3 hi, float bevel,
              Block material, glm::vec3 color)
{
    const std::array<glm::vec2,8> ring{{
        {lo.x+bevel,lo.z}, {hi.x-bevel,lo.z},
        {hi.x,lo.z+bevel}, {hi.x,hi.z-bevel},
        {hi.x-bevel,hi.z}, {lo.x+bevel,hi.z},
        {lo.x,hi.z-bevel}, {lo.x,lo.z+bevel}
    }};
    const glm::vec3 bottom{(lo.x+hi.x)*.5f,lo.y,(lo.z+hi.z)*.5f};
    const glm::vec3 top{bottom.x,hi.y,bottom.z};
    for (std::size_t i=0; i<ring.size(); ++i) {
        const auto a=ring[i], c=ring[(i+1)%ring.size()];
        const glm::vec3 al{a.x,lo.y,a.y}, ah{a.x,hi.y,a.y};
        const glm::vec3 cl{c.x,lo.y,c.y}, ch{c.x,hi.y,c.y};
        b.quad(al,ah,ch,cl,material,color*((i&1) ? .90f : 1.0f));
        b.triangle(bottom,al,cl,material,color*.82f);
        b.triangle(top,ch,ah,material,color);
    }
}

using HeadRing = std::array<glm::vec3,8>;
HeadRing headRing(float x, float y, float halfHeight, float halfDepth)
{
    return {{{x,y+halfHeight,0}, {x,y+halfHeight*.72f,halfDepth*.72f},
             {x,y,halfDepth}, {x,y-halfHeight*.72f,halfDepth*.72f},
             {x,y-halfHeight,0}, {x,y-halfHeight*.72f,-halfDepth*.72f},
             {x,y,-halfDepth}, {x,y+halfHeight*.72f,-halfDepth*.72f}}};
}

} // namespace

MeshData buildHeldPickaxe(int tier)
{
    const auto Steel = glm::mix(gearMaterial(tier).color, glm::vec3(1), .12f);
    MeshData mesh;
    const HubAsset origin{};
    AssetBuilder b(mesh,origin);

    // Separate cross sections curve down into two solid sharp points. Broad
    // material-colored side faces and lighter cutting bevels remain legible in shade.
    const std::array<HeadRing,6> head{{
        headRing(-.36f,.53f,.029f,.022f),
        headRing(-.22f,.64f,.052f,.046f),
        headRing(-.065f,.68f,.068f,.064f),
        headRing(.075f,.68f,.068f,.064f),
        headRing(.25f,.62f,.048f,.043f),
        headRing(.37f,.51f,.024f,.019f)
    }};
    for (std::size_t section=0; section+1<head.size(); ++section) {
        for (std::size_t i=0; i<head[section].size(); ++i) {
            const auto next=(i+1)%head[section].size();
            const bool bevel=i==0 || i==3 || i==4 || i==7;
            b.quad(head[section][i],head[section][next],
                   head[section+1][next],head[section+1][i],
                   Block::IronOre,Steel*(bevel ? 1.50f : .94f));
        }
    }
    for (std::size_t i=0; i<head.front().size(); ++i) {
        const auto next=(i+1)%head.front().size();
        b.triangle({-.465f,.425f,0},head.front()[next],head.front()[i],
                   Block::IronOre,Steel*((i&1) ? 1.43f : 1.13f));
        b.triangle({.445f,.405f,0},head.back()[i],head.back()[next],
                   Block::IronOre,Steel*((i&1) ? 1.43f : 1.13f));
    }

    // The hickory shaft extends through the brass eye; its end and metal wedge
    // are visible above the head. Narrow ferrules reinforce the upper handle.
    bevelBox(b,{-.035f,-.215f,-.034f},{.035f,.777f,.034f},.010f,Block::Timber,Wood);
    bevelBox(b,{-.014f,.776f,-.027f},{.014f,.788f,.027f},.005f,Block::IronOre,Steel);
    bevelBox(b,{-.076f,.590f,-.074f},{.076f,.755f,.074f},.019f,Block::CopperOre,Brass);
    bevelBox(b,{-.050f,.550f,-.048f},{.050f,.592f,.048f},.012f,Block::IronOre,Steel*.84f);
    for (float z : {-.082f,.082f}) {
        bevelBox(b,{-.025f,.646f,z-.008f},{.025f,.696f,z+.008f},.006f,
                 Block::IronOre,Steel*1.56f);
    }
    for (float y : {.22f,.43f})
        bevelBox(b,{-.039f,y,-.038f},{.039f,y+.025f,.038f},.010f,Block::CopperOre,Brass*.81f);

    bevelBox(b,{-.038f,-.193f,-.037f},{.038f,.149f,.037f},.011f,Block::Timber,Leather);
    for (int i=0; i<11; ++i) {
        const float y=-.180f+i*.029f;
        b.beam({-.038f,y,.038f},{.038f,y+.019f,.038f},.010f,Block::Timber,Binding);
        b.beam({.038f,y+.019f,.038f},{.038f,y+.023f,-.038f},.010f,Block::Timber,Binding);
        b.beam({.038f,y+.023f,-.038f},{-.038f,y+.004f,-.038f},.010f,Block::Timber,Binding);
        b.beam({-.038f,y+.004f,-.038f},{-.038f,y,.038f},.010f,Block::Timber,Binding);
    }
    bevelBox(b,{-.048f,-.229f,-.046f},{.048f,-.190f,.046f},.014f,Block::CopperOre,Brass*.87f);

    // A compact right glove shares the equipment palette; the separated curled
    // fingers and thumb wrap around the shaft instead of floating behind it.
    bevelBox(b,{.022f,-.118f,-.061f},{.119f,.061f,.064f},.022f,Block::Timber,Leather);
    for (int finger=0; finger<4; ++finger) {
        const float y=-.112f+finger*.040f;
        bevelBox(b,{-.056f,y,.022f},{.085f,y+.033f,.084f},.012f,
                 Block::Timber,Binding*(1.0f-finger*.025f));
    }
    b.beam({.104f,.030f,-.031f},{.024f,-.020f,-.065f},.052f,Block::Timber,Binding);
    b.beam({.024f,-.020f,-.065f},{-.022f,-.043f,-.019f},.046f,Block::Timber,Binding);
    bevelBox(b,{.069f,-.180f,-.074f},{.184f,-.105f,.079f},.019f,Block::Timber,Leather*.77f);
    bevelBox(b,{.079f,-.166f,.080f},{.161f,-.136f,.091f},.009f,Block::CopperOre,Brass*.76f);
    return mesh;
}

} // namespace voxel
