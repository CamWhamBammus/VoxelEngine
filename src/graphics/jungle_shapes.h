#pragma once

#include "graphics/asset_builder.h"

#include <array>
#include <cstdint>

namespace voxel::jungle {

inline constexpr float Pi = 3.14159265359f;

inline float noise(std::uint32_t seed, std::uint32_t part)
{
    auto value = seed ^ (part * 0x9e3779b9u + 0x85ebca6bu);
    value = (value ^ (value >> 16)) * 0x7feb352du;
    value = (value ^ (value >> 15)) * 0x846ca68bu;
    return float((value ^ (value >> 16)) & 0x00ffffffu) / 16777216.0f;
}

// Closed, tapered hexagons keep twigs and vines solid from any viewpoint.
inline void stem(AssetBuilder& builder, glm::vec3 a, glm::vec3 b,
                 float radiusA, float radiusB, glm::vec3 color,
                 Block material = Block::RootWood)
{
    if (glm::length(b-a) < .0001f || radiusA <= 0 || radiusB <= 0) return;
    color = glm::clamp(color,glm::vec3(0),glm::vec3(1));
    const auto along = glm::normalize(b-a);
    const auto reference = std::abs(along.y) > .9f ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
    const auto across = glm::normalize(glm::cross(reference,along));
    const auto up = glm::cross(along,across);
    std::array<glm::vec3,6> lower{},upper{};
    for (int i=0;i<6;++i) {
        const auto radial = across*std::cos(float(i)*Pi/3)+up*std::sin(float(i)*Pi/3);
        lower[i] = a+radial*radiusA;
        upper[i] = b+radial*radiusB;
    }
    for (int i=0;i<6;++i) {
        const int next=(i+1)%6;
        builder.quad(lower[i],lower[next],upper[next],upper[i],material,color*(.78f+.035f*float(i)));
        builder.triangle(a,lower[next],lower[i],material,color*.76f);
        builder.triangle(b,upper[i],upper[next],material,color);
    }
}

// A raised midrib, folded blade and thin closed rim are real geometry. This
// deliberately uses neither alpha cards nor the block leaf cutout texture.
inline void leaf(AssetBuilder& builder, glm::vec3 foot, glm::vec3 tip,
                 float width, glm::vec3 color, glm::vec3 facing = {0,1,0})
{
    if (glm::length(tip-foot) < .0001f || width <= 0) return;
    color = glm::clamp(color,glm::vec3(0),glm::vec3(1));
    const auto along = glm::normalize(tip-foot);
    if (glm::length(facing) < .001f || std::abs(glm::dot(along,glm::normalize(facing))) > .95f)
        facing = std::abs(along.z)<.9f ? glm::vec3(0,0,1) : glm::vec3(1,0,0);
    const auto across = glm::normalize(glm::cross(along,facing));
    const auto normal = glm::normalize(glm::cross(across,along));
    const float thickness = glm::clamp(width*.045f,.003f,.015f);
    const std::array<glm::vec3,4> rim{
        foot,glm::mix(foot,tip,.39f)+across*width,
        tip,glm::mix(foot,tip,.51f)-across*width*.88f};
    const auto ridge = glm::mix(foot,tip,.48f)+normal*(width*.24f+thickness);
    const auto underside = glm::mix(foot,tip,.48f)-normal*thickness;
    for (int i=0;i<4;++i) {
        const int next=(i+1)%4;
        const auto topA=rim[i]+normal*thickness, topB=rim[next]+normal*thickness;
        const auto bottomA=rim[i]-normal*thickness, bottomB=rim[next]-normal*thickness;
        builder.triangle(topA,topB,ridge,Block::Leaves,color*(i<2?1.0f:.84f));
        builder.triangle(bottomB,bottomA,underside,Block::Leaves,color*.68f);
        builder.quad(topA,bottomA,bottomB,topB,Block::Leaves,color*.76f);
    }
}

} // namespace voxel::jungle
