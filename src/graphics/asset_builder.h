#pragma once

#include "hub_assets.h"
#include <cmath>
#include <glm/geometric.hpp>

namespace voxel {

// Shared geometry helpers keep each asset definition a short list of shapes.
class AssetBuilder {
public:
    AssetBuilder(MeshData& mesh, const HubAsset& asset) : mesh_(mesh), asset_(asset) {}

    glm::vec3 point(glm::vec3 p) const
    {
        return rotate(p) + asset_.position;
    }

    void triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, Block material, glm::vec3 color, bool twoSided = false)
    {
        // Tiny petals lose precision if their edges are subtracted after a
        // large world translation. Compute their normal in model space first.
        const auto normal = rotate(glm::normalize(glm::cross(b - a, c - a)));
        a = point(a); b = point(b); c = point(c);
        const auto first = static_cast<std::uint32_t>(mesh_.vertices.size());
        for (const auto p : {a, b, c}) mesh_.vertices.push_back({p, normal, color, static_cast<float>(material)});
        mesh_.indices.insert(mesh_.indices.end(), {first, first + 1, first + 2});
        if (twoSided) {
            for (const auto p : {c, b, a}) mesh_.vertices.push_back({p, -normal, color, static_cast<float>(material)});
            mesh_.indices.insert(mesh_.indices.end(), {first + 3, first + 4, first + 5});
        }
    }

    void quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, Block material, glm::vec3 color, bool twoSided = false)
    {
        triangle(a, b, c, material, color, twoSided);
        triangle(a, c, d, material, color, twoSided);
    }

    void box(glm::vec3 lo, glm::vec3 hi, Block material, glm::vec3 color)
    {
        const float x=lo.x, y=lo.y, z=lo.z, X=hi.x, Y=hi.y, Z=hi.z;
        quad({x,y,z},{x,y,Z},{x,Y,Z},{x,Y,z},material,color);
        quad({X,y,Z},{X,y,z},{X,Y,z},{X,Y,Z},material,color);
        quad({x,y,Z},{x,y,z},{X,y,z},{X,y,Z},material,color * 0.82f);
        quad({x,Y,z},{x,Y,Z},{X,Y,Z},{X,Y,z},material,color);
        quad({X,y,z},{x,y,z},{x,Y,z},{X,Y,z},material,color);
        quad({x,y,Z},{X,y,Z},{X,Y,Z},{x,Y,Z},material,color);
    }

    void beam(glm::vec3 a, glm::vec3 b, float width, Block material, glm::vec3 color)
    {
        const auto along = glm::normalize(b - a);
        const auto across = glm::normalize(glm::cross(along, std::abs(along.y) > 0.9f ? glm::vec3(1,0,0) : glm::vec3(0,1,0))) * width * 0.5f;
        const auto up = glm::normalize(glm::cross(along, across)) * width * 0.5f;
        const glm::vec3 side[4]{across+up, -across+up, -across-up, across-up};
        for (int i=0; i<4; ++i) quad(a+side[i], a+side[(i+1)%4], b+side[(i+1)%4], b+side[i], material, color);
        quad(a+side[3],a+side[2],a+side[1],a+side[0],material,color);
        quad(b+side[0],b+side[1],b+side[2],b+side[3],material,color);
    }

private:
    glm::vec3 rotate(glm::vec3 p) const
    {
        switch (asset_.facing & 3) {
        case 1: return {p.z, p.y, -p.x};
        case 2: return {-p.x, p.y, -p.z};
        case 3: return {-p.z, p.y, p.x};
        default: return p;
        }
    }

    MeshData& mesh_;
    const HubAsset& asset_;
};

void appendHubProp(MeshData& mesh, const HubAsset& asset);
void appendHubFoliage(MeshData& mesh, const HubAsset& asset);
void appendHubPath(MeshData& mesh, const HubAsset& asset);
void appendHubNature(MeshData& mesh, const HubAsset& asset);
void appendHubGarden(MeshData& mesh, const HubAsset& asset);
void appendHubArchitecture(MeshData& mesh, const HubAsset& asset);
void appendHubBlacksmith(MeshData& mesh, const HubAsset& asset);

} // namespace voxel
