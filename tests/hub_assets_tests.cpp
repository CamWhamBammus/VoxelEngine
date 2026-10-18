#include "hub_assets.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <glm/geometric.hpp>

namespace {
using namespace voxel;
int failures = 0;
constexpr std::array kinds{
    HubAssetKind::LanternCap, HubAssetKind::WallBracket, HubAssetKind::HangingLamp,
    HubAssetKind::CrystalBeacon, HubAssetKind::WindowBox, HubAssetKind::Shutters,
    HubAssetKind::Awning, HubAssetKind::RoofBrace, HubAssetKind::Banner, HubAssetKind::CrateStack,
    HubAssetKind::Fern, HubAssetKind::Flowers, HubAssetKind::Mushrooms, HubAssetKind::Reeds,
    HubAssetKind::LilyPads, HubAssetKind::Vines, HubAssetKind::Shrub,
    HubAssetKind::PathPaving, HubAssetKind::PathEdging, HubAssetKind::PathStep,
};

void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

bool same(const MeshData& a, const MeshData& b)
{
    if (a.indices != b.indices || a.vertices.size() != b.vertices.size()) return false;
    for (std::size_t i = 0; i < a.vertices.size(); ++i) {
        const auto& x = a.vertices[i]; const auto& y = b.vertices[i];
        if (x.position != y.position || x.normal != y.normal || x.color != y.color
            || x.material != y.material || x.relief != y.relief) return false;
    }
    return true;
}

void validate(const MeshData& mesh, glm::vec3 minimum, glm::vec3 maximum)
{
    check(mesh.indices.size() % 3 == 0, "asset indices form complete triangles");
    for (const auto& vertex : mesh.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            check(std::isfinite(vertex.position[axis]) && std::isfinite(vertex.normal[axis])
                  && std::isfinite(vertex.color[axis]), "asset attributes are finite");
            check(vertex.position[axis] >= minimum[axis] - 0.0001f
                  && vertex.position[axis] <= maximum[axis] + 0.0001f, "asset stays inside its culling margin");
            check(vertex.color[axis] >= 0 && vertex.color[axis] <= 1, "painted vertex colors stay in range");
        }
        check(std::abs(glm::length(vertex.normal) - 1.0f) < 0.0001f, "asset normals have unit length");
        check(std::isfinite(vertex.material) && vertex.material > 0 && vertex.material < BlockCount
              && vertex.material == std::floor(vertex.material), "assets reuse valid existing material IDs");
        check(vertex.relief == glm::vec4(0), "models do not inherit block-relief projection metadata");
    }
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size()) {
            check(false, "asset indices reference existing vertices"); continue;
        }
        const auto area = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                     mesh.vertices[c].position - mesh.vertices[a].position);
        check(glm::length(area) > 0.000001f, "asset triangles have nonzero area");
        for (const auto index : {a, b, c}) {
            check(glm::dot(glm::normalize(area), mesh.vertices[index].normal) > 0.999f,
                  "counterclockwise winding agrees with each flat face normal");
        }
    }
}

glm::vec3 turn(glm::vec3 p, int facing)
{
    for (int i = 0; i < facing; ++i) p = {p.z, p.y, -p.x};
    return p;
}

bool samePlan(const std::vector<HubAsset>& a, const std::vector<HubAsset>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].kind != b[i].kind || a[i].position != b[i].position
            || a[i].facing != b[i].facing || a[i].variation != b[i].variation) return false;
    }
    return true;
}

void validatePath(HubAssetKind kind, std::uint32_t variation)
{
    const auto mesh = buildHubAsset({kind, {}, 0, variation});
    const bool step = kind == HubAssetKind::PathStep;
    const bool edging = kind == HubAssetKind::PathEdging;
    const glm::vec3 minimum = step ? glm::vec3(-0.5f,-0.98f,0.5f)
        : glm::vec3(-0.5f,0,edging ? 0.34f : -0.5f);
    const glm::vec3 maximum = step ? glm::vec3(0.5f,0.035f,0.53f)
        : glm::vec3(0.5f,edging ? 0.09f : 0.045f,0.5f);
    validate(mesh, minimum, maximum);
    check(!mesh.indices.empty() && mesh.indices.size() / 3 < 70, "path models have a small triangle budget");
    bool bevel = false, darkJoint = false, paintedStone = false;
    float topArea = 0;
    for (const auto& vertex : mesh.vertices) {
        check(vertex.material == static_cast<float>(Block::Path), "the fitted path kit uses its dedicated paint material");
        const float normal = step ? vertex.normal.z : vertex.normal.y;
        bevel |= normal > 0.05f && normal < 0.98f;
        darkJoint |= glm::length(vertex.color) < 0.4f;
        paintedStone |= glm::length(vertex.color) > 0.7f;
    }
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto& a = mesh.vertices[mesh.indices[i]];
        const auto& b = mesh.vertices[mesh.indices[i + 1]];
        const auto& c = mesh.vertices[mesh.indices[i + 2]];
        if (a.normal.y > 0.999f && a.position.y >= 0.015f)
            topArea += std::abs(glm::cross(b.position - a.position, c.position - a.position).y) * 0.5f;
    }
    check(bevel && darkJoint && paintedStone, "paths have actual beveled faces and contrasting recessed joints");
    if (kind == HubAssetKind::PathPaving)
        check(topArea > 0.78f && topArea < 0.99f, "fitted slabs cover most of a tile while leaving narrow joints");
}

std::size_t owner(glm::vec3 position)
{
    const auto chunk = (glm::ivec3(glm::floor(position)) - World::Min) / Chunk::Size;
    return static_cast<std::size_t>(chunk.x + World::Width * (chunk.z + World::Width * chunk.y));
}

bool hasPath(const std::vector<HubAsset>& plan, HubAssetKind kind, glm::ivec3 support, int facing = -1)
{
    const auto anchor = glm::vec3(support) + glm::vec3(0.5f,1,0.5f);
    return std::any_of(plan.begin(), plan.end(), [&](const HubAsset& asset) {
        return asset.kind == kind && asset.position == anchor && (facing < 0 || asset.facing == facing);
    });
}

void checkPathPlacement(World& world)
{
    constexpr std::array<glm::ivec3, 4> sides{{{0,0,1}, {1,0,0}, {0,0,-1}, {-1,0,0}}};
    const glm::ivec3 upper{15,15,15};
    for (int facing = 0; facing < 4; ++facing) {
        const auto lower = upper + sides[facing] - glm::ivec3(0,1,0);
        world.set(upper, Block::Path); world.set(lower, Block::Path);
        const auto plan = planHubAssets(world);
        check(hasPath(plan, HubAssetKind::PathPaving, upper) && hasPath(plan, HubAssetKind::PathPaving, lower),
              "both sides of a chunk-boundary stair retain their paving");
        check(hasPath(plan, HubAssetKind::PathStep, upper, facing)
              && !hasPath(plan, HubAssetKind::PathEdging, upper, facing),
              "stair dressing faces the lower tread instead of placing a raised curb across it");
        world.set(upper + glm::ivec3(0,1,0), Block::Stone);
        const auto covered = planHubAssets(world);
        check(!hasPath(covered, HubAssetKind::PathPaving, upper)
              && !hasPath(covered, HubAssetKind::PathStep, upper, facing),
              "covering a path removes both top and stair models");
        world.set(upper + glm::ivec3(0,1,0), Block::Air);
        check(samePlan(plan, planHubAssets(world)), "uncovering a path restores the exact original kit");
        const auto before = buildHubDressing(world);
        const auto upperOwner = owner(glm::vec3(upper) + glm::vec3(0.5f,1,0.5f));
        const auto lowerOwner = owner(glm::vec3(lower) + glm::vec3(0.5f,1,0.5f));
        check(upperOwner != lowerOwner && !before.meshes[upperOwner].indices.empty()
              && !before.meshes[lowerOwner].indices.empty(), "stair models batch across horizontal and vertical chunk seams");
        world.set(upper, Block::Air);
        const auto mined = buildHubDressing(world);
        check(mined.meshes[upperOwner].indices.empty() && !mined.meshes[lowerOwner].indices.empty(),
              "mining an upper tread removes its owned geometry while preserving the lower chunk");
        world.set(lower, Block::Air);
    }

    const glm::ivec3 left{-1,15,-1}, right{0,15,-1};
    world.set(left, Block::Path); world.set(right, Block::Path);
    const auto joined = planHubAssets(world);
    check(!hasPath(joined, HubAssetKind::PathEdging, left, 1)
          && !hasPath(joined, HubAssetKind::PathEdging, right, 3),
          "level paving has no doubled curb at a negative-coordinate chunk seam");
    world.set(right, Block::Planks);
    check(!hasPath(planHubAssets(world), HubAssetKind::PathEdging, left, 1),
          "a wooden threshold meets paving without a curb across its opening");
    world.set(right, Block::Grass);
    check(hasPath(planHubAssets(world), HubAssetKind::PathEdging, left, 1),
          "an exposed soil border receives narrow edging");
    world.set(left, Block::Air); world.set(right, Block::Air);
    const glm::ivec3 ceiling{0, World::MaxExclusive.y - 1, 0};
    world.set(ceiling, Block::Path);
    check(planHubAssets(world).empty(), "a path at the world ceiling cannot create an out-of-bounds top model");
    world.set(ceiling, Block::Air);
}
} // namespace

int main()
{
    using namespace voxel;
    int varyingKinds = 0;
    const glm::vec3 anchor{-13.25f, 22.5f, 15.75f};
    for (const auto kind : kinds) {
        varyingKinds += !same(buildHubAsset({kind, anchor, 0, 0}), buildHubAsset({kind, anchor, 0, 1}));
        if (kind >= HubAssetKind::PathPaving)
            check(!same(buildHubAsset({kind, {}, 0, 0}), buildHubAsset({kind, {}, 0, 1})),
                  "each path model varies its fitted shapes or stone palette");
        for (const auto variation : {0u, 1u, 42u, std::numeric_limits<std::uint32_t>::max()}) {
            const HubAsset asset{kind, anchor, 0, variation};
            const auto original = buildHubAsset(asset);
            check(!original.indices.empty(), "every asset kind produces visible geometry");
            check(same(original, buildHubAsset(asset)), "an identical variant is deterministic");
            validate(original, anchor - HubAssetMargin, anchor + HubAssetMargin);
            if (kind >= HubAssetKind::PathPaving) validatePath(kind, variation);
            for (int facing = 1; facing < 4; ++facing) {
                const auto rotated = buildHubAsset({kind, anchor, facing, variation});
                validate(rotated, anchor - HubAssetMargin, anchor + HubAssetMargin);
                check(original.indices == rotated.indices && original.vertices.size() == rotated.vertices.size(),
                      "rotation preserves model topology");
                if (original.vertices.size() != rotated.vertices.size()) continue;
                for (std::size_t i = 0; i < original.vertices.size(); ++i) {
                    check(glm::length(rotated.vertices[i].position
                          - (anchor + turn(original.vertices[i].position - anchor, facing))) < 0.0001f,
                          "facing rotates geometry around the asset origin");
                    check(glm::length(rotated.vertices[i].normal - turn(original.vertices[i].normal, facing)) < 0.0001f,
                          "facing rotates normals with geometry");
                    check(rotated.vertices[i].color == original.vertices[i].color
                          && rotated.vertices[i].material == original.vertices[i].material,
                          "rotation preserves the painted palette and materials");
                }
            }
        }
    }
    check(varyingKinds >= 7, "variants provide meaningful visual variation across the asset set");

    World world(1337);
    for (int y = World::Min.y; y < World::MaxExclusive.y; ++y) {
        for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
            for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) world.set({x,y,z}, Block::Air);
        }
    }
    check(planHubAssets(world).empty(), "empty worlds do not acquire floating hub models");
    const auto empty = buildHubDressing(world);
    check(empty.lights.empty(), "empty worlds do not acquire asset lights");
    for (const auto& mesh : empty.meshes) check(mesh.indices.empty(), "empty worlds have empty asset meshes");
    checkPathPlacement(world);

    buildUndergroundScene(world);
    const auto before = world;
    const auto plan = planHubAssets(world);
    const auto paths = std::count_if(plan.begin(), plan.end(), [](const HubAsset& asset) {
        return asset.kind >= HubAssetKind::PathPaving;
    });
    check(plan.size() - paths >= 17 && plan.size() - paths < 2000, "the hub retains its bounded non-path dressing plan");
    check(paths >= 1000 && paths < 20000, "paving follows substantial streets with a separate bounded model budget");
    check(samePlan(plan, planHubAssets(world)), "the asset plan is deterministic");
    std::array<bool, kinds.size()> present{};
    std::size_t expectedIndices = 0;
    for (const auto& asset : plan) {
        const auto kind = static_cast<std::size_t>(asset.kind);
        check(kind < present.size(), "the plan uses known asset kinds");
        if (kind < present.size()) present[kind] = true;
        check(asset.facing >= 0 && asset.facing < 4, "planned facing is a quarter turn");
        check(World::contains(glm::ivec3(glm::floor(asset.position))), "asset origins have owning world chunks");
        expectedIndices += buildHubAsset(asset).indices.size();
    }
    check(std::all_of(present.begin(), present.end(), [](bool found) { return found; }),
          "the actual hub uses all twenty asset kinds");
    const auto dressing = buildHubDressing(world);
    const auto repeated = buildHubDressing(world);
    std::size_t actualIndices = 0;
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        const glm::vec3 minimum(world.origin(index));
        validate(dressing.meshes[index], minimum - HubAssetMargin,
                 minimum + float(Chunk::Size) + HubAssetMargin);
        check(same(dressing.meshes[index], repeated.meshes[index]), "chunk asset batches are deterministic");
        actualIndices += dressing.meshes[index].indices.size();
    }
    check(actualIndices == expectedIndices, "batching retains every planned model exactly once");
    check(!dressing.lights.empty() && dressing.lights.size() <= 30, "asset lights have a small explicit budget");
    check(dressing.lights.size() == repeated.lights.size(), "the asset light count is deterministic");
    for (std::size_t i = 0; i < dressing.lights.size(); ++i) {
        const auto& light = dressing.lights[i];
        check(std::isfinite(light.radius) && light.radius > 0 && light.radius <= 11,
              "asset light radii fit the bounded voxel shadow traversal");
        check(World::contains(glm::ivec3(glm::floor(light.position))), "asset lights stay inside world bounds");
        for (int axis = 0; axis < 3; ++axis) {
            check(std::isfinite(light.position[axis]) && std::isfinite(light.color[axis]) && light.color[axis] >= 0,
                  "asset lights have finite positions and nonnegative colors");
        }
        if (i < repeated.lights.size()) {
            const auto& other = repeated.lights[i];
            check(light.position == other.position && light.color == other.color && light.radius == other.radius,
                  "asset lights stay at deterministic world-space locations");
        }
    }
    check(world.sameContents(before), "planning and building cosmetic assets never changes gameplay voxels");
    return failures ? 1 : 0;
}
