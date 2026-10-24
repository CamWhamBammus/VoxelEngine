#include "graphics/block_relief.h"
#include "block_shapes.h"
#include "ores.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace {
int failures = 0;
constexpr std::size_t centerChunk = voxel::World::Radius * (voxel::World::Width + 1);
constexpr std::array<glm::ivec3, 6> faces{{
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
}};

void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void clear(voxel::World& world)
{
    for (int z = voxel::World::Min.z; z < voxel::World::MaxExclusive.z; ++z) {
        for (int y = voxel::World::Min.y; y < voxel::World::MaxExclusive.y; ++y) {
            for (int x = voxel::World::Min.x; x < voxel::World::MaxExclusive.x; ++x) {
                world.set({x, y, z}, voxel::Block::Air);
            }
        }
    }
}

bool sameVertex(const voxel::MeshVertex& a, const voxel::MeshVertex& b)
{
    return a.position == b.position && a.normal == b.normal && a.color == b.color
        && a.material == b.material && a.relief == b.relief;
}

bool same(const voxel::MeshData& a, const voxel::MeshData& b)
{
    return a.indices == b.indices && a.vertices.size() == b.vertices.size()
        && std::equal(a.vertices.begin(), a.vertices.end(), b.vertices.begin(), sameVertex);
}

void cover(voxel::World& world, glm::ivec3 anchor)
{
    for (const auto face : faces) world.set(anchor + face, voxel::Block::Dirt);
}

// Reference the shader's shape field independently of the polygon clipping.
float paintHash(glm::vec2 p)
{
    const glm::ivec2 cell(p);
    std::uint32_t value = static_cast<std::uint32_t>(cell.x) * 0x9e3779b9u
                        ^ static_cast<std::uint32_t>(cell.y) * 0x85ebca6bu;
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / 16777216.0f;
}

float paintHash3(glm::ivec3 cell, voxel::Block material)
{
    std::uint32_t value = static_cast<std::uint32_t>(cell.x) * 0x9e3779b9u
        ^ static_cast<std::uint32_t>(cell.y) * 0x85ebca6bu
        ^ static_cast<std::uint32_t>(cell.z) * 0xc2b2ae35u
        ^ static_cast<std::uint32_t>(material) * 0x27d4eb2du;
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / 16777216.0f;
}

glm::vec3 paintedMineralCoordinates(glm::vec3 position, voxel::Block material)
{
    if (material == voxel::Block::Crystal || material == voxel::Block::AmberCrystal) {
        return {glm::dot(position, glm::vec3(.8f, 0, .6f)) * 1.55f,
            glm::dot(position, glm::vec3(.12f, .98f, -.16f)) * .72f,
            glm::dot(position, glm::vec3(-.588f, .2f, .784f)) * 1.55f};
    }
    constexpr std::array<glm::vec2, 13> aspects{{
        {.90f, .75f}, {1.10f, .85f}, {.85f, 1.20f}, {1.12f, 1.50f},
        {1.12f, 1.80f}, {.88f, 1.60f}, {.80f, 1.80f}, {.92f, 1.60f},
        {1.20f, 1.40f}, {1.22f, .72f}, {1.32f, .70f}, {1.30f, .75f}, {1.35f, .90f}
    }};
    const int id = static_cast<int>(material);
    const auto aspect = id >= 65 && id <= 77 ? aspects[id - 65] : glm::vec2(1);
    return {glm::dot(position, glm::vec3(.8f, .36f, .48f)) * (1.6f / aspect.x),
        glm::dot(position, glm::vec3(-.6f, .48f, .64f)) * (2.4f / aspect.y),
        glm::dot(position, glm::vec3(0, -.8f, .6f)) * 1.8f};
}

bool paintedOre(glm::vec3 position, voxel::Block material)
{
    const auto field = paintedMineralCoordinates(position, material);
    const glm::ivec3 tile(glm::floor(field));
    const auto q = glm::abs(field - glm::vec3(tile) - .5f);
    const auto id = static_cast<int>(material);
    const bool gem = id >= 68 && id <= 73;
    const glm::vec3 halfSize(.35f + .06f * paintHash3(tile + glm::ivec3(11), material),
        (gem ? .34f : .26f) + .06f * paintHash3(tile + glm::ivec3(23), material),
        .35f + .06f * paintHash3(tile + glm::ivec3(37), material));
    const auto shape = q / halfSize;
    return paintHash3(tile, material) > .18f && glm::all(glm::lessThanEqual(shape, glm::vec3(1.0001f)))
        && shape.x + shape.y + shape.z <= (gem ? 2.0001f : 1.8001f);
}

glm::ivec2 paintedStone(glm::vec2 uv, float seed, bool slate)
{
    const auto p = uv * glm::vec2(1.35f, slate ? 3.2f : 1.65f);
    const glm::ivec2 tile(glm::floor(p));
    float nearest = 100;
    glm::ivec2 result{};
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            const auto cell = tile + glm::ivec2(x, y);
            const float h = paintHash(glm::vec2(cell) + seed);
            const auto delta = p - (glm::vec2(cell) + 0.18f + 0.64f * glm::vec2(h, glm::fract(h * 17.73f)));
            const float distance = glm::dot(delta, delta);
            if (distance < nearest) { nearest = distance; result = cell; }
        }
    }
    return result;
}

void validate(const voxel::MeshData& mesh, glm::ivec3 anchor, glm::ivec3 face,
              voxel::Block material, float minimumPeak, float maximumPeak)
{
    const bool mineral = voxel::oreInfo(material) != nullptr;
    check(!mesh.indices.empty() && mesh.indices.size() % 3 == 0 && mesh.indices.size() / 3 <= (mineral ? 108 : 39),
          "one decorated face has a bounded, nonempty triangle mesh");
    const glm::vec3 normal(face);
    const auto faceCenter = glm::vec3(anchor) + glm::vec3(0.5f) + normal * 0.5f;
    const glm::vec3 u = face.x != 0 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
    const glm::vec3 v = face.y != 0 ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const auto uv = [&](glm::vec3 p) { return glm::vec2(glm::dot(p, u), glm::dot(p, v)); };
    const float paintSeed = glm::dot(glm::vec3(anchor) * glm::abs(normal), glm::vec3(17, 37, 57))
        + static_cast<float>(material) * 19.0f;
    bool embedded = false;
    float peak = 0;
    std::vector<glm::ivec3> oreTiles;
    for (const auto& vertex : mesh.vertices) {
        check(vertex.material == static_cast<float>(material), "relief keeps its anchor's material ID");
        check(std::abs(glm::length(vertex.normal) - 1.0f) < 0.0001f, "relief normals are unit length");
        check(vertex.color == voxel::blockColor(material), "relief preserves the base color used by the painted material");
        const float depth = glm::dot(vertex.position - faceCenter, normal);
        check(glm::vec3(vertex.relief) == normal && std::abs(vertex.relief.w - depth) < 0.00001f,
              "relief metadata stores the anchor face normal and signed height");
        const auto projected = vertex.position - glm::vec3(vertex.relief) * vertex.relief.w;
        check(std::abs(glm::dot(projected - faceCenter, normal)) < 0.00001f,
              "projected texture positions lie on the original block face");
        if (mineral) {
            check(paintedOre(projected, material), "raised ore vertices lie inside painted mineral veins");
            const glm::ivec3 tile(glm::floor(paintedMineralCoordinates(projected, material)));
            if (std::find(oreTiles.begin(), oreTiles.end(), tile) == oreTiles.end()) oreTiles.push_back(tile);
        }
        check(depth >= -0.0041f && depth <= maximumPeak + 0.00001f,
              "relief stays embedded and below its material's maximum height");
        peak = std::max(peak, depth);
        embedded |= depth < -0.003f;
        for (int axis = 0; axis < 3; ++axis) {
            check(std::isfinite(vertex.position[axis]) && std::isfinite(vertex.normal[axis])
                  && std::isfinite(vertex.color[axis]), "relief attributes are finite");
            check(vertex.color[axis] >= 0 && vertex.color[axis] <= 1, "relief albedo stays in range");
            if (face[axis] == 0) {
                check(vertex.position[axis] > anchor[axis] && vertex.position[axis] < anchor[axis] + 1,
                      "plates never cross the tangential block edges");
            }
        }
        const glm::ivec3 cell(glm::floor(vertex.position));
        check(cell == anchor || cell == anchor + face, "every vertex stays in the anchor or its adjacent air cell");
    }
    check(embedded && peak >= minimumPeak, "each face has real raised depth and an anchored skirt");
    check(oreTiles.size() <= 3, "each ore face raises at most three painted fragments");
    float capArea = 0;
    bool tapered = false, flushEnd = false, buriedEdge = false;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size()) {
            check(false, "relief indices reference valid vertices");
            continue;
        }
        const auto cross = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                     mesh.vertices[c].position - mesh.vertices[a].position);
        check(glm::length(cross) > 0.000001f, "all relief triangles have nonzero area");
        check(glm::dot(glm::normalize(cross), mesh.vertices[a].normal) > 0.9999f,
              "counterclockwise triangle winding matches the flat facet normal");
        check(glm::dot(mesh.vertices[a].normal, normal) >= -0.0001f,
              "caps and bevels face outward, while skirts face sideways");
        const float low = std::min({mesh.vertices[a].relief.w, mesh.vertices[b].relief.w, mesh.vertices[c].relief.w});
        const float high = std::max({mesh.vertices[a].relief.w, mesh.vertices[b].relief.w, mesh.vertices[c].relief.w});
        const bool cap = low > -0.003f;
        if (cap) {
            capArea += glm::dot(cross, normal) * 0.5f;
            tapered |= high - low > minimumPeak * 0.4f;
            flushEnd |= low < 0.0011f;
            const int buried = (mesh.vertices[a].relief.w < -0.002f)
                + (mesh.vertices[b].relief.w < -0.002f) + (mesh.vertices[c].relief.w < -0.002f);
            buriedEdge |= buried >= 2;
        }
        if (!mineral && cap) {
            const auto site = paintedStone(uv(mesh.vertices[a].position), paintSeed, material == voxel::Block::Slate);
            check(site == paintedStone(uv(mesh.vertices[b].position), paintSeed, material == voxel::Block::Slate)
                  && site == paintedStone(uv(mesh.vertices[c].position), paintSeed, material == voxel::Block::Slate),
                  "raised stone caps stay within a single painted Voronoi plate");
        }
    }
    check(capArea > 0.003f && capArea < (mineral ? .70f : .20f),
          "raised geometry occupies narrow fragments rather than complete slabs");
    check(tapered && flushEnd, "raised caps taper from a shallow ridge to the underlying face");
    if (mineral) check(buriedEdge, "ore hides a complete cap edge below the host surface");
}

// Measure cap coverage above a real world-space height, not a triangle count.
float raisedArea(const voxel::MeshData& mesh, glm::vec3 normal, float height)
{
    float result = 0;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::vector<glm::vec4> polygon;
        for (int j = 0; j < 3; ++j) {
            const auto& vertex = mesh.vertices[mesh.indices[i + j]];
            if (vertex.relief.w < -0.003f) break; // Ignore the anchored sidewalls.
            polygon.emplace_back(vertex.position, vertex.relief.w);
        }
        if (polygon.size() != 3) continue;
        std::vector<glm::vec4> clipped;
        auto previous = polygon.back();
        for (const auto point : polygon) {
            if ((point.w >= height) != (previous.w >= height)) {
                clipped.push_back(previous + (point - previous) * ((height - previous.w) / (point.w - previous.w)));
            }
            if (point.w >= height) clipped.push_back(point);
            previous = point;
        }
        for (std::size_t j = 1; j + 1 < clipped.size(); ++j) {
            result += glm::dot(glm::cross(glm::vec3(clipped[j] - clipped[0]),
                                          glm::vec3(clipped[j + 1] - clipped[0])), normal) * 0.5f;
        }
    }
    return result;
}

void stoneSurface(voxel::World& world, voxel::Block material, glm::ivec3 normal)
{
    using namespace voxel;
    clear(world);
    const glm::ivec3 u = normal.x != 0 ? glm::ivec3(0, 0, 1) : glm::ivec3(1, 0, 0);
    const glm::ivec3 v = normal.y != 0 ? glm::ivec3(0, 0, 1) : glm::ivec3(0, 1, 0);
    const auto base = glm::abs(normal) * 7;
    for (int b = -1; b <= 16; ++b) {
        for (int a = -1; a <= 16; ++a) {
            const auto cell = base + u * a + v * b;
            world.set(cell - normal, Block::Dirt);
            world.set(cell, a >= 0 && a < 16 && b >= 0 && b < 16 ? material : Block::Dirt);
        }
    }
    const auto mesh = buildBlockRelief(world, centerChunk);
    std::array<MeshData, 256> byFace;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto& first = mesh.vertices[mesh.indices[i]];
        const int a = static_cast<int>(std::floor(glm::dot(first.position, glm::vec3(u))));
        const int b = static_cast<int>(std::floor(glm::dot(first.position, glm::vec3(v))));
        check(a >= 0 && a < 16 && b >= 0 && b < 16, "surface relief stays inside the owning chunk");
        if (a < 0 || a >= 16 || b < 0 || b >= 16) continue;
        auto& face = byFace[a + 16 * b];
        for (int corner = 0; corner < 3; ++corner) {
            const auto& vertex = mesh.vertices[mesh.indices[i + corner]];
            check(std::floor(glm::dot(vertex.position, glm::vec3(u))) == a
                  && std::floor(glm::dot(vertex.position, glm::vec3(v))) == b,
                  "triangles do not cross face boundaries");
            face.indices.push_back(static_cast<std::uint32_t>(face.vertices.size()));
            face.vertices.push_back(vertex);
        }
    }
    int decorated = 0;
    std::array<bool, 4> quadrants{};
    for (int b = 0; b < 16; ++b) {
        for (int a = 0; a < 16; ++a) {
            const auto& face = byFace[a + 16 * b];
            if (face.indices.empty()) continue;
            ++decorated;
            quadrants[(a >= 8) + 2 * (b >= 8)] = true;
            // Fractures ending near block boundaries can taper below their
            // nominal 0.012 minimum peak, but must still have visible depth.
            validate(face, base + u * a + v * b, normal, material, 0.004f, 0.030f);
        }
    }
    if (normal.y > 0) {
        check(decorated >= 10 && decorated <= 38, "upward stone faces have sparse fracture strips near eight percent");
    } else {
        check(decorated >= 30 && decorated <= 74, "walls and ceilings have sparse fracture strips near twenty percent");
    }
    check(std::all_of(quadrants.begin(), quadrants.end(), [](bool value) { return value; }),
          "stone density reaches every chunk quadrant without truncation");
}

void shapeSurfaces()
{
    using namespace voxel;
    const auto contains = [](Block block, glm::vec3 point) {
        const auto shape = blockShape(block);
        for (int i = 0; i < shape.count; ++i) {
            const auto& box = shape.boxes[i];
            if (point.x >= box.minimum.x && point.x <= box.maximum.x
                && point.y >= box.minimum.y && point.y <= box.maximum.y
                && point.z >= box.minimum.z && point.z <= box.maximum.z) return true;
        }
        return false;
    };
    for (const auto block : {Block::Air, Block::Water, Block::Stone, Block::StoneStairs,
                             Block::StoneStairsWest, Block::StoneStairsSouth, Block::StoneStairsEast}) {
        const auto surfaces = blockSurfaces(block);
        check(surfaces.count == (isStair(block) ? 10 : isSolid(block) ? 6 : 0),
              "shape boundary contains only its bounded set of exterior rectangles");
        float area = 0;
        for (int i = 0; i < surfaces.count; ++i) {
            const auto& surface = surfaces.faces[i];
            const auto size = surface.maximum - surface.minimum;
            area += size.x * size.y + size.x * size.z + size.y * size.z;
            const auto middle = (surface.minimum + surface.maximum) * .5f;
            const auto offset = glm::vec3(surface.normal) * .01f;
            check(contains(block, middle - offset) && !contains(block, middle + offset),
                  "every extracted face separates solid stair material from empty space");
            for (int j = 0; j < i; ++j) {
                const auto& other = surfaces.faces[j];
                const auto overlap = glm::min(surface.maximum, other.maximum)
                    - glm::max(surface.minimum, other.minimum);
                const bool samePlane = surface.normal == other.normal
                    && glm::dot(middle - other.minimum, glm::vec3(surface.normal)) == 0;
                check(!samePlane || !((surface.normal.x != 0 ? overlap.z > 0 : overlap.x > 0)
                       && (surface.normal.y != 0 ? overlap.z > 0 : overlap.y > 0)),
                      "coplanar shape rectangles never overlap");
            }
        }
        check(std::abs(area - (isStair(block) ? 5.5f : isSolid(block) ? 6.0f : 0.0f)) < .0001f,
              "shape boundary covers the entire exterior with no internal box faces");
    }
}

void stairRelief(voxel::World& world)
{
    using namespace voxel;
    for (const auto material : {Block::Stone, Block::Limestone, Block::Slate, Block::MossStone}) {
        for (int facing = 0; facing < 4; ++facing) {
            clear(world);
            const auto block = stairBlock(material, facing);
            for (int z = 0; z < 16; z += 2) {
                for (int x = 0; x < 16; x += 2) world.set({x, 6, z}, block);
            }
            const auto mesh = buildBlockRelief(world, centerChunk);
            check(!mesh.indices.empty() && mesh.indices.size() / 3 <= 64 * 10 * 39,
                  "each rocky stair orientation has bounded real surface geometry");
            check(same(mesh, buildBlockRelief(world, centerChunk)), "stair surface detail is deterministic");
            bool lowerTread = false, upperTread = false, riser = false, side = false, raised = false, embedded = false;
            std::array<unsigned, 256> stairCoverage{};
            for (const auto& vertex : mesh.vertices) {
                const glm::vec3 normal(vertex.relief);
                const auto projected = vertex.position - normal * vertex.relief.w;
                const glm::ivec3 cell(glm::floor(projected - normal * .001f));
                const auto local = projected - glm::vec3(cell);
                check(world.get(cell) == block, "stair relief remains attached to its stair cell");
                bool supported = false;
                const auto surfaces = blockSurfaces(block);
                for (int i = 0; i < surfaces.count; ++i) {
                    const auto& surface = surfaces.faces[i];
                    if (glm::vec3(surface.normal) != normal) continue;
                    bool inside = true;
                    for (int axis = 0; axis < 3; ++axis) {
                        inside &= normal[axis] != 0 ? std::abs(local[axis] - surface.minimum[axis]) < .0001f
                            : local[axis] > surface.minimum[axis] && local[axis] < surface.maximum[axis];
                    }
                    supported |= inside;
                }
                check(supported, "raised chips fit strictly inside the actual tread, riser, or side rectangle");
                check(vertex.material == static_cast<float>(material) && vertex.color == blockColor(material),
                      "stair relief uses the cube material and color instead of the oriented stair ID");
                check(vertex.relief.w >= -.0041f && vertex.relief.w <= .0301f,
                      "stair chips retain the shallow depth of ordinary stone blocks");
                check(std::abs(glm::length(vertex.normal) - 1) < .0001f, "stair facets have valid lighting normals");
                lowerTread |= normal.y == 1 && std::abs(local.y - .5f) < .0001f;
                upperTread |= normal.y == 1 && std::abs(local.y - 1) < .0001f;
                const bool innerRiser = normal.y == 0 && std::abs(glm::dot(local - .5f, normal)) < .0001f;
                riser |= innerRiser;
                stairCoverage[cell.x + cell.z * 16] |= (normal.y == 1 && std::abs(local.y - .5f) < .0001f ? 1 : 0)
                    | (normal.y == 1 && std::abs(local.y - 1) < .0001f ? 2 : 0) | (innerRiser ? 4 : 0);
                side |= normal.y == 0 && !innerRiser;
                raised |= vertex.relief.w > .004f;
                embedded |= vertex.relief.w < -.003f;
            }
            check(lowerTread && upperTread && riser && side,
                  "rocky stair material reaches both treads, the inner riser, and exterior sides");
            check(raised && embedded, "stair chips rise from and sink into their supporting faces");
            for (int z = 0; z < 16; z += 2) {
                for (int x = 0; x < 16; x += 2) check(stairCoverage[x + z * 16] == 7,
                    "every individual rocky stair has relief on both treads and its inner riser");
            }
            for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const auto& a = mesh.vertices[mesh.indices[i]];
                const auto& b = mesh.vertices[mesh.indices[i + 1]];
                const auto& c = mesh.vertices[mesh.indices[i + 2]];
                const auto cross = glm::cross(b.position - a.position, c.position - a.position);
                check(glm::length(cross) > .000001f && glm::dot(glm::normalize(cross), a.normal) > .9999f,
                      "stair relief triangle normals match their outward winding");
                if (std::min({a.relief.w, b.relief.w, c.relief.w}) < -.003f) continue;
                const glm::vec3 normal(a.relief);
                const auto projected = a.position - normal * a.relief.w;
                const glm::vec3 cell(glm::floor(projected - normal * .001f));
                const float seed = glm::dot(cell * glm::abs(normal), glm::vec3(17, 37, 57))
                    + static_cast<float>(material) * 19.0f;
                const glm::vec3 u = normal.x != 0 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
                const glm::vec3 v = normal.y != 0 ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
                const auto plate = [&](const MeshVertex& point) {
                    return paintedStone({glm::dot(point.position, u), glm::dot(point.position, v)}, seed, material == Block::Slate);
                };
                check(plate(a) == plate(b) && plate(a) == plate(c),
                      "stair chip caps stay inside the same painted rock plate as the base material");
            }
        }
    }
}

void stairNeighbors(voxel::World& world)
{
    using namespace voxel;
    clear(world);
    const auto solidAt = [&](glm::vec3 point) {
        const glm::ivec3 cell(glm::floor(point));
        const auto local = point - glm::vec3(cell);
        const auto shape = blockShape(world.get(cell));
        for (int i = 0; i < shape.count; ++i) {
            const auto& box = shape.boxes[i];
            if (local.x > box.minimum.x && local.x < box.maximum.x
                && local.y > box.minimum.y && local.y < box.maximum.y
                && local.z > box.minimum.z && local.z < box.maximum.z) return true;
        }
        return false;
    };
    int survivingFaces = 0;
    for (const auto anchor : {glm::ivec3(15, 6, 6), glm::ivec3(0, 8, 10),
                              glm::ivec3(6, 7, 15), glm::ivec3(-1, 6, -1)}) {
        const auto chunkIndex = anchor.x < 0 ? centerChunk - World::Width - 1 : centerChunk;
        for (const auto block : {Block::CopperOre, Block::Crystal, Block::AmberCrystal, Block::StoneStairs, Block::StoneStairsWest,
                                 Block::StoneStairsSouth, Block::StoneStairsEast}) {
            world.set(anchor, block);
            cover(world, anchor);
            for (const auto face : faces) {
                const glm::vec3 normal(face);
                const int axis = face.x != 0 ? 0 : face.y != 0 ? 1 : 2;
                const float plane = anchor[axis] + (face[axis] > 0 ? 1.0f : 0.0f);
                const auto boundaryVertex = [&](const MeshVertex& vertex) {
                    const auto projected = vertex.position - glm::vec3(vertex.relief) * vertex.relief.w;
                    return glm::vec3(vertex.relief) == normal && std::abs(projected[axis] - plane) < .0001f;
                };
                world.set(anchor + face, Block::Air);
                const auto uncovered = buildBlockRelief(world, chunkIndex);
                for (int facing = 0; facing < 4; ++facing) {
                    world.set(anchor + face, stairBlock(Block::Planks, facing));
                    const auto mesh = buildBlockRelief(world, chunkIndex);
                    int visible = 0;
                    for (const auto& vertex : mesh.vertices) {
                        if (!boundaryVertex(vertex)) continue;
                        ++visible;
                        const auto projected = vertex.position - normal * vertex.relief.w;
                        check(solidAt(projected - normal * .002f),
                              "clipped relief remains anchored in solid geometry at stair seams");
                        check(!solidAt(projected + normal * .002f),
                              "chips beside stairs occupy only the neighboring empty half-cube");
                        if (vertex.relief.w > 0) {
                            check(!solidAt(vertex.position), "raised chip vertices never enter neighboring stair collision");
                        }
                    }
                    if (visible > 0) ++survivingFaces;
                    if (face.y > 0) check(visible == 0, "a stair's solid lower slab hides relief directly underneath it");
                    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                        const auto& a = mesh.vertices[mesh.indices[i]];
                        const auto& b = mesh.vertices[mesh.indices[i + 1]];
                        const auto& c = mesh.vertices[mesh.indices[i + 2]];
                        if (!boundaryVertex(a)) continue;
                        const auto center = (a.position + b.position + c.position) / 3.0f;
                        const float depth = (a.relief.w + b.relief.w + c.relief.w) / 3.0f;
                        check(!solidAt(center - normal * depth + normal * .002f),
                              "clipped triangle interiors do not bridge across a neighboring stair's solid area");
                    }
                }
                world.set(anchor + face, Block::Dirt);
                const auto covered = buildBlockRelief(world, chunkIndex);
                check(std::none_of(covered.vertices.begin(), covered.vertices.end(), boundaryVertex),
                      "replacing the neighboring stair with a cube suppresses the full shared boundary");
                world.set(anchor + face, Block::Air);
                check(same(uncovered, buildBlockRelief(world, chunkIndex)),
                      "removing a neighboring stair restores the original surface geometry deterministically");
                world.set(anchor + face, Block::Dirt);
            }
            world.set(anchor, Block::Air);
            for (const auto face : faces) world.set(anchor + face, Block::Air);
        }
    }
    check(survivingFaces > 30, "cube and stair relief remains visible through many partial openings at chunk seams");
}

void crystalRelief(voxel::World& world)
{
    using namespace voxel;
    clear(world);
    for (const auto material : {Block::Crystal, Block::AmberCrystal}) {
        for (const auto anchor : {glm::ivec3(15, 6, 6), glm::ivec3(-1, 6, -1)}) {
            const auto chunk = anchor.x < 0 ? centerChunk - World::Width - 1 : centerChunk;
            world.set(anchor, material);
            cover(world, anchor);
            check(buildBlockRelief(world, chunk).indices.empty(), "covered crystals have no hidden facets");
            for (const auto face : faces) {
                world.set(anchor + face, Block::Air);
                const auto mesh = buildBlockRelief(world, chunk);
                check(!mesh.indices.empty() && mesh.indices.size() / 3 <= 36,
                      "crystal faces contain at most three broad, bounded facets");
                const glm::vec3 normal(face);
                const auto paintedFacet = [&](glm::vec3 position) {
                    return glm::ivec3(glm::floor(paintedMineralCoordinates(position, material)));
                };
                bool raised = false, embedded = false;
                float coverage = 0;
                for (const auto& vertex : mesh.vertices) {
                    check(vertex.material == static_cast<float>(material) && vertex.color == blockColor(material),
                          "crystal facets preserve their mineral color and emissive material");
                    check(glm::vec3(vertex.relief) == normal && vertex.relief.w >= -.0041f && vertex.relief.w <= .0601f,
                          "crystal facets have a buried perimeter and modest raised depth");
                    const auto projected = vertex.position - normal * vertex.relief.w;
                    for (int axis = 0; axis < 3; ++axis) {
                        if (face[axis] == 0) check(projected[axis] > anchor[axis] && projected[axis] < anchor[axis] + 1,
                                                  "crystal facets remain inside tangential block boundaries");
                    }
                    raised |= vertex.relief.w >= .03f;
                    embedded |= vertex.relief.w < -.003f;
                }
                for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
                    const auto& a = mesh.vertices[mesh.indices[i]];
                    const auto& b = mesh.vertices[mesh.indices[i + 1]];
                    const auto& c = mesh.vertices[mesh.indices[i + 2]];
                    const auto cross = glm::cross(b.position - a.position, c.position - a.position);
                    check(glm::length(cross) > .000001f && glm::dot(glm::normalize(cross), a.normal) > .9999f,
                          "crystal facets supply real outward lighting normals");
                    check(paintedFacet(a.position - normal * a.relief.w) == paintedFacet(b.position - normal * b.relief.w)
                          && paintedFacet(a.position - normal * a.relief.w) == paintedFacet(c.position - normal * c.relief.w),
                          "each crystal facet stays inside one independently evaluated painted mineral plane");
                    coverage += glm::dot(cross, normal) * .5f;
                }
                check(raised && embedded && coverage > .25f && coverage < .98f,
                      "broad crystal planes visibly lift the mineral body without separate floating gems");
                check(same(mesh, buildBlockRelief(world, chunk)), "crystal facet geometry is stable across repeated builds");
                world.set(anchor + face, Block::Dirt);
                check(buildBlockRelief(world, chunk).indices.empty(), "covering crystals removes their relief");
            }
            world.set(anchor, Block::Air);
            for (const auto face : faces) world.set(anchor + face, Block::Air);
        }
    }
}

void oreEdgeContinuity(voxel::World& world)
{
    using namespace voxel;
    struct EdgePiece { glm::ivec3 block, deposit, side; };
    for (const auto& ore : oreCatalog()) {
        clear(world);
        // Several corners avoid depending on one lucky deposit. Include
        // neighboring chunk coordinates and an entire negative-coordinate cube.
        for (int z : {2, 6, 10}) for (int x : {2, 6, 10}) world.set({x, 6, z}, ore.block);
        world.set({-1, 6, -1}, ore.block);
        std::vector<EdgePiece> topPieces, sidePieces;
        for (const auto chunk : {centerChunk, centerChunk - World::Width - 1}) {
            const auto mesh = buildBlockRelief(world, chunk);
            for (const auto& vertex : mesh.vertices) {
                const glm::vec3 normal(vertex.relief);
                const auto projected = vertex.position - normal * vertex.relief.w;
                const glm::ivec3 block(glm::floor(projected - normal * .001f));
                const auto local = projected - glm::vec3(block);
                const glm::ivec3 deposit(glm::floor(paintedMineralCoordinates(projected, ore.block)));
                if (normal.y == 1) {
                    for (int axis : {0, 2}) for (int sign : {-1, 1}) {
                        const float boundary = sign > 0 ? 1.0f : 0.0f;
                        if (std::abs(local[axis] - boundary) > .035f) continue;
                        glm::ivec3 side(0); side[axis] = sign;
                        topPieces.push_back({block, deposit, side});
                    }
                } else if (normal.y == 0 && local.y > .965f) {
                    sidePieces.push_back({block, deposit, glm::ivec3(normal)});
                }
            }
        }
        int matchingCorners = 0;
        for (const auto& top : topPieces) {
            matchingCorners += std::any_of(sidePieces.begin(), sidePieces.end(), [&](const EdgePiece& side) {
                return top.block == side.block && top.deposit == side.deposit && top.side == side.side;
            });
        }
        check(matchingCorners > 0,
              "every ore has real geometry from the same deposit continuing around a top/side corner");
    }
}

} // namespace

int main()
{
    using namespace voxel;
    shapeSurfaces();
    World world(42);
    clear(world);
    check(buildBlockRelief(world, centerChunk).indices.empty(), "air has no relief");
    const glm::ivec3 anchor{15, 6, 6};
    // These painted ore fixtures cover bottoms and all negative orientations.
    for (const auto material : {Block::CoalOre, Block::CopperOre, Block::IronOre}) {
        world.set(anchor, material);
        cover(world, anchor);
        check(buildBlockRelief(world, centerChunk).indices.empty(), "fully covered ore has no relief");
        for (const auto face : faces) {
            world.set(anchor + face, Block::Air);
            const auto before = world;
            const auto mesh = buildBlockRelief(world, centerChunk);
            validate(mesh, anchor, face, material, 0.040f, 0.070f);
            check(world.sameContents(before), "building relief leaves voxel contents unchanged");
            check(same(mesh, buildBlockRelief(world, centerChunk)), "relief is deterministic");
            world.set(anchor + face, Block::Dirt);
            check(buildBlockRelief(world, centerChunk).indices.empty(), "covering any face removes its relief");
        }
    }

    // Appended ore IDs include thin metal seams and broader faceted gems.
    // Probe several cells per orientation because some naturally contain no vein.
    for (const auto& ore : oreCatalog()) {
        if (ore.block < Block::TinOre) continue;
        std::array<bool, 6> seen{};
        for (int sample = 0; sample < 4; ++sample) {
            const glm::ivec3 position{15, 6 + sample, 4 + sample};
            world.set(anchor, Block::Air);
            for (const auto face : faces) world.set(anchor + face, Block::Air);
            world.set(position, ore.block);
            cover(world, position);
            check(buildBlockRelief(world, centerChunk).indices.empty(), "new ores never grow hidden geometry");
            for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                const auto face = faces[faceIndex];
                world.set(position + face, Block::Air);
                const auto mesh = buildBlockRelief(world, centerChunk);
                if (!mesh.indices.empty()) {
                    validate(mesh, position, face, ore.block, .001f, .070f);
                    check(same(mesh, buildBlockRelief(world, centerChunk)), "new mineral shapes are deterministic");
                    seen[faceIndex] = true;
                }
                world.set(position + face, Block::Dirt);
            }
            world.set(position, Block::Air);
            for (const auto face : faces) world.set(position + face, Block::Air);
        }
        check(std::all_of(seen.begin(), seen.end(), [](bool face) { return face; }),
              "every new mineral has paint-aligned embedded geometry on all six face orientations");
    }
    cover(world, anchor);

    world.set(anchor, Block::CopperOre);
    world.set(anchor + faces[0], Block::Air); // x=16 belongs to the next chunk.
    const auto seam = buildBlockRelief(world, centerChunk);
    check(!seam.indices.empty() && buildBlockRelief(world, centerChunk + 1).indices.empty(),
          "cross-chunk relief belongs only to its solid anchor's chunk");
    world.set(anchor + faces[0], Block::Brick);
    check(buildBlockRelief(world, centerChunk).indices.empty(), "a neighboring chunk can cover the relief");
    world.set(anchor + faces[0], Block::Air);
    check(same(seam, buildBlockRelief(world, centerChunk)), "reopening the neighboring chunk restores the same geometry");
    world.set(anchor, Block::Air);
    check(buildBlockRelief(world, centerChunk).indices.empty(), "mining an anchor removes its relief");
    for (const auto material : {Block::Grass, Block::Dirt, Block::Sand,
                               Block::Timber, Block::Lantern, Block::Bedrock,
                               Block::Water, Block::Leaves, Block::GlowLeaves}) {
        world.set(anchor, material);
        check(buildBlockRelief(world, centerChunk).indices.empty(), "other materials never grow stone or ore plates");
    }

    World anotherSeed(43);
    clear(anotherSeed);
    anotherSeed.set(anchor, Block::CopperOre);
    cover(anotherSeed, anchor);
    anotherSeed.set(anchor + faces[0], Block::Air);
    check(!same(seam, buildBlockRelief(anotherSeed, centerChunk)), "world seed varies plate geometry and facets");

    // Adding an earlier anchor must not consume a cap or reshuffle later shapes.
    world.set(anchor, Block::CopperOre);
    world.set({1, 1, 1}, Block::IronOre);
    const auto more = buildBlockRelief(world, centerChunk);
    check(more.vertices.size() > seam.vertices.size()
          && std::equal(seam.vertices.begin(), seam.vertices.end(),
                        more.vertices.end() - static_cast<std::ptrdiff_t>(seam.vertices.size()), sameVertex),
          "unrelated earlier blocks do not change existing relief");

    clear(world);
    const glm::ivec3 edge{World::Min.x, 6, 5};
    world.set(edge, Block::IronOre);
    cover(world, edge);
    check(buildBlockRelief(world, World::Radius * World::Width).indices.empty(),
          "world exterior is not an adjacent air cell for relief");

    clear(world);
    const glm::ivec3 negative{-1, 6, -1};
    world.set(negative, Block::IronOre);
    cover(world, negative);
    world.set(negative + faces[0], Block::Air);
    constexpr auto negativeChunk = centerChunk - World::Width - 1;
    validate(buildBlockRelief(world, negativeChunk), negative, faces[0], Block::IronOre, 0.040f, 0.070f);
    world.set(negative + faces[0], Block::Stone);
    check(buildBlockRelief(world, negativeChunk).indices.empty(), "negative-coordinate chunk seams also suppress hidden relief");

    clear(world);
    const glm::ivec3 unpainted{15, 6, 5};
    world.set(unpainted, Block::CopperOre);
    cover(world, unpainted);
    world.set(unpainted + faces[2], Block::Air);
    const auto unpaintedMesh = buildBlockRelief(world, centerChunk);
    for (const auto& vertex : unpaintedMesh.vertices) {
        const auto projected = vertex.position - glm::vec3(vertex.relief) * vertex.relief.w;
        check(paintedOre(projected, Block::CopperOre),
              "ore geometry only appears inside the continuous painted deposit field");
    }

    clear(world);
    const glm::ivec3 visibleOre{0, 6, 15};
    world.set(visibleOre, Block::CopperOre);
    cover(world, visibleOre);
    world.set(visibleOre + faces[4], Block::Air);
    check(raisedArea(buildBlockRelief(world, centerChunk), {0, 0, 1}, 0.020f) > 0.01f,
          "the native ore fixture raises more than one percent of its face by at least 0.020 blocks");

    clear(world);
    constexpr auto upperChunk = centerChunk + World::Width * World::Width;
    for (int z = -1; z <= 16; ++z) {
        for (int x = -1; x <= 16; ++x) {
            world.set({x, 30, z}, Block::Dirt);
            world.set({x, 31, z}, x >= 0 && x < 16 && z >= 0 && z < 16 ? Block::CopperOre : Block::Dirt);
        }
    }
    const auto upperRelief = buildBlockRelief(world, upperChunk);
    check(!upperRelief.indices.empty(), "ore in an upper layer can grow across its top chunk seam");
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) world.set({x, 32, z}, Block::Planks);
    }
    check(buildBlockRelief(world, upperChunk).indices.empty(), "the next vertical chunk suppresses covered relief");
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) world.set({x, 32, z}, Block::Air);
    }
    check(same(upperRelief, buildBlockRelief(world, upperChunk)), "reopening an upper seam restores the same ore relief");

    // Whole surfaces exercise sparse density and endpoint taper on every orientation.
    for (const auto material : {Block::Stone, Block::Limestone, Block::Slate, Block::MossStone, Block::Calcite}) {
        for (const auto face : faces) stoneSurface(world, material, face);
    }

    stairRelief(world);
    stairNeighbors(world);
    crystalRelief(world);
    oreEdgeContinuity(world);

    bool threw = false;
    try { buildBlockRelief(world, World::ChunkCount); }
    catch (const std::out_of_range&) { threw = true; }
    check(threw, "invalid chunk indices are rejected");
    return failures ? 1 : 0;
}
