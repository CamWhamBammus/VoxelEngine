#include "hub_assets.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <utility>
#include <glm/geometric.hpp>

namespace {
using namespace voxel;
int failures = 0;
constexpr auto kinds = [] {
    std::array<HubAssetKind, HubAssetCount> result{};
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = static_cast<HubAssetKind>(i);
    return result;
}();

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

void validateGarden(HubAssetKind kind, std::uint32_t variation)
{
    const auto mesh = buildHubAsset({kind, {}, 0, variation});
    const float height = kind == HubAssetKind::BroadleafClump ? 2.3f : kind == HubAssetKind::FernBed ? 1.7f : 0.5f;
    validate(mesh, {-1.35f,0,-1.35f}, {1.35f,height,1.35f});
    check(mesh.indices.size() / 3 >= 300 && mesh.indices.size() / 3 <= 800,
          "layered gardens retain their intended detail within a bounded triangle budget");
    glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
    bool green = false, paleAccent = false;
    for (const auto& vertex : mesh.vertices) {
        low = glm::min(low, vertex.position); high = glm::max(high, vertex.position);
        check(vertex.material != static_cast<float>(Block::GlowLeaves)
              && vertex.material != static_cast<float>(Block::Crystal)
              && vertex.material != static_cast<float>(Block::Lantern), "garden accents use painted surfaces instead of emitters");
        green |= vertex.color.g > vertex.color.r * 1.2f && vertex.color.g > vertex.color.b * 1.2f;
        paleAccent |= vertex.color.r > 0.5f && vertex.color.g > 0.6f;
    }
    check(green && paleAccent, "garden palettes combine rich foliage with pale veins or flowers");
    check(high.x - low.x > 1.0f && high.z - low.z > 1.0f,
          "garden models form broad overlapping patches rather than narrow spikes");
}

bool triangleIntersectsBox(const MeshData& mesh, std::size_t first, glm::vec3 low, glm::vec3 high)
{
    std::vector<glm::vec3> polygon;
    for (int corner = 0; corner < 3; ++corner)
        polygon.push_back(mesh.vertices[mesh.indices[first + corner]].position);
    // Clip the face itself: vertex-only checks miss triangles whose edges
    // cross a walking lane while all three corners lie outside it.
    for (int plane = 0; plane < 6 && !polygon.empty(); ++plane) {
        const int axis = plane / 2;
        const auto distance = [&](glm::vec3 p) {
            return plane % 2 == 0 ? p[axis] - low[axis] : high[axis] - p[axis];
        };
        std::vector<glm::vec3> clipped;
        for (std::size_t edge = 0; edge < polygon.size(); ++edge) {
            const auto a = polygon[edge], b = polygon[(edge + 1) % polygon.size()];
            const float da = distance(a), db = distance(b);
            if (da >= 0) clipped.push_back(a);
            if ((da >= 0) != (db >= 0)) clipped.push_back(a + (b - a) * (da / (da - db)));
        }
        polygon = std::move(clipped);
    }
    return !polygon.empty();
}

void validateRootArch(std::uint32_t variation)
{
    const auto mesh = buildHubAsset({HubAssetKind::RootArch, {}, 0, variation});
    bool leftFoot = false, rightFoot = false;
    for (const auto& vertex : mesh.vertices) {
        check(vertex.position.y >= -0.0001f, "root arch feet lie on their ground plane");
        if (std::abs(vertex.position.y) < 0.0001f) {
            leftFoot |= vertex.position.x < -2.5f;
            rightFoot |= vertex.position.x > 2.5f;
        }
    }
    check(leftFoot && rightFoot, "both sides of a root arch reach the ground");
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        check(!triangleIntersectsBox(mesh, i, {-2.4999f,-0.0001f,-4.001f}, {2.4999f,2.4999f,4.001f}),
              "root arch faces leave a five-wide opening clear below height 2.5");
}

void checkArchWalkingClearance(const World& world, const HubAsset& asset)
{
    const auto mesh = buildHubAsset(asset);
    glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
    for (const auto& vertex : mesh.vertices) {
        low = glm::min(low, vertex.position); high = glm::max(high, vertex.position);
    }
    const auto first = glm::ivec3(glm::floor(low - glm::vec3(0.3f,3.5f,0.3f)));
    const auto last = glm::ivec3(glm::floor(high + glm::vec3(0.3f,0,0.3f)));
    for (int y = first.y; y <= last.y; ++y) for (int z = first.z; z <= last.z; ++z)
        for (int x = first.x; x <= last.x; ++x) {
            const glm::ivec3 floor{x,y,z};
            const auto block = world.get(floor);
            if ((block != Block::Path && block != Block::Planks)
                || world.get(floor + glm::ivec3(0,1,0)) != Block::Air
                || world.get(floor + glm::ivec3(0,2,0)) != Block::Air) continue;
            // Preserve the entire tile plus the player's radius, including
            // side paths and steps at a different height from the arch origin.
            const auto laneLow = glm::vec3(floor) + glm::vec3(-0.3f,1.0001f,-0.3f);
            const auto laneHigh = glm::vec3(floor) + glm::vec3(1.3f,3.5f,1.3f);
            for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                if (!triangleIntersectsBox(mesh, i, laneLow, laneHigh)) continue;
                std::cerr << "Arch at " << asset.position.x << ',' << asset.position.y << ','
                          << asset.position.z << " facing " << asset.facing
                          << " intersects walking tile " << x << ',' << y << ',' << z << '\n';
                check(false, "oriented root geometry leaves every nearby Path and Planks walking lane clear");
                return;
            }
        }
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

bool containsAsset(const std::vector<HubAsset>& plan, const HubAsset& expected)
{
    return std::any_of(plan.begin(), plan.end(), [&](const HubAsset& asset) {
        return asset.kind == expected.kind && asset.position == expected.position
            && asset.facing == expected.facing;
    });
}

// These checks use the actual scene plan. One edited copy tests all attachment
// failures together, without generating another scene for each asset family.
void checkHubPlacement(const World& world, const std::vector<HubAsset>& plan)
{
    World edited = world;
    std::vector<HubAsset> removed;
    const HubAsset* garden = nullptr;
    const HubAsset* coveredGarden = nullptr;
    const HubAsset* bay = nullptr;
    const HubAsset* boulder = nullptr;
    const HubAsset* stalactite = nullptr;
    const HubAsset* rootArch = nullptr;
    std::array<bool, 4> archFacings{};
    int archCount = 0;
    const auto cell = [](glm::vec3 point) { return glm::ivec3(glm::floor(point)); };
    const auto natural = [](Block block) {
        return block == Block::Grass || block == Block::Dirt || block == Block::Stone
            || block == Block::Slate || block == Block::Limestone || block == Block::MossStone
            || block == Block::Calcite || block == Block::RootedDirt || block == Block::Moss || block == Block::GlowMoss;
    };
    const auto soil = [](Block block) { return block == Block::Grass || block == Block::Dirt
        || block == Block::RootedDirt || block == Block::Moss; };
    const auto air = [](Block block) { return block == Block::Air; };
    const auto volume = [&](glm::vec3 low, glm::vec3 high, auto accepts) {
        const auto first = cell(low + glm::vec3(0.0001f));
        const auto last = cell(high - glm::vec3(0.0001f));
        if (!World::contains(first) || !World::contains(last)) return false;
        for (int y = first.y; y <= last.y; ++y)
            for (int z = first.z; z <= last.z; ++z)
                for (int x = first.x; x <= last.x; ++x)
                    if (!accepts(world.get({x,y,z}))) return false;
        return true;
    };
    for (const auto& asset : plan) {
        const auto p = asset.position;
        if (asset.kind >= HubAssetKind::BroadleafClump && asset.kind <= HubAssetKind::MossGarden) {
            check(volume(p + glm::vec3(-1.35f,-1,-1.35f), p + glm::vec3(1.35f,0,1.35f), soil),
                  "dense gardens have soil below their entire footprint");
            check(volume(p + glm::vec3(-1.35f,0,-1.35f), p + glm::vec3(1.35f,3,1.35f), air),
                  "dense gardens have room for their full height and broad leaves");
            if (!garden) garden = &asset;
            else if (!coveredGarden) coveredGarden = &asset;
        }
        if (asset.kind == HubAssetKind::Boulder) {
            check(volume(p + glm::vec3(-1.8f,-1,-1.8f), p + glm::vec3(1.8f,0,1.8f), natural)
                  && volume(p + glm::vec3(-1.8f,0,-1.8f), p + glm::vec3(1.8f,3,1.8f), air),
                  "boulder clusters rest on natural ground with clear space above");
            if (!boulder) boulder = &asset;
        }
        if (asset.kind == HubAssetKind::StalactiteCluster) {
            check(volume(p + glm::vec3(-1.6f,0,-1.6f), p + glm::vec3(1.6f,1,1.6f), natural)
                  && volume(p + glm::vec3(-1.6f,-4,-1.6f), p + glm::vec3(1.6f,0,1.6f), air),
                  "stalactite clusters attach across a solid ceiling and hang into clear air");
            if (!stalactite) stalactite = &asset;
        }
        if (asset.kind == HubAssetKind::RootArch) {
            ++archCount;
            if (asset.facing >= 0 && asset.facing < 4) archFacings[asset.facing] = true;
            const auto outward = turn({0,0,1}, asset.facing);
            check(glm::length(p - (HeartTreeCenter + outward * 10.0f)) < 0.0001f,
                  "tree-bed root arches face radially toward their bridge approach");
            const auto bedA = turn({-4.3f,-1,-1.15f}, asset.facing);
            const auto bedB = turn({4.3f,0,1.15f}, asset.facing);
            check(volume(p + glm::min(bedA,bedB), p + glm::max(bedA,bedB), natural),
                  "the whole padded arch bed stays on natural terrain instead of walking routes");
            for (const float side : {-3.3f, 3.3f}) {
                const auto a = turn({side - 0.65f,-1,-0.8f}, asset.facing);
                const auto b = turn({side + 0.65f,0,0.8f}, asset.facing);
                check(volume(p + glm::min(a,b), p + glm::max(a,b), isSolid),
                      "root arch feet and flared toes have solid ground below them");
            }
            checkArchWalkingClearance(world, asset);
            if (!rootArch) rootArch = &asset;
        }
        if (asset.kind >= HubAssetKind::Boulder && asset.kind <= HubAssetKind::RootCurtain) {
            glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
            for (const auto& vertex : buildHubAsset(asset).vertices) {
                low = glm::min(low, vertex.position); high = glm::max(high, vertex.position);
            }
            for (const auto& building : sceneBuildingPlan()) {
                const auto size = hubBuildingFootprint(building);
                const auto bodyLow = glm::vec3(building.origin) + glm::vec3(-1.5f,1,-1.5f);
                const auto bodyHigh = glm::vec3(building.origin)
                    + glm::vec3(size.x + 1.5f,hubBuildingHeight(building) + 5.0f,size.y + 1.5f);
                check(!(low.x < bodyHigh.x && high.x > bodyLow.x
                        && low.y < bodyHigh.y && high.y > bodyLow.y
                        && low.z < bodyHigh.z && high.z > bodyLow.z),
                      "natural rock and root meshes stay outside buildings and their projecting trim");
            }
        }
        if (asset.kind < HubAssetKind::Dormer || asset.kind > HubAssetKind::PorchCanopy) continue;

        // Attachment points are expressed in each model's local frame, so the
        // same support checks cover all four building orientations.
        std::array<glm::vec3, 2> supports{};
        switch (asset.kind) {
        case HubAssetKind::Dormer: supports = {{{-0.9f,-0.1f,0.8f}, {0.9f,-0.1f,0.8f}}}; break;
        case HubAssetKind::BayWindow: supports = {{{-0.5f,0,-0.48f}, {0.5f,0,-0.48f}}}; break;
        case HubAssetKind::Chimney: supports = {{{0,-0.5f,0}, {0,-0.5f,0}}}; break;
        case HubAssetKind::Balcony: supports = {{{-1.3f,-0.5f,-0.49f}, {1.3f,-0.5f,-0.49f}}}; break;
        case HubAssetKind::PorchCanopy: supports = {{{-1.7f,-0.4f,-0.49f}, {1.7f,-0.4f,-0.49f}}}; break;
        default: break;
        }
        for (const auto support : supports)
            check(isSolid(world.get(cell(p + turn(support, asset.facing)))),
                  "architectural fittings meet real sills, corbels, or roof supports");
        if (asset.kind == HubAssetKind::Balcony)
            check(world.get(cell(p + turn({0,0.5f,-0.49f}, asset.facing))) == Block::Air
                  && world.get(cell(p + turn({0,1.5f,-0.49f}, asset.facing))) == Block::Air,
                  "balconies align with upper windows instead of blank wall panels");
        if (asset.kind == HubAssetKind::BayWindow && !bay) bay = &asset;

        const auto mesh = buildHubAsset(asset);
        const auto outward = turn({0,0,1}, asset.facing);
        const float inset = asset.kind == HubAssetKind::Dormer ? 0.65f : 0.07f;
        glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
        for (const auto& vertex : mesh.vertices) {
            low = glm::min(low, vertex.position); high = glm::max(high, vertex.position);
            if (glm::dot(vertex.position - p, outward) > inset)
                check(world.get(cell(vertex.position)) == Block::Air,
                      "projecting architectural faces stay outside the solid facade");
        }
        // A player's entry corridor must remain free even though cosmetic
        // models do not participate in voxel collision or the route flood fill.
        for (const auto& building : sceneBuildingPlan()) {
            const auto door = glm::vec3(hubBuildingDoor(building)) + glm::vec3(0.5f,0,0.5f);
            const auto a = turn({-0.35f,0.05f,-0.35f}, building.facing);
            const auto b = turn({0.35f,1.85f,1.5f}, building.facing);
            const auto doorLow = door + glm::min(a,b), doorHigh = door + glm::max(a,b);
            check(!(low.x < doorHigh.x && high.x > doorLow.x
                    && low.y < doorHigh.y && high.y > doorLow.y
                    && low.z < doorHigh.z && high.z > doorLow.z),
                  "projecting facades leave player headroom through every entrance");
        }
    }
    check(garden && coveredGarden && bay && boulder && stalactite && rootArch,
          "the real hub provides garden, facade, grounded, and hanging edit fixtures");
    check(archCount == 4 && std::all_of(archFacings.begin(), archFacings.end(), [](bool found) { return found; }),
          "the real hub retains one safely planted arch facing each bridge approach");
    const auto mine = [&](const HubAsset* asset, glm::vec3 offset) {
        if (!asset) return;
        const auto support = cell(asset->position + turn(offset, asset->facing));
        check(isSolid(edited.get(support)), "the removal fixture mines an existing attachment");
        edited.set(support, Block::Air);
        removed.push_back(*asset);
    };
    mine(garden, {-1.3f,-0.5f,-1.3f});
    mine(bay, {-0.5f,0,-0.48f});
    mine(boulder, {0,-0.5f,0});
    mine(stalactite, {0,0.5f,0});
    if (rootArch) {
        const auto route = cell(rootArch->position + turn({3.3f,-0.5f,0}, rootArch->facing));
        check(natural(edited.get(route)), "the route-edit fixture starts on a natural arch bed");
        edited.set(route, Block::Path);
        removed.push_back(*rootArch);
    }
    if (coveredGarden) {
        const auto ceiling = cell(coveredGarden->position) + glm::ivec3(0,2,0);
        check(edited.get(ceiling) == Block::Air, "the garden clearance fixture starts in open air");
        edited.set(ceiling, Block::Stone);
        removed.push_back(*coveredGarden);
    }
    const auto changed = planHubAssets(edited);
    for (const auto& asset : removed)
        check(!containsAsset(changed, asset), "mining a support or closing clearance removes its attached model");
}

void checkBlacksmith(const World& world, const std::vector<HubAsset>& plan)
{
    const auto layout=sceneBlacksmithLayout();
    const auto approach=sceneBlacksmithApproach();
    World edited=world;
    std::vector<HubAsset> removed;
    int fixtures=0;
    for (const auto& asset : plan) {
        if (!isBlacksmithAsset(asset.kind)) continue;
        ++fixtures;
        const auto mesh=buildHubAsset(asset);
        check(mesh.indices.size()/3 < 700,"blacksmith fixtures have bounded geometry budgets");
        for (std::size_t triangle=0;triangle<mesh.indices.size();triangle+=3) {
            check(!triangleIntersectsBox(mesh,triangle,approach+glm::vec3(-0.31f,0.01f,-0.31f),
                                         approach+glm::vec3(0.31f,1.81f,0.31f)),
                  "blacksmith models leave the interaction standing position clear");
            const auto door=glm::vec3(hubBuildingDoor(sceneBlacksmithBuilding()))+glm::vec3(0.5f,0,0.5f);
            check(!triangleIntersectsBox(mesh,triangle,door+glm::vec3(-1.25f,0.01f,-4),
                                         door+glm::vec3(1.25f,2.0f,1.5f)),
                  "blacksmith fixtures preserve a broad entrance and central aisle");
        }
        glm::vec3 support=asset.position;
        if (asset.kind==HubAssetKind::ToolRack || asset.kind==HubAssetKind::AnvilSign)
            support.z-=0.15f;
        check(isSolid(world.get(glm::ivec3(glm::floor(support)))),
              "each blacksmith fixture is anchored to a real solid voxel");
        edited.set(glm::ivec3(glm::floor(support)),Block::Air);
        removed.push_back(asset);
        bool luminous=false;
        for (const auto& vertex : mesh.vertices)
            luminous |= vertex.material==static_cast<float>(Block::Lantern);
        check(luminous==(asset.kind==HubAssetKind::Forge),"only forge coals emit light among smith fixtures");
    }
    check(fixtures==5,"the arrival workshop has forge, anvil, tools, workbench and hanging anvil sign");
    const auto changed=planHubAssets(edited);
    for (const auto& asset : removed)
        check(!containsAsset(changed,asset),"removing a smith fixture's anchor removes its model");
    const auto lightPosition=layout.forge+glm::vec3(0,1.55f,0.45f);
    const auto lit=buildHubDressing(world), unlit=buildHubDressing(edited);
    const auto hasForgeLight=[&](const HubDressing& dressing) {
        return std::any_of(dressing.lights.begin(),dressing.lights.end(),[&](const auto& light) {
            return glm::distance(light.position,lightPosition)<0.001f;
        });
    };
    check(hasForgeLight(lit) && !hasForgeLight(unlit),"the warm coal light follows forge placement and support edits");
}
} // namespace

int main()
{
    using namespace voxel;
    int varyingKinds = 0;
    const glm::vec3 anchor{-13.25f, 22.5f, 15.75f};
    for (const auto kind : kinds) {
        varyingKinds += !same(buildHubAsset({kind, anchor, 0, 0}), buildHubAsset({kind, anchor, 0, 1}));
        if (isPathAsset(kind) || kind >= HubAssetKind::Boulder)
            check(!same(buildHubAsset({kind, {}, 0, 0}), buildHubAsset({kind, {}, 0, 1})),
                  "each new model varies its handcrafted shape or painted palette");
        for (const auto variation : {0u, 1u, 42u, std::numeric_limits<std::uint32_t>::max()}) {
            const HubAsset asset{kind, anchor, 0, variation};
            const auto original = buildHubAsset(asset);
            check(!original.indices.empty(), "every asset kind produces visible geometry");
            check(same(original, buildHubAsset(asset)), "an identical variant is deterministic");
            validate(original, anchor - HubAssetMargin, anchor + HubAssetMargin);
            if (isPathAsset(kind)) validatePath(kind, variation);
            if (kind >= HubAssetKind::BroadleafClump && kind <= HubAssetKind::MossGarden)
                validateGarden(kind, variation);
            if (kind == HubAssetKind::RootArch) validateRootArch(variation);
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
        return isPathAsset(asset.kind);
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
    check(std::all_of(present.begin(), present.begin() + static_cast<std::size_t>(HubAssetKind::AnvilSign) + 1, [](bool found) { return found; }),
          "the lush pass retains every existing hub asset family");
    checkHubPlacement(world, plan);
    checkBlacksmith(world, plan);
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
    check(!dressing.lights.empty() && dressing.lights.size() <= 100, "lanterns and lush bioluminescence retain a bounded source budget");
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
