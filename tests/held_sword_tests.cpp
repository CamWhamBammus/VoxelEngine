#include "graphics/held_sword.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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
        if (std::abs(a[c][r] - b[c][r]) > .00001f) return false;
    return true;
}
}

int main()
{
    int failures = 0;
    const auto check = [&](bool pass, const char* message) {
        if (!pass) { std::cerr << message << '\n'; ++failures; }
    };
    const auto mesh = buildHeldSword();
    check(!mesh.vertices.empty() && !mesh.indices.empty() && mesh.indices.size() % 3 == 0,
          "Sword must contain indexed triangle geometry");
    bool valid = true, winding = true;
    glm::vec3 lo{100}, hi{-100};
    float bladeFront = 0, bladeBack = 0;
    bool steel = false, brass = false, leather = false;
    for (const auto& v : mesh.vertices) {
        valid &= finite(v.position) && finite(v.normal) && finite(v.color)
            && std::abs(glm::length(v.normal) - 1) < .001f;
        lo = glm::min(lo,v.position);
        hi = glm::max(hi,v.position);
        if (v.position.y > .22f && v.position.y < .80f) {
            bladeFront = std::max(bladeFront,v.position.z);
            bladeBack = std::min(bladeBack,v.position.z);
        }
        steel |= v.material == static_cast<float>(Block::IronOre);
        brass |= v.material == static_cast<float>(Block::CopperOre);
        leather |= v.material == static_cast<float>(Block::Timber);
    }
    for (auto index : mesh.indices) valid &= index < mesh.vertices.size();
    if (valid) {
        for (std::size_t i=0; i<mesh.indices.size(); i+=3) {
            const auto& a = mesh.vertices[mesh.indices[i]];
            const auto& b = mesh.vertices[mesh.indices[i+1]];
            const auto& c = mesh.vertices[mesh.indices[i+2]];
            const auto cross = glm::cross(b.position-a.position,c.position-a.position);
            winding &= glm::length(cross) > .000001f && glm::dot(cross,a.normal) > 0;
        }
    }
    check(valid && winding,"Sword must have finite unit normals and nondegenerate, consistently wound faces");
    check(bladeFront > .02f && bladeBack < -.02f && hi.y-lo.y > 1.0f && hi.y-lo.y < 1.3f,
          "Blade needs real thickness on both sides and a readable metre-long silhouette");
    check(steel && brass && leather,"Steel, guard and glove must retain distinct surface materials");

    bool bounded = true, poseFinite = true, smooth = true;
    float minDepth=100,maxDepth=-100;
    for (float aspect : {4.0f/3.0f, 16.0f/9.0f, 21.0f/9.0f, 32.0f/9.0f}) {
        const auto projection = glm::perspective(glm::radians(65.0f),aspect,.03f,10.0f);
        glm::vec3 previousTip{};
        for (int frame=0; frame<=200; ++frame) {
            const auto pose = heldSwordPose(1.0f-frame/200.0f,aspect);
            for (int c=0; c<4; ++c) for (int r=0; r<4; ++r)
                poseFinite &= std::isfinite(pose[c][r]);
            poseFinite &= glm::determinant(pose) > .1f;
            const auto tip = glm::vec3(pose * glm::vec4(0,.89f,0,1));
            minDepth = std::min(minDepth,tip.z);
            maxDepth = std::max(maxDepth,tip.z);
            if (frame > 0) smooth &= glm::length(tip-previousTip) < .06f;
            previousTip = tip;
            for (const auto& v : mesh.vertices) {
                const auto p = pose * glm::vec4(v.position,1);
                const auto clip = projection * p;
                const auto ndc = glm::vec3(clip) / clip.w;
                bounded &= finite(ndc) && p.z < -.15f && std::abs(ndc.x) < .98f
                    && ndc.y < .98f && ndc.y > -1.12f;
            }
        }
    }
    check(poseFinite && bounded,"Held sword must remain finite, in frame and clear of the near plane during attacks");
    if (!smooth || maxDepth-minDepth <= .12f) std::cerr << "Swing depth range: " << maxDepth-minDepth << " smooth: " << smooth << '\n';
    check(smooth && maxDepth-minDepth > .12f,"Swing must move through depth without abrupt pose changes");
    const auto idle = heldSwordPose(0,16.0f/9.0f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    check(near(idle,heldSwordPose(1,16.0f/9.0f)),"Swing must start and finish at the resting pose");
    check(near(idle,heldSwordPose(-10,16.0f/9.0f))
          && near(idle,heldSwordPose(10,16.0f/9.0f))
          && near(idle,heldSwordPose(nan,nan))
          && near(idle,heldSwordPose(inf,-1)),"Invalid or out-of-range inputs must produce safe resting poses");
    return failures ? 1 : 0;
}
