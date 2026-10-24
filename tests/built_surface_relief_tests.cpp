#include "graphics/built_surface_relief.h"
#include "graphics/block_relief.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <glm/geometric.hpp>

namespace {
using namespace voxel;
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

bool same(const MeshData& a, const MeshData& b)
{
    if (a.indices != b.indices || a.vertices.size() != b.vertices.size()) return false;
    for (std::size_t i = 0; i < a.vertices.size(); ++i) {
        const auto& first = a.vertices[i];
        const auto& second = b.vertices[i];
        if (first.position != second.position || first.normal != second.normal || first.color != second.color
            || first.material != second.material || first.relief != second.relief) return false;
    }
    return true;
}

void faceGeometry(Block material, glm::ivec3 cell, const BlockSurface& surface)
{
    MeshData mesh;
    appendBuiltSurfaceRelief(mesh, cell, surface, material, 628);
    check(!mesh.indices.empty(), "each exposed manufactured face has actual relief");
    check(mesh.indices.size() / 3 <= 120, "each face stays within its small tile budget");
    if (material == Block::RoofTiles) {
        check(mesh.indices.size() / 3 <= 54, "roof courses use compact wedges without hidden rear bevels");
        check(mesh.vertices.size() * 5 <= mesh.indices.size() * 4,
            "roof caps and lips share vertices instead of duplicating every triangle");
    } else if (material != Block::Path) {
        check(mesh.vertices.size() * 5 <= mesh.indices.size() * 4,
            "planar brick and board faces share their vertex rings");
    }
    bool bevel = false, raised = false, embedded = false;
    const auto normal = glm::vec3(surface.normal);
    const auto plane = glm::vec3(cell) + surface.minimum;
    for (const auto& vertex : mesh.vertices) {
        check(vertex.material == static_cast<float>(stairMaterial(material)), "relief retains the base material ID");
        check(glm::vec3(vertex.relief) == normal, "paint keeps the original face normal on beveled edges");
        check(std::abs(glm::length(vertex.normal) - 1) < .0001f, "real face normals stay finite and normalized");
        check(vertex.relief.w >= -.0041f && vertex.relief.w <= .0451f, "surface depth stays shallow and embedded");
        const auto projected = vertex.position - normal * vertex.relief.w;
        check(std::abs(glm::dot(projected - plane, normal)) < .0001f, "signed relief depth projects onto its supporting plane");
        for (int axis = 0; axis < 3; ++axis) {
            if (normal[axis] != 0) continue;
            check(projected[axis] >= cell[axis] + surface.minimum[axis] + .0058f
                && projected[axis] <= cell[axis] + surface.maximum[axis] - .0058f,
                "bevels fit inside the exposed surface without crossing a stair edge");
        }
        raised |= vertex.relief.w > .01f;
        embedded |= vertex.relief.w < -.003f;
        bevel |= glm::dot(vertex.normal, normal) < .95f;
    }
    check(raised && embedded && bevel, "tiles have a raised cap, buried skirt, and angled lighting facets");
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto& a = mesh.vertices[mesh.indices[i]];
        const auto& b = mesh.vertices[mesh.indices[i + 1]];
        const auto& c = mesh.vertices[mesh.indices[i + 2]];
        const auto cross = glm::cross(b.position - a.position, c.position - a.position);
        check(glm::dot(cross, cross) > .00000000001f, "clipping does not leave degenerate triangles");
        check(glm::dot(glm::normalize(cross), a.normal) > .9999f, "triangle winding agrees with the facet lighting normal");
        check(glm::dot(a.normal, normal) > 0, "the face bevel points into the exposed half-space");
    }
    MeshData again;
    appendBuiltSurfaceRelief(again, cell, surface, material, 628);
    check(same(mesh, again), "rebuilding a surface preserves its geometry and stone colors");
}

bool hasRaisedFace(const MeshData& mesh, glm::ivec3 cell, const BlockSurface& face)
{
    const auto normal = glm::vec3(face.normal);
    const auto low = glm::vec3(cell) + face.minimum;
    const auto high = glm::vec3(cell) + face.maximum;
    for (const auto& vertex : mesh.vertices) {
        if (glm::vec3(vertex.relief) != normal || vertex.relief.w < .006f) continue;
        const auto point = vertex.position - normal * vertex.relief.w;
        bool inside = true;
        for (int axis = 0; axis < 3; ++axis)
            inside &= point[axis] >= low[axis] - .0001f && point[axis] <= high[axis] + .0001f;
        if (inside) return true;
    }
    return false;
}

void allStairs()
{
    World world(8321);
    constexpr std::size_t chunk = World::Radius * (World::Width + 1);
    // Only the center chunk and its sampling border are used by this fixture.
    for (int z = -1; z <= Chunk::Size; ++z)
        for (int y = 0; y <= Chunk::Size; ++y)
            for (int x = -1; x <= Chunk::Size; ++x) world.set({x, y, z}, Block::Air);
    constexpr glm::ivec3 cell(5, 6, 7);
    constexpr std::array materials{Block::Planks, Block::Stone, Block::Brick, Block::Limestone,
        Block::Slate, Block::MossStone, Block::Path, Block::AgedPlanks};
    for (const auto material : materials) {
        for (int orientation = 0; orientation < 4; ++orientation) {
            const auto block = stairBlock(material, orientation);
            world.set(cell, block);
            const auto mesh = buildBlockRelief(world, chunk);
            const auto surfaces = blockSurfaces(block);
            for (int i = 0; i < surfaces.count; ++i) {
                if (!hasRaisedFace(mesh, cell, surfaces.faces[i])) {
                    std::cerr << blockName(block) << " facing " << orientation << " surface " << i << '\n';
                    check(false, "every stair material and orientation has raised treads, risers, sides, and underside");
                }
            }
        }
    }
}
} // namespace

int main()
{
    constexpr std::array materials{Block::Brick, Block::MossBrick, Block::Path,
        Block::Planks, Block::AgedPlanks, Block::RoofTiles};
    for (const auto material : materials) {
        check(hasBuiltSurfaceRelief(material), "manufactured materials are recognized");
        for (const auto cell : {glm::ivec3(0, 5, 0), glm::ivec3(-31, 47, 18), glm::ivec3(127, 94, -112)}) {
            const auto cube = blockSurfaces(material);
            for (int i = 0; i < cube.count; ++i) {
                faceGeometry(material, cell, cube.faces[i]);
                auto clipped = cube.faces[i];
                for (int axis = 0; axis < 3; ++axis) {
                    if (clipped.normal[axis] == 0) {
                        clipped.minimum[axis] = .25f;
                        clipped.maximum[axis] = .75f;
                    }
                }
                faceGeometry(material, cell, clipped);
            }
            if (!isStair(stairBlock(material, 0))) continue;
            for (int orientation = 0; orientation < 4; ++orientation) {
                const auto stair = blockSurfaces(stairBlock(material, orientation));
                for (int i = 0; i < stair.count; ++i) faceGeometry(material, cell, stair.faces[i]);
            }
        }
    }
    MeshData unsupported;
    const auto top = BlockSurface{{0, 1, 0}, {1, 1, 1}, {0, 1, 0}};
    for (const auto block : {Block::Air, Block::Water, Block::Leaves, Block::Lantern}) {
        check(!hasBuiltSurfaceRelief(block), "non-building materials keep their own surface treatment");
        appendBuiltSurfaceRelief(unsupported, {0, 0, 0}, top, block, 9);
    }
    check(unsupported.vertices.empty(), "unsupported materials do not gain manufactured tile geometry");
    MeshData path;
    appendBuiltSurfaceRelief(path, {0, 0, 0}, top, Block::Path, 9);
    check(!path.vertices.empty(), "fitted path top is populated");
    if (!path.vertices.empty()) check(std::any_of(path.vertices.begin(), path.vertices.end(), [&](const MeshVertex& vertex) {
        return vertex.color != path.vertices.front().color;
    }), "fitted slabs carry individual stone colors instead of a flat mortar color");
    allStairs();
    return failures ? 1 : 0;
}
