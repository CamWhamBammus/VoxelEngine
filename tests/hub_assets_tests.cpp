#include "hub_assets.h"
#include "asset_collision.h"
#include "equipment.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
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

void validateFenceModules()
{
    for (const auto kind : {HubAssetKind::TimberFence, HubAssetKind::PicketFence,
                            HubAssetKind::IronRailing, HubAssetKind::GardenGate}) {
        for (const auto variant : {0u,1u,42u}) for (int facing=0;facing<4;++facing) {
            const glm::vec3 origin{-4.5f,7,-8.5f};
            const auto mesh=buildHubAsset({kind,origin,facing,variant});
            validate(mesh,origin+glm::vec3(-0.5f,0,-0.5f),origin+glm::vec3(0.5f,1,0.5f));
            const std::array<HubAsset,2> line{{{kind,origin,facing,variant},
                {kind,origin+turn({1,0,0},facing),facing,variant}}};
            AssetCollision collision;
            collision.rebuild(line);
            // Sample across the joining cell face, including either side of
            // the seam: both visual end posts must form a continuous barrier.
            for (const float x : {0.499f,0.5f,0.501f}) {
                const auto hit=collision.raycast(origin+turn({x,0.90f,1},facing),turn({0,0,-1},facing),2);
                check(hit && hit->distance<1.0f,
                      "one-block fence modules join without a gap in all four orientations");
            }
        }
    }
}

void validateFenceCorners()
{
    for (int facing=0;facing<4;++facing) {
        const glm::vec3 origin{3.5f,7,-8.5f};
        validate(buildHubAsset({HubAssetKind::FenceCorner,origin,facing,0}),
                 origin+glm::vec3(-0.5f,0,-0.5f),origin+glm::vec3(0.5f,1,0.5f));
        const std::array<HubAsset,3> corner{{{HubAssetKind::FenceCorner,origin,facing,0},
            {HubAssetKind::TimberFence,origin+turn({1,0,0},facing),facing,0},
            {HubAssetKind::TimberFence,origin+turn({0,0,1},facing),(facing+1)%4,0}}};
        AssetCollision collision;
        collision.rebuild(corner);
        for (const float seam : {0.499f,0.5f,0.501f}) {
            check(collision.raycast(origin+turn({seam,0.70f,-1},facing),turn({0,0,1},facing),2).has_value()
                  && collision.raycast(origin+turn({-1,0.70f,seam},facing),turn({1,0,0},facing),2).has_value(),
                  "both corner arms meet neighbouring straight rails on the shared block face");
        }
        check(!collision.overlaps(origin+glm::vec3(-0.499f,0.1f,-0.499f),
                                  origin+glm::vec3(-0.3f,0.2f,-0.3f)),
              "a corner stays thin rather than filling the surrounding block");
    }
}

void validateStructuralModules()
{
    const glm::vec3 origin{-4.5f,7,-8.5f};
    for (int facing=0;facing<4;++facing) {
        for (const auto kind : {HubAssetKind::TimberPillar,HubAssetKind::TimberBeam,HubAssetKind::DiagonalBrace})
            validate(buildHubAsset({kind,origin,facing,0}),origin+glm::vec3(-0.5f,0,-0.5f),
                     origin+glm::vec3(0.5f,1,0.5f));
        const std::array<HubAsset,2> pillars{{{HubAssetKind::TimberPillar,origin,facing,0},
            {HubAssetKind::TimberPillar,origin+glm::vec3(0,1,0),facing,0}}};
        AssetCollision column;
        column.rebuild(pillars);
        const auto top=column.raycast(origin+glm::vec3(0,3,0),{0,-1,0},4);
        check(top && std::abs(top->point.y-origin.y-2.0f)<0.0001f,
              "two pillar modules stack to exactly two blocks high");
        for (const float seam : {0.999f,1.0f,1.001f})
            check(column.raycast(origin+glm::vec3(0,seam,1),{0,0,-1},2).has_value(),
                  "matching pillar ends leave no gap at their shared horizontal face");
        const std::array<HubAsset,2> beams{{{HubAssetKind::TimberBeam,origin,facing,0},
            {HubAssetKind::TimberBeam,origin+turn({1,0,0},facing),facing,0}}};
        AssetCollision span;
        span.rebuild(beams);
        for (const float seam : {0.499f,0.5f,0.501f}) {
            const auto hit=span.raycast(origin+turn({seam,2,0},facing),{0,-1,0},3);
            check(hit && std::abs(hit->point.y-origin.y-0.25f)<0.0001f,
                  "one-block beams have a continuous flat top across an integer-cell join");
        }
    }
}

void validateWallFittings()
{
    const glm::vec3 origin{-4.5f,7,-8.5f};
    for (int facing=0;facing<4;++facing) {
        const auto a=turn({-1.5f,0,-0.5f},facing), b=turn({1.5f,3,0.5f},facing);
        validate(buildHubAsset({HubAssetKind::StoneDoorway,origin,facing,0}),
                 origin+glm::min(a,b),origin+glm::max(a,b));
        const std::array<HubAsset,1> door{{{HubAssetKind::StoneDoorway,origin,facing,0}}};
        AssetCollision masonry;
        masonry.rebuild(door);
        for (const float x : {-0.499f,0.0f,0.499f}) for (const float y : {0.1f,1.0f,1.999f})
            check(!masonry.raycast(origin+turn({x,y,1},facing),turn({0,0,-1},facing),2),
                  "arched doorway preserves a full one-by-two-block clear opening");
        for (const float x : {-1.4f,0.0f,1.4f})
            check(masonry.raycast(origin+turn({x,2.90f,1},facing),turn({0,0,-1},facing),2).has_value(),
                  "the square doorway header fills the top of a three-block wall cutout");
        validate(buildHubAsset({HubAssetKind::WindowFrame,origin,facing,0}),
                 origin+glm::vec3(-0.5f,0,-0.5f),origin+glm::vec3(0.5f,1,0.5f));
        const std::array<HubAsset,1> window{{{HubAssetKind::WindowFrame,origin,facing,0}}};
        AssetCollision frame;
        frame.rebuild(window);
        check(frame.raycast(origin+turn({0.45f,0.4f,1},facing),turn({0,0,-1},facing),2).has_value()
              && !frame.raycast(origin+turn({0.2f,0.4f,1},facing),turn({0,0,-1},facing),2),
              "a one-block window has solid jambs and genuinely open panes");
    }
}

void validateBarrelFootprint()
{
    const glm::vec3 origin{-4.5f,7,-8.5f};
    for (int facing=0;facing<4;++facing) for (const auto variant : {0u,1u,42u}) {
        const auto mesh=buildHubAsset({HubAssetKind::Barrel,origin,facing,variant});
        validate(mesh,origin+glm::vec3(-0.5f,0,-0.5f),origin+glm::vec3(0.5f,1,0.5f));
        glm::vec3 low(1000),high(-1000);
        for (const auto& vertex : mesh.vertices) {
            low=glm::min(low,vertex.position); high=glm::max(high,vertex.position);
        }
        check(std::abs(high.y-origin.y-1.0f)<0.0001f && low.y==origin.y
              && high.x-low.x<0.95f && high.z-low.z<0.95f,
              "barrel fits one cell with side clearance and a top exactly one block above its base");
        const std::array<HubAsset,2> barrels{{{HubAssetKind::Barrel,origin,facing,variant},
            {HubAssetKind::Barrel,origin+glm::vec3(0,1,0),facing,variant}}};
        AssetCollision collision;
        collision.rebuild(barrels);
        const auto top=collision.raycast(origin+turn({0.14f,3,0},facing),{0,-1,0},4);
        check(top && std::abs(top->point.y-origin.y-2.0f)<0.0001f,
              "stacked barrels keep their real top surface at the next integer block height");
    }
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

void validateLootChest()
{
    const auto original = buildHubAsset({HubAssetKind::LootChest, {}, 0, 0});
    check(original.indices.size() / 3 < 700, "the detailed chest retains a small model triangle budget");
    for (const auto kind : {HubAssetKind::LootChest, HubAssetKind::CrystalChest, HubAssetKind::IronboundChest}) {
        check(isChestAsset(kind) && !chestAssetName(kind).empty(), "each chest style has a readable independent asset name");
        const auto mesh = buildHubAsset({kind, {}, 0, 0});
        validate(mesh, {-.79f,0,-.50f}, {.79f,1.12f,.56f});
        check(same(mesh, buildHubAsset({kind, {}, 0, 17, {{ItemId::Ruby, 99}}})),
              "variation and authored rewards never change a chest's appearance");
        check(original.indices == mesh.indices && original.vertices.size() == mesh.vertices.size(),
              "changing chest contents preserves the standing and collision geometry");
        for (std::size_t i = 0; i < original.vertices.size() && i < mesh.vertices.size(); ++i)
            check(original.vertices[i].position == mesh.vertices[i].position,
                  "chest style changes do not move its surfaces");
        if (kind != HubAssetKind::LootChest) check(!same(original, mesh), "each chest style has a distinct painted palette");
        for (int facing = 0; facing < 4; ++facing) {
            const std::array<HubAsset, 1> placements{{{kind, {}, facing, 0}}};
            AssetCollision collision;
            collision.rebuild(placements);
            const auto top = collision.raycast({0,3,0}, {0,-1,0}, 4);
            check(top && std::abs(top->point.y - 1.10f) < .001f && top->normal.y > .99f,
                  "the chest has a real flat lid surface in every orientation");
            check(collision.overlaps({-.1f,.3f,-.1f}, {.1f,.5f,.1f}),
                  "the closed chest body is solid");
            const auto latch = collision.raycast(turn({0,.65f,2}, facing), turn({0,0,-1}, facing), 3);
            check(latch && latch->distance > 1.40f && latch->distance < 1.50f,
                  "the front latch rotates with the visible model and can be targeted");
            check(std::abs(collision.sweep({-.3f,2,-.3f}, {.3f,3.8f,.3f}, 1, -3) + .90f) < .001f,
                  "a falling player lands on the chest crown without tunneling");
        }
    }
    const auto key = buildHubAsset({HubAssetKind::KeyPickup, {}, 0, 0});
    validate(key, {-.19f,0,-.06f}, {.20f,.79f,.06f});
    check(!key.vertices.empty() && key.indices.size() / 3 < 100,
          "the cave key has a small readable standalone mesh");
    check(same(key, buildHubAsset({HubAssetKind::KeyPickup, {}, 0, 123})),
          "key appearance does not depend on chest variation");
    for (int facing = 0; facing < 4; ++facing) {
        const std::array<HubAsset, 1> placements{{{HubAssetKind::KeyPickup, {}, facing, 0}}};
        AssetCollision collision;
        collision.rebuild(placements);
        check(collision.empty() && !collision.raycast(turn({0,.30f,1}, facing), turn({0,0,-1}, facing), 2),
              "legacy floating keys never obstruct scenery collision");
    }
}

void validateItemPickups()
{
    const HubAsset oldKey{HubAssetKind::KeyPickup, {}, 0, 0};
    check(isItemPickup(oldKey.kind) && itemPickupStack(oldKey) == ItemStack{ItemId::Key, 1}
          && validChestItems(oldKey), "legacy key pickups keep their implicit one-key payload");
    for (const auto& item : itemCatalog()) {
        HubAsset pickup{HubAssetKind::ItemPickup, {}, 0, 0, {}, {item.id, 1}};
        check(isItemPickup(pickup.kind) && !isKeyPickup(pickup.kind)
              && validChestItems(pickup) && itemPickupStack(pickup) == pickup.pickupItem,
              "generic item pickups expose the exact authored stack");
        const auto mesh = buildHubAsset(pickup);
        validate(mesh, {-.35f,0,-.28f}, {.33f,.80f,.27f});
        check(!mesh.indices.empty() && mesh.indices.size() / 3 <= 100,
              "all nineteen item types have small visible ground meshes");
        float lowest = std::numeric_limits<float>::max();
        for (const auto& vertex : mesh.vertices) lowest = std::min(lowest, vertex.position.y);
        check(lowest == 0, "cached local pickup meshes retain a consistent base for the hover transform");
        auto stack = pickup;
        stack.pickupItem.count = 99;
        stack.variation = 123;
        check(same(mesh, buildHubAsset(stack)), "partial pickup quantities and variation never shift the model footprint");
        if (item.id == ItemId::Key) {
            check(same(mesh, buildHubAsset(oldKey)), "the Items-tab key uses the existing brass key mesh");
        } else {
            check(std::any_of(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
                return vertex.color == item.color;
            }), "each ore or gem uses the matching item catalog color");
        }
        for (int facing = 0; facing < 4; ++facing) {
            pickup.facing = facing;
            const std::array<HubAsset, 1> placed{pickup};
            AssetCollision collision;
            collision.rebuild(placed);
            check(collision.empty() && !collision.raycast({0,2,0}, {0,-1,0}, 3)
                  && !collision.overlaps({-.3f,0,-.3f},{.3f,2,.3f})
                  && collision.sweep({-.3f,0,-.3f},{.3f,2,.3f},0,2)==2,
                  "dropped ores, gems and keys are pass-through in every rotation");
            pickup.position = {-13.25f,22.5f,15.75f};
            const auto [minimum,maximum] = pickupVisualBounds(pickup);
            for (const float time : {0.0f,.25f,1.5f,4.0f,15.0f,1000000.0f,std::numeric_limits<float>::quiet_NaN()}) {
                const auto transform = pickupTransform(pickup,time);
                float bottom = std::numeric_limits<float>::max(), top = std::numeric_limits<float>::lowest();
                for (const auto& vertex : mesh.vertices) {
                    const auto p = glm::vec3(transform*glm::vec4(vertex.position,1));
                    bottom=std::min(bottom,p.y);top=std::max(top,p.y);
                    check(glm::all(glm::greaterThanEqual(p,minimum)) && glm::all(glm::lessThanEqual(p,maximum)),
                          "the fixed hover envelope contains every rotated pickup vertex");
                }
                check(bottom-pickup.position.y>.28f && bottom-pickup.position.y<.40f
                      && top-bottom<.45f,"dropped items stay small and visibly clear of the floor");
            }
            check(pickupTransform(pickup,1.5f)==pickupTransform(pickup,1.5f)
                  && pickupTransform(pickup,0)!=pickupTransform(pickup,1.5f),
                  "pickup bob and spin are deterministic and advance over time");
            pickup.position = {};
        }
        const std::array<HubAsset,1> placed{pickup};
        const auto dressing=buildAssetDressing(placed);
        check(std::all_of(dressing.meshes.begin(),dressing.meshes.end(),[](const auto& part){return part.indices.empty();})
              && dressing.lights.empty(),"animated pickups never enter baked chunk meshes or the world-light cache");
    }
    auto translated=oldKey;translated.position={1,0,0};
    check(pickupTransform(oldKey,2)[3].y!=pickupTransform(translated,2)[3].y,
          "neighboring pickups have stable independent bob phases");
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
            const auto block = stairMaterial(world.get(floor));
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
        check(!hasPath(plan, HubAssetKind::PathPaving, upper) && !hasPath(plan, HubAssetKind::PathPaving, lower),
              "integral paving is not covered by a second generated slab model");
        check(!hasPath(plan, HubAssetKind::PathStep, upper, facing)
              && !hasPath(plan, HubAssetKind::PathEdging, upper, facing),
              "integral risers need no duplicate cladding or curb across the next tread");
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
        world.set(upper,stairBlock(Block::Path,facing));
        const auto shaped=planHubAssets(world);
        check(!hasPath(shaped,HubAssetKind::PathPaving,upper)
              && !hasPath(shaped,HubAssetKind::PathStep,upper)
              && !hasPath(shaped,HubAssetKind::PathEdging,upper)
              && !hasPath(shaped,HubAssetKind::PathPaving,lower),
              "real stairs and adjoining cubes use their own surface geometry");
        world.set(upper,Block::Air);
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

void checkArmory(const World& world, const std::vector<HubAsset>& plan)
{
    const auto layout=sceneArmoryLayout();
    const auto approach=sceneArmoryApproach();
    const auto door=glm::vec3(hubBuildingDoor(sceneArmoryBuilding()))+glm::vec3(.5f,0,.5f);
    std::vector<HubAsset> fixtures;
    World edited=world;
    int stands=0,benches=0,signs=0;
    for (const auto& asset : plan) {
        if (!isArmoryAsset(asset.kind)) continue;
        fixtures.push_back(asset);
        stands+=asset.kind==HubAssetKind::ArmorStand;
        benches+=asset.kind==HubAssetKind::ArmorerBench;
        signs+=asset.kind==HubAssetKind::ShieldSign;
        const auto support=glm::ivec3(glm::floor(asset.position
            +(asset.kind==HubAssetKind::ShieldSign ? glm::vec3(0,0,-.15f) : glm::vec3(0,-.01f,0))));
        check(isSolid(world.get(support)),"armory furniture rests on the room floor or attaches to a wall");
        edited.set(support,Block::Air);
        const auto mesh=buildHubAsset(asset);
        check(mesh.indices.size()/3<650,"armor displays and fitting furniture keep a modest triangle count");
        for (std::size_t i=0;i<mesh.indices.size();i+=3) {
            check(!triangleIntersectsBox(mesh,i,approach+glm::vec3(-.31f,.01f,-.31f),
                                        approach+glm::vec3(.31f,1.81f,.31f)),
                  "armory models leave the fitting bench approach clear");
            check(!triangleIntersectsBox(mesh,i,door+glm::vec3(-1.1f,.01f,-5),
                                        door+glm::vec3(1.1f,1.81f,1.5f)),
                  "armory displays preserve the entry and central aisle");
        }
    }
    check(stands==3 && benches==1 && signs==1,"the armory contains three armor sets, a fitting bench and a shield sign");
    const auto changed=planHubAssets(edited);
    for (const auto& asset : fixtures)
        check(!containsAsset(changed,asset),"removing an armory fixture's support removes its model");
    AssetCollision collision;
    collision.rebuild(fixtures);
    check(!collision.overlaps(approach+glm::vec3(-.3f,.01f,-.3f),approach+glm::vec3(.3f,1.8f,.3f)),
          "the player can stand at the armory without penetrating model collision");
    const auto ray=collision.raycast(approach+glm::vec3(0,1.62f,0),
                                    sceneArmoryStation()-approach-glm::vec3(0,1.62f,0),3);
    check(!ray || ray->distance>1.3f,"armory interaction is not blocked by a display in front of the work surface");
    for (const auto stand : layout.stands)
        check(collision.raycast(stand+glm::vec3(0,1.50f,1),{0,0,-1},2).has_value(),
              "displayed breastplates have real collision geometry");
    for (int tier=1;tier<=GearTierCount;++tier) {
        const auto mesh=buildHubAsset({HubAssetKind::ArmorStand,{},0,std::uint32_t(tier-1)});
        std::array<bool,4> pieces{};
        bool bevel=false;
        for (const auto& vertex : mesh.vertices) {
            if (vertex.material!=static_cast<float>(Block::Slate)) continue;
            const float y=vertex.position.y;
            const int piece=y<.65f ? 0 : y<1.15f ? 1 : y<1.75f ? 2 : 3;
            pieces[piece]=true;
            bevel|=std::abs(vertex.normal.y)>.05f && std::abs(vertex.normal.y)<.95f;
        }
        check(std::all_of(pieces.begin(),pieces.end(),[](bool visible){ return visible; }) && bevel,
              "every ore variant has separate boots, leggings, breastplate and a faceted helmet");
        if (tier>1) check(!same(mesh,buildHubAsset({HubAssetKind::ArmorStand,{},0,std::uint32_t(tier-2)})),
                          "consecutive ore armor displays use distinct material palettes");
    }
}
} // namespace

int main()
{
    using namespace voxel;
    int varyingKinds = 0;
    const glm::vec3 anchor{-13.25f, 22.5f, 15.75f};
    for (const auto kind : kinds) {
        varyingKinds += !same(buildHubAsset({kind, anchor, 0, 0}), buildHubAsset({kind, anchor, 0, 1}));
        if ((isPathAsset(kind) || kind >= HubAssetKind::Boulder) && !isChestAsset(kind) && !isItemPickup(kind))
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
    validateFenceModules();
    validateFenceCorners();
    validateStructuralModules();
    validateWallFittings();
    validateBarrelFootprint();
    validateLootChest();
    validateItemPickups();

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
    check(paths > 50 && paths < 10000, "street borders keep a bounded set of curb models");
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
    for (std::size_t kind=0;kind<=static_cast<std::size_t>(HubAssetKind::AnvilSign);++kind) {
        if (kind==static_cast<std::size_t>(HubAssetKind::PathStep)
            || kind==static_cast<std::size_t>(HubAssetKind::PathPaving)) continue;
        check(present[kind],("the lush pass retains supported hub asset family " + std::to_string(kind)).c_str());
    }
    check(!present[static_cast<std::size_t>(HubAssetKind::PathStep)],
          "the hub uses real stair blocks instead of the old full-block step cladding");
    check(!present[static_cast<std::size_t>(HubAssetKind::PathPaving)],
          "intrinsic block paving replaces automatically overlaid paving assets");
    checkHubPlacement(world, plan);
    checkBlacksmith(world, plan);
    checkArmory(world, plan);
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
