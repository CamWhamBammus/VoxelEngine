#include "graphics/held_compass.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace voxel;
namespace {
bool finite(glm::vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool near(float a, float b)
{
    return std::abs(a-b)<.00001f;
}
bool near(const glm::mat4& a, const glm::mat4& b)
{
    for (int c=0; c<4; ++c) for (int r=0; r<4; ++r)
        if (!near(a[c][r],b[c][r])) return false;
    return true;
}
using Point=std::array<int,3>;
Point point(glm::vec3 p)
{
    return {static_cast<int>(std::lround(p.x*1000000)),
            static_cast<int>(std::lround(p.y*1000000)),
            static_cast<int>(std::lround(p.z*1000000))};
}

bool closedSolid(const MeshData& mesh)
{
    if (mesh.indices.empty() || mesh.indices.size()%3!=0) return false;
    for (const auto index : mesh.indices) if (index>=mesh.vertices.size()) return false;
    std::map<std::pair<Point,Point>,int> edges;
    double volume=0;
    for (std::size_t i=0; i<mesh.indices.size(); i+=3) {
        const auto& a=mesh.vertices[mesh.indices[i]];
        const auto& b=mesh.vertices[mesh.indices[i+1]];
        const auto& c=mesh.vertices[mesh.indices[i+2]];
        const auto cross=glm::cross(b.position-a.position,c.position-a.position);
        if (glm::length(cross)<1e-8f || glm::dot(cross,a.normal)<=0) return false;
        volume+=glm::dot(a.position,glm::cross(b.position,c.position))/6.0;
        const std::array<Point,3> points{{point(a.position),point(b.position),point(c.position)}};
        for (int j=0; j<3; ++j) {
            const auto p=points[j],q=points[(j+1)%3];
            if (p<q) ++edges[{p,q}];
            else --edges[{q,p}];
        }
    }
    return volume>0 && std::all_of(edges.begin(),edges.end(),[](const auto& edge) { return edge.second==0; });
}
}

int main()
{
    int failures=0;
    const auto check=[&](bool pass,const char* message) {
        if (!pass) { std::cerr<<message<<'\n'; ++failures; }
    };
    const auto body=buildHeldCompass(),needle=buildHeldCompassNeedle();
    check(closedSolid(body) && closedSolid(needle),
          "Compass case and needle must have closed, outward-facing, nondegenerate geometry");
    check(body.indices.size()/3<2500 && needle.indices.size()/3<100,
          "Compass should remain a compact reusable two-part model");
    bool valid=true,brass=false,iron=false,red=false,gold=false;
    glm::vec3 bodyLo{100},bodyHi{-100},needleLo{100},needleHi{-100};
    for (const auto* mesh : {&body,&needle}) for (const auto& v : mesh->vertices) {
        valid &= finite(v.position) && finite(v.normal) && finite(v.color)
            && std::abs(glm::length(v.normal)-1)<.001f;
        valid &= v.material==static_cast<float>(Block::IronOre)
            || v.material==static_cast<float>(Block::CopperOre);
    }
    for (const auto& v : body.vertices) {
        bodyLo=glm::min(bodyLo,v.position);
        bodyHi=glm::max(bodyHi,v.position);
        brass |= v.material==static_cast<float>(Block::CopperOre);
        iron |= v.material==static_cast<float>(Block::IronOre);
    }
    for (const auto& v : needle.vertices) {
        needleLo=glm::min(needleLo,v.position);
        needleHi=glm::max(needleHi,v.position);
        red |= v.position.y>.1f && v.color.r>v.color.g*3;
        gold |= v.position.y<-.1f && v.color.g>v.color.b*1.5f;
    }
    check(valid && brass && iron,"Compass must have finite unit normals and non-emissive brass and iron materials");
    check(bodyHi.x-bodyLo.x>.5f && bodyHi.y-bodyLo.y>.5f
          && bodyHi.z-bodyLo.z>.15f && bodyHi.x-bodyLo.x<.6f,
          "Compass needs a compact, thick faceted case instead of an icon or a long handle");
    check(red && gold && needleHi.z-needleLo.z>.015f && needleLo.z>.05f,
          "Raised needle needs a solid red exit-pointing half and brass counterweight");

    bool bounded=true,poseFinite=true,smooth=true;
    float minDepth=100,maxDepth=-100;
    for (float aspect : {.75f,4.0f/3.0f,16.0f/9.0f,21.0f/9.0f,32.0f/9.0f}) {
        const auto projection=glm::perspective(glm::radians(65.0f),aspect,.03f,10.0f);
        glm::vec3 previousCentre{};
        for (int frame=0; frame<=240; ++frame) {
            const auto pose=heldCompassPose(frame/30.0f,aspect);
            for (int c=0; c<4; ++c) for (int r=0; r<4; ++r)
                poseFinite &= std::isfinite(pose[c][r]);
            poseFinite &= glm::determinant(pose)>.05f;
            const auto centre=glm::vec3(pose*glm::vec4(0,0,0,1));
            if (frame) smooth &= glm::length(centre-previousCentre)<.002f;
            previousCentre=centre;
            for (const auto& v : body.vertices) {
                const auto p=pose*glm::vec4(v.position,1);
                const auto clip=projection*p;
                const auto ndc=glm::vec3(clip)/clip.w;
                minDepth=std::min(minDepth,p.z);
                maxDepth=std::max(maxDepth,p.z);
                bounded &= finite(ndc) && p.z<-.15f && ndc.x>.18f
                    && ndc.x<.98f && ndc.y<-.15f && ndc.y>-.98f;
            }
        }
    }
    check(poseFinite && bounded && smooth && maxDepth-minDepth>.2f,
          "Compass must sway gently in the lower-right view, retain depth, and leave the centre clear");
    const float aspect=16.0f/9.0f;
    const auto idle=heldCompassPose(0,aspect);
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    check(!near(idle,heldCompassPose(2,aspect)) && near(idle,heldCompassPose(8,aspect)),
          "Idle sway must move and loop seamlessly");
    check(near(idle,heldCompassPose(-1,aspect)) && near(idle,heldCompassPose(nan,nan))
          && near(idle,heldCompassPose(inf,-1)),"Invalid pose inputs must return the safe resting pose");

    bool bearings=true,rotation=true;
    const glm::vec3 position{12,8,-5};
    const std::array<glm::vec3,4> directions{{{0,0,-1},{1,0,0},{0,0,1},{-1,0,0}}};
    for (std::size_t facing=0; facing<directions.size(); ++facing) {
        for (std::size_t target=0; target<directions.size(); ++target) {
            const float expected=static_cast<float>((target+4-facing)%4)*glm::half_pi<float>();
            const float bearing=compassBearing(position,directions[facing],position+directions[target]*20.0f);
            bearings &= std::abs(std::remainder(bearing-expected,glm::two_pi<float>()))<.00001f;
            const auto tip=glm::vec3(heldCompassNeedlePose(bearing)*glm::vec4(0,1,0,0));
            rotation &= glm::length(tip-glm::vec3(std::sin(expected),std::cos(expected),0))<.00001f;
            const auto pivot=glm::vec3(heldCompassNeedlePose(bearing)*glm::vec4(0,0,.067f,1));
            rotation &= glm::length(pivot-glm::vec3(0,0,.067f))<.00001f;
        }
    }
    check(bearings && rotation,"Every camera heading must turn the needle clockwise toward the correct exit bearing without moving its axle");
    check(near(compassBearing({0,0,0},{0,0,-1},{10,200,-10}),glm::quarter_pi<float>())
          && near(compassBearing({10,0,0},{0,0,-1},{10,200,-10}),0)
          && near(compassBearing({0,0,0},{0,.9f,-.1f},{-10,-200,-10}),-glm::quarter_pi<float>()),
          "Bearing must update with player and exit position, independent of elevation and camera pitch");
    check(near(compassBearing(position,{0,0,-1},position),0)
          && near(compassBearing(position,{0,0,-1},position+glm::vec3(0,100,0)),0)
          && near(compassBearing(position,{0,1,0},{10,0,10}),0)
          && near(compassBearing({nan,0,0},{0,0,-1},position),0)
          && near(compassBearing(position,{0,0,-1},{inf,0,0}),0)
          && near(heldCompassNeedlePose(nan),glm::mat4(1))
          && near(heldCompassNeedlePose(inf),glm::mat4(1)),
          "Coincident, vertical and invalid targets must not create an invalid needle transform");
    return failures ? 1 : 0;
}
