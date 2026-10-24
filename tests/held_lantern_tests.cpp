#include "graphics/held_lantern.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace voxel;
namespace {
bool finite(glm::vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool near(const glm::mat4& a, const glm::mat4& b)
{
    for (int c=0; c<4; ++c) for (int r=0; r<4; ++r)
        if (std::abs(a[c][r]-b[c][r])>.00001f) return false;
    return true;
}
using Point=std::array<int,3>;
Point point(glm::vec3 p)
{
    return {static_cast<int>(std::lround(p.x*1000000)),
            static_cast<int>(std::lround(p.y*1000000)),
            static_cast<int>(std::lround(p.z*1000000))};
}
}

int main()
{
    int failures=0;
    const auto check=[&](bool pass,const char* message) {
        if (!pass) { std::cerr<<message<<'\n'; ++failures; }
    };
    const auto mesh=buildHeldLantern();
    check(!mesh.vertices.empty() && !mesh.indices.empty() && mesh.indices.size()%3==0,
          "Lantern must contain indexed triangle geometry");
    check(mesh.indices.size()/3<5000,"Held lantern must stay within a small reusable mesh budget");
    bool valid=true,winding=true,iron=false,brass=false,leather=false,glass=false;
    glm::vec3 lo{100},hi{-100},glassLo{100},glassHi{-100};
    std::array<bool,4> paneFaces{};
    for (const auto& v : mesh.vertices) {
        valid &= finite(v.position) && finite(v.normal) && finite(v.color)
            && std::abs(glm::length(v.normal)-1)<.001f;
        if (v.material!=static_cast<float>(Block::Slate)) {
            lo=glm::min(lo,v.position);
            hi=glm::max(hi,v.position);
        }
        iron |= v.material==static_cast<float>(Block::IronOre);
        brass |= v.material==static_cast<float>(Block::CopperOre);
        leather |= v.material==static_cast<float>(Block::Timber);
        if (v.material==static_cast<float>(Block::Lantern)) {
            glass=true;
            glassLo=glm::min(glassLo,v.position);
            glassHi=glm::max(glassHi,v.position);
            paneFaces[0] |= v.normal.x>.99f;
            paneFaces[1] |= v.normal.x<-.99f;
            paneFaces[2] |= v.normal.z>.99f;
            paneFaces[3] |= v.normal.z<-.99f;
        }
    }
    for (auto index : mesh.indices) valid &= index<mesh.vertices.size();
    std::map<std::pair<Point,Point>,int> edgeBalance;
    double volume=0;
    if (valid) {
        for (std::size_t i=0; i<mesh.indices.size(); i+=3) {
            const auto& a=mesh.vertices[mesh.indices[i]];
            const auto& b=mesh.vertices[mesh.indices[i+1]];
            const auto& c=mesh.vertices[mesh.indices[i+2]];
            const auto cross=glm::cross(b.position-a.position,c.position-a.position);
            winding &= glm::length(cross)>.000001f && glm::dot(cross,a.normal)>0;
            volume+=glm::dot(a.position,glm::cross(b.position,c.position))/6.0;
            const std::array<Point,3> points{{point(a.position),point(b.position),point(c.position)}};
            for (std::size_t edge=0; edge<3; ++edge) {
                const auto p=points[edge],q=points[(edge+1)%3];
                if (p<q) ++edgeBalance[{p,q}];
                else --edgeBalance[{q,p}];
            }
        }
    }
    check(valid && winding,"Lantern needs finite unit normals and nondegenerate, consistently wound faces");
    check(volume>.02 && std::all_of(edgeBalance.begin(),edgeBalance.end(),[](const auto& entry) {
        return entry.second==0;
    }),"Lantern cage, cap, handle and glove must be closed outward-facing solids");
    check(iron && brass && leather && glass,"Lantern needs separate forged frame, brass, leather and luminous glass");
    check(glassHi.x-glassLo.x>.25f && glassHi.z-glassLo.z>.24f
          && glassHi.y-glassLo.y>.28f && std::all_of(paneFaces.begin(),paneFaces.end(),[](bool value){return value;}),
          "Glowing cage must have substantial depth and four genuine side panes");
    check(lo.y<-.63f && hi.y>.10f && hi.x-lo.x<.5f && hi.z-lo.z>.36f,
          "Lantern should be a compact hanging cage with a carry handle and glove above it");

    bool bounded=true,poseFinite=true,smooth=true,lightAttached=true,centreClear=true;
    float minDepth=100,maxDepth=-100;
    for (float aspect : {3.0f/4.0f,4.0f/3.0f,16.0f/9.0f,21.0f/9.0f,32.0f/9.0f}) {
        const auto projection=glm::perspective(glm::radians(65.0f),aspect,.03f,10.0f);
        glm::vec3 previousLight{};
        bool sleeveExitsRight=true;
        for (int frame=0; frame<=240; ++frame) {
            const float time=frame/30.0f;
            const auto pose=heldLanternPose(time,aspect);
            for (int c=0; c<4; ++c) for (int r=0; r<4; ++r)
                poseFinite &= std::isfinite(pose[c][r]);
            poseFinite &= glm::determinant(pose)>.05f;
            const auto light=heldLanternLightPosition(time,aspect);
            const auto localLight=glm::vec3(glm::inverse(pose)*glm::vec4(light,1));
            lightAttached &= finite(light) && glm::all(glm::greaterThan(localLight,glassLo))
                && glm::all(glm::lessThan(localLight,glassHi));
            if (frame>0) smooth &= glm::length(light-previousLight)<.003f;
            previousLight=light;
            float sleeveRight=0;
            for (const auto& v : mesh.vertices) {
                const auto p=pose*glm::vec4(v.position,1);
                const auto clip=projection*p;
                const auto ndc=glm::vec3(clip)/clip.w;
                minDepth=std::min(minDepth,p.z);
                maxDepth=std::max(maxDepth,p.z);
                const bool sleeve=v.material==static_cast<float>(Block::Slate);
                if (sleeve) sleeveRight=std::max(sleeveRight,ndc.x);
                bounded &= finite(ndc) && p.z<-.15f && ndc.x>0
                    && (sleeve || ndc.x<.98f) && ndc.y<.20f
                    && (sleeve || ndc.y>-.98f);
                centreClear &= ndc.x>.18f;
            }
            sleeveExitsRight &= sleeveRight>1.05f;
        }
        check(sleeveExitsRight,"Forearm must continue off the right edge throughout the idle cycle");
    }
    check(poseFinite && bounded && centreClear,
          "Lantern must fit the lower-right view, leave the aim clear and avoid clipping across aspect ratios");
    check(smooth && lightAttached && maxDepth-minDepth>.20f,
          "Lantern must retain real view-space depth and carry its light smoothly inside the glass");
    const float aspect=16.0f/9.0f;
    const auto idle=heldLanternPose(0,aspect);
    const auto later=heldLanternPose(2,aspect);
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    check(!near(idle,later) && near(idle,heldLanternPose(8,aspect)),
          "Gentle idle sway must move and return seamlessly at the end of its cycle");
    check(near(idle,heldLanternPose(-1,aspect)) && near(idle,heldLanternPose(nan,nan))
          && near(idle,heldLanternPose(inf,-1)),"Invalid input must produce a safe idle pose");
    return failures ? 1 : 0;
}
