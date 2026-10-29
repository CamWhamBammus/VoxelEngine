#include "graphics/held_pickaxe.h"
#include "equipment.h"

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
using Point = std::array<int,3>;
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
    const auto mesh=buildHeldPickaxe();
    check(!mesh.vertices.empty() && !mesh.indices.empty() && mesh.indices.size()%3==0,
          "Pickaxe must contain indexed triangle geometry");
    bool valid=true, winding=true;
    glm::vec3 lo{100},hi{-100};
    float headFront=0,headBack=0;
    bool steel=false,brass=false,wood=false;
    for (const auto& v : mesh.vertices) {
        valid &= finite(v.position) && finite(v.normal) && finite(v.color)
            && std::abs(glm::length(v.normal)-1)<.001f;
        lo=glm::min(lo,v.position);
        hi=glm::max(hi,v.position);
        if (v.position.y>.50f) {
            headFront=std::max(headFront,v.position.z);
            headBack=std::min(headBack,v.position.z);
        }
        steel |= v.material==static_cast<float>(Block::IronOre);
        brass |= v.material==static_cast<float>(Block::CopperOre);
        wood |= v.material==static_cast<float>(Block::Timber);
    }
    for (auto index : mesh.indices) valid &= index<mesh.vertices.size();
    std::map<std::pair<Point,Point>,int> edgeBalance;
    if (valid) {
        for (std::size_t i=0; i<mesh.indices.size(); i+=3) {
            const auto& a=mesh.vertices[mesh.indices[i]];
            const auto& b=mesh.vertices[mesh.indices[i+1]];
            const auto& c=mesh.vertices[mesh.indices[i+2]];
            const auto cross=glm::cross(b.position-a.position,c.position-a.position);
            winding &= glm::length(cross)>.000001f && glm::dot(cross,a.normal)>0;
            const std::array<Point,3> points{{point(a.position),point(b.position),point(c.position)}};
            for (std::size_t edge=0; edge<3; ++edge) {
                const auto p=points[edge],q=points[(edge+1)%3];
                if (p<q) ++edgeBalance[{p,q}];
                else --edgeBalance[{q,p}];
            }
        }
    }
    check(valid && winding,"Pickaxe needs finite unit normals and consistently wound, nondegenerate faces");
    check(std::all_of(edgeBalance.begin(),edgeBalance.end(),[](const auto& entry) {
        return entry.second==0;
    }),"All pickaxe surfaces must close, including the curved points, socket and glove");
    check(headFront>.06f && headBack<-.06f && hi.x-lo.x>.85f && hi.x-lo.x<1.05f
          && hi.y-lo.y>1.0f && hi.y-lo.y<1.2f,
          "Pickaxe needs a broad curved head with real depth and a metre-long handle");
    check(steel && brass && wood,"Head, socket, shaft and glove need their own readable materials");

    for (int tier=1;tier<=GearTierCount;++tier) {
        const auto variant=buildHeldPickaxe(tier);
        check(variant.indices==mesh.indices && variant.vertices.size()==mesh.vertices.size(),
              "Every material must retain the complete 3D pickaxe mesh");
        bool unchangedShape=true;
        for (std::size_t vertex=0;vertex<mesh.vertices.size();++vertex)
            unchangedShape &= variant.vertices[vertex].position==mesh.vertices[vertex].position
                && variant.vertices[vertex].normal==mesh.vertices[vertex].normal
                && finite(variant.vertices[vertex].color);
        check(unchangedShape,"Material changes must preserve normals, framing and animation");
        if (tier>1) check(glm::length(variant.vertices.front().color-mesh.vertices.front().color)>.01f,
                         "Ore tiers need visibly distinct tool materials");
    }
    check(buildHeldPickaxe(-1).vertices.front().color==mesh.vertices.front().color
          && buildHeldPickaxe(999).vertices.front().color==buildHeldPickaxe(GearTierCount).vertices.front().color,
          "Invalid tool material tiers must clamp safely");

    bool bounded=true,poseFinite=true,smooth=true;
    float minDepth=100,maxDepth=-100;
    for (float aspect : {3.0f/4.0f,4.0f/3.0f,16.0f/9.0f,21.0f/9.0f,32.0f/9.0f}) {
        const auto projection=glm::perspective(glm::radians(65.0f),aspect,.03f,10.0f);
        glm::vec3 previousHead{};
        for (int frame=0; frame<=200; ++frame) {
            const auto pose=heldPickaxePose(1.0f-frame/200.0f,aspect);
            for (int c=0; c<4; ++c) for (int r=0; r<4; ++r)
                poseFinite &= std::isfinite(pose[c][r]);
            poseFinite &= glm::determinant(pose)>.1f;
            const auto head=glm::vec3(pose*glm::vec4(0,.68f,0,1));
            minDepth=std::min(minDepth,head.z);
            maxDepth=std::max(maxDepth,head.z);
            if (frame>0) smooth &= glm::length(head-previousHead)<.06f;
            previousHead=head;
            for (const auto& v : mesh.vertices) {
                const auto p=pose*glm::vec4(v.position,1);
                const auto clip=projection*p;
                const auto ndc=glm::vec3(clip)/clip.w;
                bounded &= finite(ndc) && p.z<-.15f && std::abs(ndc.x)<.98f
                    && ndc.y<.98f && ndc.y>-1.12f;
            }
        }
    }
    check(poseFinite && bounded,"Pickaxe must stay in frame and clear of the near plane throughout a strike");
    check(smooth && maxDepth-minDepth>.35f,"Mining stroke must move forward through depth without abrupt changes");
    const float aspect=16.0f/9.0f;
    const auto headAt=[&](float remaining) {
        return glm::vec3(heldPickaxePose(remaining,aspect)*glm::vec4(0,.68f,0,1));
    };
    const auto raised=headAt(.74f),impact=headAt(.47f),rebound=headAt(.36f);
    check(impact.z<raised.z-.35f && impact.y<raised.y-.2f
          && rebound.z>impact.z && rebound.y>impact.y,
          "Pick head must strike forward and down, then rebound from the impact");
    const auto idle=heldPickaxePose(0,aspect);
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    check(near(idle,heldPickaxePose(1,aspect)),"Repeated strikes must join at the exact resting pose");
    check(near(idle,heldPickaxePose(-10,aspect)) && near(idle,heldPickaxePose(10,aspect))
          && near(idle,heldPickaxePose(nan,nan)) && near(idle,heldPickaxePose(inf,-1)),
          "Invalid or out-of-range pose input must produce a safe resting tool");
    return failures ? 1 : 0;
}
