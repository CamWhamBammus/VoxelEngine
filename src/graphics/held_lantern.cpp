#include "graphics/held_lantern.h"
#include "graphics/asset_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace voxel {
namespace {

constexpr glm::vec3 Iron{.16f,.20f,.205f};
constexpr glm::vec3 Brass{.63f,.39f,.14f};
constexpr glm::vec3 Leather{.23f,.105f,.055f};
constexpr glm::vec3 Binding{.36f,.19f,.09f};
constexpr glm::vec3 Amber{.85f,.44f,.12f};
constexpr glm::vec3 GlowCentre{0,-.445f,0};
using Ring = std::array<glm::vec3,8>;

Ring ring(float y, glm::vec2 lo, glm::vec2 hi, float cut)
{
    return {{{lo.x+cut,y,lo.y}, {hi.x-cut,y,lo.y},
             {hi.x,y,lo.y+cut}, {hi.x,y,hi.y-cut},
             {hi.x-cut,y,hi.y}, {lo.x+cut,y,hi.y},
             {lo.x,y,hi.y-cut}, {lo.x,y,lo.y+cut}}};
}

// Broad bevels give the rim and cap a readable edge in the warm cave light.
// Both rings are wound alike; caps close the solid, including its underside.
void solid(AssetBuilder& b, const Ring& bottom, const Ring& top,
           Block material, glm::vec3 color)
{
    glm::vec3 lower{},upper{};
    for (std::size_t i=0; i<bottom.size(); ++i) {
        lower+=bottom[i]/8.0f;
        upper+=top[i]/8.0f;
    }
    for (std::size_t i=0; i<bottom.size(); ++i) {
        const auto next=(i+1)%bottom.size();
        b.quad(bottom[i],top[i],top[next],bottom[next],material,
               color*((i&1) ? .88f : 1.0f));
        b.triangle(lower,bottom[i],bottom[next],material,color*.78f);
        b.triangle(upper,top[next],top[i],material,color*1.07f);
    }
}

void bevelBox(AssetBuilder& b, glm::vec3 lo, glm::vec3 hi, float cut,
              Block material, glm::vec3 color)
{
    solid(b,ring(lo.y,{lo.x,lo.z},{hi.x,hi.z},cut),
          ring(hi.y,{lo.x,lo.z},{hi.x,hi.z},cut),material,color);
}

Ring sleeveRing(float x, float y, float z, float halfHeight, float halfDepth)
{
    auto points=ring(x,{-halfHeight,-halfDepth},{halfHeight,halfDepth},.014f);
    for (auto& p : points) p={p.y,y-p.x,z+p.z};
    return points;
}

} // namespace

MeshData buildHeldLantern()
{
    MeshData mesh;
    const HubAsset origin{};
    AssetBuilder b(mesh,origin);

    // Recessed amber glass is a real volume. The cage lies outside it, so the
    // side panes become visible naturally as the lantern turns in the hand.
    bevelBox(b,{-.140f,-.588f,-.128f},{.140f,-.288f,.128f},.022f,
             Block::Lantern,Amber);
    bevelBox(b,{-.183f,-.636f,-.169f},{.183f,-.602f,.169f},.033f,
             Block::IronOre,Iron*.79f);
    bevelBox(b,{-.172f,-.607f,-.157f},{.172f,-.579f,.157f},.028f,
             Block::CopperOre,Brass);
    bevelBox(b,{-.171f,-.301f,-.157f},{.171f,-.272f,.157f},.028f,
             Block::CopperOre,Brass*.91f);

    // Four chamfered corner pillars and slimmer mid-height bands protect the
    // panes. Their dark silhouettes keep the light from reading as a flat icon.
    for (float x : {-.153f,.153f}) for (float z : {-.141f,.141f}) {
        bevelBox(b,{x-.020f,-.591f,z-.020f},{x+.020f,-.285f,z+.020f},.007f,
                 Block::IronOre,Iron);
        for (float y : {-.565f,-.320f})
            bevelBox(b,{x-.023f,y,z-.023f},{x+.023f,y+.020f,z+.023f},.008f,
                     Block::CopperOre,Brass*.86f);
    }
    for (float z : {-.143f,.143f}) {
        b.box({-.155f,-.460f,z-.009f},{.155f,-.444f,z+.009f},Block::IronOre,Iron);
        b.box({-.009f,-.584f,z-.009f},{.009f,-.294f,z+.009f},Block::IronOre,Iron);
    }
    for (float x : {-.155f,.155f}) {
        b.box({x-.009f,-.460f,-.140f},{x+.009f,-.444f,.140f},Block::IronOre,Iron);
        b.box({x-.009f,-.584f,-.009f},{x+.009f,-.294f,.009f},Block::IronOre,Iron);
    }

    // A low pitched cap sheds water; the raised vent and seams are geometry,
    // not painted highlights. The crown fits inside the open carry bail.
    solid(b,ring(-.273f,{-.198f,-.182f},{.198f,.182f},.043f),
          ring(-.197f,{-.106f,-.096f},{.106f,.096f},.022f),
          Block::IronOre,Iron*1.25f);
    bevelBox(b,{-.201f,-.281f,-.185f},{.201f,-.264f,.185f},.043f,
             Block::CopperOre,Brass*.81f);
    bevelBox(b,{-.071f,-.201f,-.065f},{.071f,-.149f,.065f},.016f,
             Block::IronOre,Iron*.66f);
    for (float z : {-.069f,.069f}) for (float x : {-.050f,0.0f,.050f})
        b.box({x-.006f,-.198f,z-.005f},{x+.006f,-.147f,z+.005f},Block::CopperOre,Brass*.73f);
    bevelBox(b,{-.090f,-.151f,-.083f},{.090f,-.133f,.083f},.022f,
             Block::IronOre,Iron*1.32f);
    bevelBox(b,{-.039f,-.135f,-.033f},{.039f,-.117f,.033f},.012f,
             Block::CopperOre,Brass);

    // Hinges sit on the sides of the cap. A faceted bail rises around the vent
    // to a horizontal leather grip, leaving open air between hand and lantern.
    for (float side : {-1.0f,1.0f}) {
        const float x=side*.183f;
        bevelBox(b,{x-.026f,-.260f,-.029f},{x+.026f,-.207f,.029f},.010f,
                 Block::CopperOre,Brass);
        b.beam({side*.196f,-.235f,0},{side*.196f,-.063f,0},.025f,Block::IronOre,Iron*1.26f);
        b.beam({side*.196f,-.063f,0},{side*.115f,.014f,0},.025f,Block::IronOre,Iron*1.26f);
        b.beam({side*.115f,.014f,0},{0,.014f,0},.025f,Block::IronOre,Iron*1.26f);
    }
    bevelBox(b,{-.109f,-.007f,-.023f},{.109f,.035f,.023f},.009f,
             Block::Timber,Leather);
    for (int i=0; i<7; ++i) {
        const float x=-.093f+i*.029f;
        b.box({x,-.009f,-.025f},{x+.008f,.037f,.025f},Block::Timber,Binding);
    }

    // The right hand closes over the horizontal grip. Four curled fingers end
    // below it; the thumb crosses the back and a short cuff exits to the right.
    bevelBox(b,{-.076f,.018f,-.065f},{.102f,.094f,.041f},.020f,Block::Timber,Leather);
    for (int finger=0; finger<4; ++finger) {
        const float x=-.076f+finger*.043f;
        bevelBox(b,{x,-.021f,.019f},{x+.036f,.065f,.067f},.011f,
                 Block::Timber,Binding*(1.0f-finger*.025f));
        bevelBox(b,{x,-.032f,-.008f},{x+.036f,.003f,.057f},.009f,
                 Block::Timber,Binding*.85f);
    }
    b.beam({.099f,.059f,-.034f},{.070f,.008f,-.061f},.047f,Block::Timber,Binding);
    b.beam({.070f,.008f,-.061f},{.025f,-.006f,-.033f},.043f,Block::Timber,Binding);
    bevelBox(b,{.090f,.021f,-.071f},{.206f,.101f,.051f},.020f,Block::Timber,Leather*.77f);
    bevelBox(b,{.122f,.104f,-.033f},{.174f,.114f,.011f},.009f,Block::CopperOre,Brass*.76f);

    // A narrow sleeve continues from beneath the cuff out of the right side
    // of the view. It widens toward the player rather than ending as a floating
    // hand, while the hanging cage remains completely visible beneath it.
    const glm::vec3 cloth{.10f,.16f,.16f};
    const auto wrist=sleeveRing(.185f,.061f,-.010f,.037f,.054f);
    const auto forearm=sleeveRing(.65f,-.045f,.012f,.065f,.078f);
    const auto offscreen=sleeveRing(1.35f,-.26f,.030f,.095f,.105f);
    solid(b,wrist,forearm,Block::Slate,cloth);
    solid(b,forearm,offscreen,Block::Slate,cloth*.92f);
    return mesh;
}

glm::mat4 heldLanternPose(float timeSeconds, float aspectRatio)
{
    if (!std::isfinite(timeSeconds) || timeSeconds<0) timeSeconds=0;
    if (!std::isfinite(aspectRatio) || aspectRatio<=0) aspectRatio=16.0f/9.0f;
    const float aspect=std::clamp(aspectRatio,.6f,3.6f);
    // Periodic phase keeps long-running sessions precise. Rotation is centred
    // on the grip, so the hanging cage moves a little farther than the hand.
    const float phase=std::fmod(timeSeconds,8.0f)*glm::radians(45.0f);
    const glm::vec3 position{.36f*aspect,-.035f+std::sin(phase*2)*.004f,-1.25f};
    auto model=glm::translate(glm::mat4(1),position);
    model=glm::rotate(model,glm::radians(-4.0f+std::sin(phase)*2.2f),{0,0,1});
    model=glm::rotate(model,glm::radians(-24.0f+std::sin(phase)*1.3f),{0,1,0});
    model=glm::rotate(model,glm::radians(-4.0f+std::sin(phase*2)*1.0f),{1,0,0});
    const float scale=.90f*std::min(1.0f,aspect/(4.0f/3.0f));
    return glm::scale(model,glm::vec3(scale));
}

glm::vec3 heldLanternLightPosition(float timeSeconds, float aspectRatio)
{
    return glm::vec3(heldLanternPose(timeSeconds,aspectRatio)*glm::vec4(GlowCentre,1));
}

} // namespace voxel
