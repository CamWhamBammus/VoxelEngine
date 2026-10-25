#include "graphics/held_compass.h"
#include "graphics/asset_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace voxel {
namespace {

constexpr glm::vec3 Iron{.13f,.185f,.195f};
constexpr glm::vec3 Brass{.67f,.43f,.18f};
constexpr glm::vec3 Dial{.70f,.72f,.57f};
constexpr glm::vec3 Ink{.22f,.32f,.30f};
constexpr glm::vec3 Red{.79f,.105f,.075f};
constexpr int Sides=12;
using Ring=std::array<glm::vec3,Sides>;

Ring ring(float radius, float z)
{
    Ring result{};
    for (int i=0; i<Sides; ++i) {
        const float angle=(i+.5f)*glm::two_pi<float>()/Sides;
        result[i]={std::cos(angle)*radius,std::sin(angle)*radius,z};
    }
    return result;
}

void disk(AssetBuilder& b, float lowerRadius, float upperRadius,
          float back, float front, Block material, glm::vec3 color)
{
    const auto low=ring(lowerRadius,back), high=ring(upperRadius,front);
    for (int i=0; i<Sides; ++i) {
        const int next=(i+1)%Sides;
        b.quad(low[i],low[next],high[next],high[i],material,color*.83f);
        b.triangle({0,0,back},low[next],low[i],material,color*.71f);
        b.triangle({0,0,front},high[i],high[next],material,color);
    }
}

void bezel(AssetBuilder& b, float innerRadius, float outerRadius,
           float back, float front, glm::vec3 color)
{
    const auto innerLow=ring(innerRadius,back), innerHigh=ring(innerRadius,front);
    const auto outerLow=ring(outerRadius,back), outerHigh=ring(outerRadius,front);
    for (int i=0; i<Sides; ++i) {
        const int next=(i+1)%Sides;
        b.quad(outerLow[i],outerLow[next],outerHigh[next],outerHigh[i],Block::CopperOre,color*.78f);
        b.quad(innerLow[next],innerLow[i],innerHigh[i],innerHigh[next],Block::CopperOre,color*.62f);
        b.quad(innerHigh[i],outerHigh[i],outerHigh[next],innerHigh[next],Block::CopperOre,color);
        b.quad(innerLow[next],outerLow[next],outerLow[i],innerLow[i],Block::CopperOre,color*.72f);
    }
}

bool finite(glm::vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

MeshData buildHeldCompass()
{
    MeshData mesh;
    const HubAsset origin{};
    AssetBuilder b(mesh,origin);

    // The bevelled back, dark casing and raised brass rim leave the pale dial
    // visibly recessed. Every piece has a back and real side faces.
    disk(b,.233f,.267f,-.077f,-.046f,Block::IronOre,Iron);
    disk(b,.267f,.267f,-.046f,.026f,Block::IronOre,Iron*1.15f);
    disk(b,.267f,.247f,.026f,.047f,Block::CopperOre,Brass*.87f);
    disk(b,.219f,.219f,.047f,.050f,Block::IronOre,Dial);
    bezel(b,.218f,.247f,.047f,.073f,Brass);
    bezel(b,.208f,.217f,.050f,.056f,Brass*.66f);

    // Four major indices and twelve minor marks read clearly at hand scale.
    // They indicate relative direction; the needle tracks the section exit.
    for (int i=0; i<16; ++i) {
        const float angle=i*glm::two_pi<float>()/16;
        const glm::vec3 axis{std::sin(angle),std::cos(angle),0};
        const bool major=i%4==0;
        b.beam(axis*(major ? .160f : .183f)+glm::vec3(0,0,.055f),
               axis*.200f+glm::vec3(0,0,.055f),major ? .010f : .005f,
               Block::IronOre,major ? Ink : Ink*1.35f);
    }

    // An understated inset cross sits beneath the moving needle; four iron
    // rivets break up the brass perimeter without adding a floating handle.
    for (const glm::vec3 axis : {glm::vec3(1,0,0),glm::vec3(0,1,0)})
        b.beam(-axis*.113f+glm::vec3(0,0,.052f),
               axis*.113f+glm::vec3(0,0,.052f),.004f,Block::IronOre,Dial*.77f);
    for (float x : {-.163f,.163f}) for (float y : {-.163f,.163f})
        b.box({x-.010f,y-.010f,.072f},{x+.010f,y+.010f,.083f},Block::IronOre,Iron*1.45f);

    // The fixed axle cap conceals the join between the red and brass halves.
    disk(b,.020f,.015f,.067f,.079f,Block::CopperOre,Brass*1.21f);
    disk(b,.010f,.007f,.079f,.085f,Block::IronOre,Iron*1.8f);
    return mesh;
}

MeshData buildHeldCompassNeedle()
{
    MeshData mesh;
    const HubAsset origin{};
    AssetBuilder b(mesh,origin);
    const std::array<glm::vec2,4> outline{{{0,.157f},{-.029f,0},{0,-.122f},{.029f,0}}};
    for (std::size_t i=0; i<outline.size(); ++i) {
        const auto next=(i+1)%outline.size();
        const auto a=outline[i],c=outline[next];
        const glm::vec3 lowA{a,.058f},lowC{c,.058f};
        const glm::vec3 highA{a,.063f},highC{c,.063f};
        const bool forward=i==0 || i==3;
        const glm::vec3 color=(forward ? Red : Brass*1.20f)*((i&1) ? .82f : 1.0f);
        b.quad(lowA,lowC,highC,highA,Block::IronOre,color*.70f);
        b.triangle({0,0,.058f},lowC,lowA,Block::IronOre,color*.65f);
        b.triangle({0,0,.075f},highA,highC,Block::IronOre,color);
    }
    return mesh;
}

glm::mat4 heldCompassPose(float timeSeconds, float aspectRatio)
{
    if (!std::isfinite(timeSeconds) || timeSeconds<0) timeSeconds=0;
    if (!std::isfinite(aspectRatio) || aspectRatio<=0) aspectRatio=16.0f/9.0f;
    const float aspect=std::clamp(aspectRatio,.6f,3.6f);
    const float phase=std::fmod(timeSeconds,8.0f)*glm::quarter_pi<float>();
    auto pose=glm::translate(glm::mat4(1),
        {.36f*aspect,-.405f+std::sin(phase*2)*.004f,-1.20f});
    pose=glm::rotate(pose,glm::radians(-7.0f+std::sin(phase)*1.0f),{0,0,1});
    pose=glm::rotate(pose,glm::radians(-13.0f+std::sin(phase)*.7f),{0,1,0});
    pose=glm::rotate(pose,glm::radians(-18.0f+std::sin(phase*2)*.6f),{1,0,0});
    const float scale=.88f*std::min(1.0f,aspect/(4.0f/3.0f));
    return glm::scale(pose,glm::vec3(scale));
}

glm::mat4 heldCompassNeedlePose(float bearingRadians)
{
    if (!std::isfinite(bearingRadians)) bearingRadians=0;
    return glm::rotate(glm::mat4(1),-std::remainder(bearingRadians,glm::two_pi<float>()),{0,0,1});
}

float compassBearing(glm::vec3 position, glm::vec3 forward, glm::vec3 target)
{
    if (!finite(position) || !finite(forward) || !finite(target)) return 0;
    const glm::dvec2 facing{forward.x,forward.z};
    const glm::dvec2 direction{static_cast<double>(target.x)-position.x,
                             static_cast<double>(target.z)-position.z};
    if (glm::dot(facing,facing)<1e-12 || glm::dot(direction,direction)<1e-12) return 0;
    // In a Y-up world, cross(forward, up) is the camera's horizontal right.
    const double towardRight=-facing.y*direction.x+facing.x*direction.y;
    return static_cast<float>(std::atan2(towardRight,glm::dot(facing,direction)));
}

} // namespace voxel
