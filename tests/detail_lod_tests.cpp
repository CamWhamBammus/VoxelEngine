#include "graphics/detail_lod.h"
#include "graphics/block_relief.h"
#include "graphics/cave_details.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <glm/common.hpp>
#include <glm/vec2.hpp>

namespace {
int failures = 0;
void check(bool value, const std::string& message)
{
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
using Triangle = std::array<std::uint32_t, 3>;
std::vector<Triangle> triangles(const voxel::MeshData& mesh)
{
    std::vector<Triangle> result;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
        result.push_back({mesh.indices[i], mesh.indices[i+1], mesh.indices[i+2]});
    return result;
}
void unchangedGeometry(const voxel::MeshData& original, const voxel::DetailLodMesh& grouped)
{
    check(original.vertices.size() == grouped.mesh.vertices.size(), "LOD shares the original vertex allocation shape");
    bool sameVertices = original.vertices.size() == grouped.mesh.vertices.size();
    for (std::size_t i = 0; sameVertices && i < original.vertices.size(); ++i) {
        const auto& a = original.vertices[i]; const auto& b = grouped.mesh.vertices[i];
        sameVertices = a.position == b.position && a.normal == b.normal && a.color == b.color
            && a.material == b.material && a.relief == b.relief;
    }
    check(sameVertices, "positions, normals, materials, colors and relief attributes are unchanged");
    auto before = triangles(original), after = triangles(grouped.mesh);
    std::sort(before.begin(), before.end()); std::sort(after.begin(), after.end());
    check(before == after, "the disjoint ranges contain every original triangle exactly once with its winding");
    std::size_t end = 0;
    for (const auto range : grouped.ranges) {
        check(range.firstIndex == end && range.indexCount % 3 == 0, "ranges are contiguous complete triangles");
        end += range.indexCount;
        std::size_t previous = 0;
        for (std::size_t i = range.firstIndex; i < end; i += 3) {
            while (previous < original.indices.size()
                   && !std::equal(grouped.mesh.indices.begin()+i, grouped.mesh.indices.begin()+i+3,
                                  original.indices.begin()+previous)) previous += 3;
            check(previous < original.indices.size(), "triangle order within each range stays stable");
            previous += 3;
        }
    }
    check(end == original.indices.size(), "range union has no gaps or duplicated index slots");
}
void face(voxel::MeshData& mesh, glm::ivec3 cell, glm::vec3 normal)
{
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const auto uv : std::array<glm::vec2,4>{{{.1f,.1f},{.9f,.1f},{.9f,.9f},{.1f,.9f}}}) {
        const auto anchor = glm::vec3(cell) + (normal.y != 0 ? glm::vec3(uv.x,1,uv.y) : glm::vec3(uv.x,uv.y,1));
        mesh.vertices.push_back({anchor + normal*.04f,normal,{.7f,.6f,.4f},float(voxel::Block::Path),glm::vec4(normal,.04f)});
    }
    for (const auto index : {0u,1u,2u,0u,2u,3u}) mesh.indices.push_back(first+index);
}
std::size_t chunkAt(glm::ivec3 cell)
{
    const auto local = (cell - voxel::World::Min) / voxel::Chunk::Size;
    return std::size_t(local.x + voxel::World::Width * (local.z + voxel::World::Width * local.y));
}
} // namespace

int main()
{
    using namespace voxel;
    MeshData surface;
    for (int z = -8; z <= 8; ++z) for (int x = -8; x <= 8; ++x) {
        face(surface,{x,7,z},{0,1,0});
        face(surface,{x,7,z},{0,0,1});
    }
    const auto grouped = partitionDetailLod(surface, DetailLodKind::Relief);
    unchangedGeometry(surface, grouped);
    check(grouped.mesh.indices == partitionDetailLod(surface, DetailLodKind::Relief).mesh.indices,
          "negative and positive world positions partition identically on rebuild");
    std::vector<int> bandForCell(17*17,-1);
    bool wholeCells = true;
    for (std::size_t band = 0; band < DetailLodBandCount; ++band) {
        const auto range = grouped.ranges[band];
        for (std::size_t i = range.firstIndex; i < range.firstIndex+range.indexCount; i += 3) {
            // Eight authored vertices per cell: both independent faces must
            // stay together even though their normals and anchors differ.
            const auto cell = grouped.mesh.indices[i] / 8;
            if (bandForCell[cell] < 0) bandForCell[cell] = int(band);
            else wholeCells &= bandForCell[cell] == int(band);
        }
    }
    check(wholeCells, "all faces of a raised block share one distance band");
    check(std::all_of(grouped.ranges.begin(),grouped.ranges.end(),[](auto range){return range.indexCount > 0;}),
          "an ordinary patch uses all three distance bands");

    for (const auto kind : {DetailLodKind::Relief,DetailLodKind::Cave}) {
        float previousEnd = 0;
        for (std::size_t band = 0; band < DetailLodBandCount; ++band) {
            const auto fade = detailLodFade(kind,band);
            check(fade.start >= previousEnd && fade.end-fade.start >= 10,
                  "bands use wide nonoverlapping transitions");
            previousEnd = fade.end;
            check(detailLodCoverage(fade.start,fade) == 1 && detailLodCoverage(fade.end,fade) == 0,
                  "near detail is complete and fully faded detail is absent");
            check(std::abs(detailLodCoverage((fade.start+fade.end)*.5f,fade)-.5f)<.00001f,
                  "transition passes smoothly through half coverage");
            float last = 1;
            for (float distance = 0; distance <= fade.end+2; distance += .1f) {
                const float coverage = detailLodCoverage(distance,fade);
                check(coverage >= 0 && coverage <= last, "coverage is bounded and fades monotonically");
                last = coverage;
            }
        }
    }
    const auto fade = detailLodFade(DetailLodKind::Relief,0);
    check(detailLodVisible({43.5f,8,0},{-8,0,-8},{8,16,8},fade),
          "a chunk survives while its nearest corner still has visible detail");
    check(detailLodVisible({-43.5f,8,0},{-8,0,-8},{8,16,8},fade),
          "distance culling is symmetric at negative coordinates");
    check(!detailLodVisible({44,8,0},{-8,0,-8},{8,16,8},fade),
          "the CPU omits a range only when its entire bound is fully faded");
    check(!detailLodVisible({35,8,35},{-8,0,-8},{8,16,8},fade),
          "diagonal distance is measured against the box rather than separate axes");
    check(detailLodVisible({0,8,0},{-8,0,-8},{8,16,8},fade), "detail containing the camera remains visible");
    const auto submitted = [&](float distance) {
        std::size_t count = 0;
        for (std::size_t band=0;band<DetailLodBandCount;++band)
            if (detailLodVisible({distance,0,0},{0,0,0},{0,0,0},detailLodFade(DetailLodKind::Relief,band)))
                count += grouped.ranges[band].indexCount;
        return count;
    };
    check(submitted(24) == surface.indices.size(), "nearby relief submits every original triangle");
    check(submitted(48) > 0 && submitted(48) < surface.indices.size()*3/4,
          "middle distance removes actual submitted geometry, not just fragments");
    check(submitted(84) == 0, "far relief submits no geometry");

    // Exercise real paving, ore, stairs and natural dressing, not only synthetic
    // triangles. Underlying voxel and collision data must never be modified.
    World world(42);
    const auto chunk = chunkAt({0,10,0});
    constexpr std::array materials{Block::Path,Block::IronOre,Block::RootWood,Block::Crystal,
                                   Block::Grass,Block::MossStone,Block::PathStairs,Block::RoofTiles};
    for (std::size_t i=0;i<materials.size();++i) world.set({int(i%4)*3,10,int(i/4)*4},materials[i]);
    const World originalWorld=world;
    auto relief=buildBlockRelief(world,chunk),cave=buildCaveDetails(world,chunk);
    check(!relief.indices.empty() && !cave.indices.empty(), "real fixture contains both relief and cave geometry");
    unchangedGeometry(relief,partitionDetailLod(relief,DetailLodKind::Relief));
    unchangedGeometry(cave,partitionDetailLod(cave,DetailLodKind::Cave));
    check(world.sameContents(originalWorld), "building LOD leaves all authored voxels and collision shapes untouched");
    const auto empty=partitionDetailLod({},DetailLodKind::Relief);
    check(empty.mesh.indices.empty() && empty.ranges.back().indexCount==0, "empty chunks need no draw ranges");
    bool rejected=false;
    try { MeshData broken; broken.indices={0,1}; partitionDetailLod(std::move(broken),DetailLodKind::Relief); }
    catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"incomplete triangles are rejected");
    rejected=false;
    try { MeshData broken; broken.indices={0,1,2}; partitionDetailLod(std::move(broken),DetailLodKind::Relief); }
    catch(const std::out_of_range&) {rejected=true;}
    check(rejected,"missing vertices are rejected");
    rejected=false;
    try { auto broken=surface; broken.vertices[0].position.x=std::numeric_limits<float>::quiet_NaN();
          partitionDetailLod(std::move(broken),DetailLodKind::Relief); }
    catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"invalid positions cannot enter the spatial hash");
    return failures?1:0;
}
