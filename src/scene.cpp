#include "scene.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace voxel {
namespace {
constexpr int Ground = 8;
constexpr glm::ivec2 Center{8, 8};
using Style = HubBuildingStyle;

// Floor corner, local size, silhouette, facing (+Z, +X, -Z, -X).
// Keep the building plan separate from the landscape so moving a house is easy.
constexpr std::array<HubBuilding, 28> Buildings{{
    {{-22, Ground, 34}, 9, 9, Style::Workshop, 0},
    {{-9, Ground, 37}, 7, 7, Style::Cottage, 0},
    {{22, Ground, 37}, 9, 9, Style::Hall, 0},
    {{35, Ground, 33}, 9, 9, Style::Cottage, 0},
    {{-36, Ground, 24}, 9, 11, Style::Hall, 1},
    {{43, Ground, 21}, 9, 11, Style::Storehouse, 3},
    {{-22, Ground, 49}, 9, 9, Style::Cottage, 2},
    {{-8, Ground, 52}, 7, 7, Style::Cottage, 2},
    {{7, Ground, 50}, 9, 9, Style::Storehouse, 2},
    {{22, Ground, 51}, 7, 7, Style::Cottage, 2},
    {{36, Ground, 45}, 9, 9, Style::Cottage, 2},
    {{-40, Ground, 9}, 9, 9, Style::Workshop, 1},
    {{-43, Ground, -5}, 9, 9, Style::Cottage, 1},
    {{-36, Ground, -19}, 11, 9, Style::Hall, 1},
    {{-25, Ground, -30}, 9, 9, Style::Storehouse, 0},
    {{-12, Ground, -36}, 9, 9, Style::Cottage, 0},
    {{24, Ground, -36}, 11, 9, Style::Hall, 0},
    {{39, Ground, -26}, 9, 9, Style::Workshop, 3},
    {{43, Ground, -9}, 9, 9, Style::Cottage, 3},
    {{43, Ground, 6}, 7, 9, Style::Storehouse, 3},
    {{-36, Ground, -39}, 9, 9, Style::Cottage, 0},
    {{-20, Ground, -48}, 7, 7, Style::Cottage, 0},
    {{25, Ground, -48}, 7, 7, Style::Cottage, 0},
    {{39, Ground, -42}, 7, 9, Style::Cottage, 0},
    {{-51, Ground, 9}, 7, 9, Style::Cottage, 1},
    {{-50, Ground, 24}, 7, 7, Style::Cottage, 1},
    {{57, Ground, 6}, 7, 9, Style::Cottage, 3},
    {{56, Ground, 23}, 7, 7, Style::Cottage, 3},
}};

std::uint32_t hash(int x, int z, std::uint32_t seed)
{
    auto value = seed ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u)
                      ^ (static_cast<std::uint32_t>(z) * 0x85ebca6bu);
    value ^= value >> 16;
    value *= 0x7feb352du;
    return value ^ (value >> 15);
}

void box(World& world, glm::ivec3 from, glm::ivec3 to, Block block)
{
    for (int y = from.y; y <= to.y; ++y)
        for (int z = from.z; z <= to.z; ++z)
            for (int x = from.x; x <= to.x; ++x) world.set({x, y, z}, block);
}

float ellipse(int x, int z, glm::vec2 center, glm::vec2 radius)
{
    const auto offset = (glm::vec2(x, z) - center) / radius;
    return glm::dot(offset, offset);
}

bool lake(int x, int z)
{
    const float island = ellipse(x, z, Center, {10, 10});
    return (ellipse(x, z, Center, {26, 23}) < 1.0f && island > 1.0f)
        || ellipse(x, z, {-24, -13}, {15, 8}) < 1.0f
        || ellipse(x, z, {40, -12}, {9, 8}) < 1.0f;
}

void landscape(World& world)
{
    for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
        for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
            const float distance = ellipse(x, z, Center, {63, 61});
            const auto variation = hash(x / 5, z / 5, world.seed());
            const int ceiling = std::min(59, 44 + int(12 * std::max(0.0f, 1.0f - distance)) + int(variation % 3));
            const bool chamber = distance < 1.0f + float(variation % 5) * 0.006f;
            const bool water = chamber && lake(x, z);
            for (int y = 0; y < World::MaxExclusive.y; ++y) {
                const bool shell = y == 0 || y == World::MaxExclusive.y - 1
                    || x == World::Min.x || z == World::Min.z
                    || x == World::MaxExclusive.x - 1 || z == World::MaxExclusive.z - 1;
                Block block = y < 6 || (y / 7 + int(variation % 3)) % 4 == 0 ? Block::Slate : Block::Stone;
                if ((y / 9 + int(variation % 2)) % 3 == 0) block = Block::Limestone;
                if (chamber) {
                    if (y > Ground && y <= ceiling) block = Block::Air;
                    if (y == Ground) block = hash(x / 3, z / 3, world.seed()) % 6 == 0 ? Block::Dirt : Block::Grass;
                    if (y == Ground - 1) block = Block::Dirt;
                    if (water && y >= 5 && y <= Ground) {
                        block = y == 5 ? Block::Sand : y == 6 ? Block::Water : Block::Air;
                    }
                }
                world.set({x, y, z}, shell ? Block::Bedrock : block);
            }
        }
    }
    // Two one-block shore steps let players climb back out of the shallow basin.
    for (int z = -24; z <= 38; ++z) {
        for (int x = -40; x <= 50; ++x) {
            if (!lake(x, z)) continue;
            int shoreDistance = 3;
            for (const auto side : {glm::ivec2(1, 0), {-1, 0}, {0, 1}, {0, -1}})
                for (int distance = 1; distance <= 2; ++distance)
                    if (!lake(x + side.x * distance, z + side.y * distance))
                        shoreDistance = std::min(shoreDistance, distance);
            if (shoreDistance <= 2) world.set({x, 6, z}, Block::Limestone);
            if (shoreDistance == 1) world.set({x, 7, z}, Block::Limestone);
        }
    }
}

void path(World& world, glm::ivec2 from, glm::ivec2 to, int radius = 2, Block block = Block::Limestone)
{
    const int steps = std::max(std::abs(to.x - from.x), std::abs(to.y - from.y));
    for (int step = 0; step <= steps; ++step) {
        const auto point = glm::ivec2(glm::round(glm::mix(glm::vec2(from), glm::vec2(to), steps ? float(step) / steps : 0.0f)));
        for (int z = point.y - radius; z <= point.y + radius; ++z) {
            for (int x = point.x - radius; x <= point.x + radius; ++x) {
                world.set({x, Ground, z}, block);
                box(world, {x, Ground + 1, z}, {x, Ground + 4, z}, Block::Air);
            }
        }
    }
}

void lamp(World& world, int x, int z, int floor = Ground)
{
    if (!isSolid(world.get({x, floor, z}))) return;
    box(world, {x, floor + 1, z}, {x, floor + 2, z}, Block::Timber);
    world.set({x, floor + 3, z}, Block::Lantern);
    world.set({x, floor + 4, z}, Block::RoofTiles);
}

void bridge(World& world, glm::ivec2 from, glm::ivec2 to)
{
    path(world, from, to, 3, Block::Planks);
    const bool northSouth = from.x == to.x;
    const int begin = northSouth ? std::min(from.y, to.y) : std::min(from.x, to.x);
    const int end = northSouth ? std::max(from.y, to.y) : std::max(from.x, to.x);
    for (int position = begin; position <= end; ++position) {
        for (const int side : {-4, 4}) {
            const glm::ivec2 p = northSouth ? glm::ivec2(from.x + side, position) : glm::ivec2(position, from.y + side);
            world.set({p.x, Ground, p.y}, Block::Timber);
            world.set({p.x, Ground + 1, p.y}, Block::Timber);
            if ((position - begin) % 6 == 0 || position == end) {
                box(world, {p.x, 5, p.y}, {p.x, Ground + 2, p.y}, Block::Timber);
                if ((position - begin) % 12 == 0 || position == end) world.set({p.x, Ground + 3, p.y}, Block::Lantern);
            }
        }
    }
}

void sphere(World& world, glm::vec3 center, glm::vec3 radii, Block block, bool onlyAir = false)
{
    const auto low = glm::ivec3(glm::floor(center - radii));
    const auto high = glm::ivec3(glm::ceil(center + radii));
    for (int y = low.y; y <= high.y; ++y) {
        for (int z = low.z; z <= high.z; ++z) {
            for (int x = low.x; x <= high.x; ++x) {
                const auto offset = (glm::vec3(x, y, z) - center) / radii;
                if (glm::dot(offset, offset) <= 1.0f && (!onlyAir || world.get({x, y, z}) == Block::Air))
                    world.set({x, y, z}, block);
            }
        }
    }
}

void branch(World& world, glm::vec3 from, glm::vec3 to, float startWidth, float endWidth)
{
    const int steps = std::max(1, int(glm::distance(from, to) * 2));
    for (int i = 0; i <= steps; ++i) {
        const float amount = float(i) / steps;
        sphere(world, glm::mix(from, to, amount), glm::vec3(glm::mix(startWidth, endWidth, amount)), Block::Timber);
    }
}

void heartTree(World& world)
{
    // Roots leave the four cardinal bridge approaches open.
    for (const auto root : {glm::vec3(-1, 9, 1), {18, 9, 0}, {-1, 9, 17}, {18, 9, 18}})
        branch(world, {8, 12, 8}, root, 2.4f, 0.8f);
    branch(world, {8, 10, 8}, {7, 23, 8}, 3.8f, 2.6f);
    branch(world, {7, 23, 8}, {10, 34, 9}, 2.6f, 1.4f);
    for (const auto tip : {glm::vec3(-7, 34, 2), {24, 35, 13}, {5, 37, -9}, {6, 36, 25}}) {
        const glm::vec3 elbow = glm::mix(glm::vec3(8, 26, 8), tip, 0.58f) - glm::vec3(0, 2, 0);
        branch(world, {8, 24, 8}, elbow, 2.0f, 1.4f);
        branch(world, elbow, tip, 1.4f, 0.7f);
    }
    // Smaller lobes at different heights reveal the branches between the crowns.
    struct Crown { glm::vec3 center, radius; };
    for (const auto& crown : std::array<Crown, 12>{{
        {{9, 42, 8}, {8, 7, 7}}, {{-6, 36, 2}, {8, 5, 7}},
        {{-12, 38, 4}, {6, 4, 5}}, {{22, 38, 13}, {8, 5, 7}},
        {{28, 40, 12}, {5, 4, 5}}, {{4, 39, -7}, {7, 6, 8}},
        {{2, 42, -13}, {5, 5, 5}}, {{7, 39, 24}, {8, 6, 8}},
        {{10, 41, 30}, {5, 4, 5}}, {{-3, 41, 18}, {7, 5, 6}},
        {{18, 44, 1}, {7, 5, 7}}, {{2, 45, 6}, {6, 4, 6}},
    }}) {
        sphere(world, crown.center, crown.radius, Block::Leaves, true);
    }
    // Luminous accents occupy exposed crown edges, with hanging living roots.
    for (int z = -19; z <= 36; ++z) {
        for (int x = -21; x <= 37; ++x) {
            for (int y = 29; y <= 49; ++y) {
                if (world.get({x, y, z}) != Block::Leaves) continue;
                const auto variation = hash(x + y * 7, z, world.seed());
                const bool underside = world.get({x, y - 1, z}) == Block::Air;
                const bool luminousPatch = hash((x + 24) / 3 + y / 3, (z + 24) / 3, world.seed()) % 16 == 0;
                if (luminousPatch && (underside || world.get({x, y + 1, z}) == Block::Air))
                    world.set({x, y, z}, Block::GlowLeaves);
                if (underside && variation % 61 == 0) {
                    const int length = 2 + variation % 6;
                    for (int drop = 1; drop <= length; ++drop) {
                        if (world.get({x, y - drop, z}) != Block::Air) break;
                        world.set({x, y - drop, z}, drop == length ? Block::GlowLeaves : Block::Leaves);
                    }
                }
            }
        }
    }
    for (const glm::ivec3 light : {glm::ivec3(4, 18, 8), {12, 22, 9}, {8, 28, 5}, {5, 33, 17}}) {
        world.set(light, Block::Lantern);
        box(world, light + glm::ivec3(0, 1, 0), light + glm::ivec3(0, 2, 0), Block::Timber);
    }
    // A low ring and four gaps create a deliberate centerpiece plaza.
    for (int z = -1; z <= 17; ++z) {
        for (int x = -1; x <= 17; ++x) {
            const float radius = glm::length(glm::vec2(x - 8, z - 8));
            if (radius >= 7.0f && radius <= 9.2f) world.set({x, Ground, z}, Block::Limestone);
        }
    }
}

void crystalGarden(World& world, glm::ivec3 base, int height)
{
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (std::abs(dx) + std::abs(dz) > 3) continue;
            const int top = base.y + std::max(0, height - std::abs(dx) - std::abs(dz));
            box(world, {base.x + dx, Ground + 1, base.z + dz}, {base.x + dx, top, base.z + dz}, Block::Slate);
            world.set({base.x + dx, top + 1, base.z + dz}, Block::Crystal);
        }
    }
}

void waterfall(World& world, int x, int z, int top)
{
    // Uneven rock shoulders cradle the ribbons and their source pool.
    sphere(world, {x - 4, 16, z - 3}, {6, 12, 6}, Block::Slate, true);
    sphere(world, {x + 4, 19, z - 4}, {5, 14, 5}, Block::Slate, true);
    for (int level = 7; level <= top; ++level) {
        const int shoulder = 5 + (top - level) / 9;
        box(world, {x - shoulder, level, z - 4}, {x + shoulder, level, z - 1},
            level % 11 == 0 ? Block::Limestone : Block::Slate);
        for (int dx = -2; dx <= 2; ++dx)
            if (dx != 2 || level < top - 2) world.set({x + dx, level, z}, Block::Water);
    }
    box(world, {x - 3, top, z - 4}, {x + 3, top, z}, Block::Water);
    for (const int side : {-5, 5}) {
        box(world, {x + side, top - 3, z - 1}, {x + side, top, z + 1}, Block::Leaves);
        world.set({x + side, top - 4, z + 1}, Block::GlowLeaves);
    }
    crystalGarden(world, {x + 7, Ground + 1, z + 1}, 3);
}

void expeditionGateway(World& world)
{
    // An architectural destination only: the open arch leads into a staging hall.
    box(world, {-2, Ground, -56}, {18, Ground, -35}, Block::Limestone);
    box(world, {0, Ground + 1, -56}, {16, 23, -42}, Block::Air);
    for (const int x : {-2, 16}) {
        box(world, {x, Ground + 1, -47}, {x + 2, 25, -43}, Block::Slate);
        box(world, {x - 1, 24, -48}, {x + 3, 26, -42}, Block::Limestone);
        box(world, {x, 27, -47}, {x + 2, 29, -43}, Block::RoofTiles);
        world.set({x + 1, 22, -42}, Block::Lantern);
        world.set({x + 1, 28, -45}, Block::Crystal);
    }
    box(world, {1, 22, -46}, {15, 24, -43}, Block::Limestone);
    box(world, {4, 25, -46}, {12, 26, -43}, Block::Slate);
    box(world, {7, 26, -45}, {9, 28, -44}, Block::Crystal);
    // Chevron floor inlay and twin supply shelters distinguish the departure plaza.
    for (int z = -42; z <= -30; ++z) {
        world.set({8, Ground, z}, Block::RoofTiles);
        if ((z + 42) % 4 == 0) {
            for (int dx = 1; dx <= 3; ++dx) {
                world.set({8 - dx, Ground, z + dx}, Block::RoofTiles);
                world.set({8 + dx, Ground, z + dx}, Block::RoofTiles);
            }
        }
    }
    for (const int x : {0, 15}) {
        box(world, {x, 9, -38}, {x + 1, 10, -36}, Block::Timber);
        lamp(world, x, -33);
    }
    // A recessed, turning passage suggests the journey beyond the hub. It is
    // still ordinary walkable architecture; no loading or travel action runs.
    box(world, {0, 9, -58}, {16, 23, -56}, Block::Slate);
    box(world, {4, Ground, -62}, {12, 18, -53}, Block::Slate);
    box(world, {5, Ground, -62}, {25, 17, -57}, Block::Slate);
    box(world, {5, 9, -61}, {11, 17, -53}, Block::Air);
    box(world, {5, 9, -61}, {24, 16, -58}, Block::Air);
    world.set({13, 13, -61}, Block::Lantern);
    world.set({23, 12, -58}, Block::Lantern);
}

void greenery(World& world)
{
    // Low planted borders sit on grass only, so doors and paved routes stay clear.
    for (int z = -49; z <= 62; z += 3) {
        for (int x = -51; x <= 65; x += 3) {
            const auto variation = hash(x, z, world.seed());
            if (variation % 5 != 0 || world.get({x, Ground, z}) != Block::Grass
                || world.get({x, Ground + 1, z}) != Block::Air) continue;
            sphere(world, {x, Ground + 1, z}, {1.4f, 0.8f, 1.4f}, Block::Leaves, true);
            if (variation % 3 == 0) world.set({x, Ground + 2, z}, Block::GlowLeaves);
        }
    }
    // Small trees frame the waterfront without competing with the heart tree.
    for (const auto p : {glm::ivec2(-17, 25), {32, 27}, {-27, 0}, {36, 2}, {-5, -25}, {22, -25}}) {
        if (world.get({p.x, Ground, p.y}) != Block::Grass) continue;
        box(world, {p.x, 9, p.y}, {p.x, 15, p.y}, Block::Timber);
        sphere(world, {p.x, 16, p.y}, {3, 3, 3}, Block::Leaves, true);
        sphere(world, {p.x + 2, 15, p.y}, {2, 2, 2}, Block::Leaves, true);
        world.set({p.x, 13, p.y + 2}, Block::GlowLeaves);
    }
    // Hanging vegetation breaks up the high cavern silhouette.
    for (const auto p : {glm::ivec2(-34, -20), {48, -18}, {-40, 28}, {43, 43}, {-17, 51}}) {
        for (int dx = -3; dx <= 3; ++dx) {
            int y = 59;
            while (y > 25 && world.get({p.x + dx, y, p.y}) != Block::Air) --y;
            const int length = 4 + hash(p.x + dx, p.y, world.seed()) % 6;
            for (int drop = 0; drop < length; ++drop) {
                if (world.get({p.x + dx, y - drop, p.y}) == Block::Air)
                    world.set({p.x + dx, y - drop, p.y}, drop + 1 == length ? Block::GlowLeaves : Block::Leaves);
            }
        }
    }
}

void geology(World& world)
{
    for (const auto p : {glm::ivec3(-45, 13, -25), {51, 12, -22}, {48, 12, 39},
                         {-42, 16, 42}}) {
        box(world, {p.x - 2, Ground + 1, p.z - 2}, {p.x + 2, p.y, p.z + 2}, Block::Stone);
        sphere(world, p, {4, 6, 4}, Block::Stone, true);
        for (int y = p.y - 4; y <= p.y + 4; ++y) {
            for (int z = p.z - 4; z <= p.z + 4; ++z) {
                for (int x = p.x - 4; x <= p.x + 4; ++x) {
                    if (world.get({x, y, z}) != Block::Stone) continue;
                    const auto variation = hash(x / 2 + y * 3, z / 2, world.seed());
                    if (variation % 4 == 0) world.set({x, y, z}, Block::CopperOre);
                    else if (variation % 7 == 0) world.set({x, y, z}, Block::IronOre);
                    else if (variation % 11 == 0) world.set({x, y, z}, Block::CoalOre);
                }
            }
        }
        crystalGarden(world, {p.x + 3, Ground + 1, p.z + 4}, 3);
    }
    for (const auto p : {glm::ivec3(-17, 9, -7), {33, 9, 20}, {-7, 9, 29},
                         {20, 9, -15}, {-32, 9, 44}, {51, 9, 2}}) crystalGarden(world, p, 2);
    // Hanging rocks remain sparse and high above all roofs and walking routes.
    for (const auto p : {glm::ivec2(-21, -17), {37, -14}, {-28, 32}, {39, 37}, {8, -38}}) {
        int top = 59;
        while (top > 30 && world.get({p.x, top, p.y}) != Block::Air) --top;
        for (int drop = 0; drop < 8; ++drop) {
            const int width = drop < 3 ? 2 : drop < 6 ? 1 : 0;
            box(world, {p.x - width, top - drop, p.y - width}, {p.x + width, top - drop, p.y + width}, Block::Stone);
        }
    }
}
} // namespace

std::span<const HubBuilding> sceneBuildingPlan() { return Buildings; }

void buildUndergroundScene(World& world)
{
    landscape(world);
    geology(world);
    waterfall(world, -30, -17, 33);
    waterfall(world, 43, -16, 29);

    // Broad waterfront and residential loops; every entrance gets a paved spur.
    path(world, {-29, -27}, {43, -27});
    path(world, {-29, -27}, {-29, 46});
    path(world, {-29, 46}, {52, 46});
    path(world, {38, -27}, {38, 46});
    path(world, {-46, -8}, {-46, 36});
    path(world, {-46, 36}, {64, 36});
    path(world, {54, -8}, {54, 36});
    path(world, {-29, -40}, {47, -40});
    path(world, {-29, -40}, {-29, -24});
    path(world, {38, -40}, {38, -24});
    path(world, {8, -48}, {8, 51});
    path(world, {-44, 8}, {58, 8});

    for (const auto& building : Buildings) {
        const auto size = hubBuildingFootprint(building);
        box(world, building.origin - glm::ivec3(1, 0, 1),
            building.origin + glm::ivec3(size.x, 0, size.y), Block::Limestone);
        box(world, building.origin + glm::ivec3(-1, 1, -1),
            building.origin + glm::ivec3(size.x, 20, size.y), Block::Air);
        const auto door = hubBuildingDoor(building);
        const auto direction = std::array<glm::ivec2, 4>{{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}}[building.facing];
        path(world, {door.x, door.z}, glm::ivec2(door.x, door.z) + direction * 5, 1);
    }
    bridge(world, {8, 18}, {8, 34});
    bridge(world, {8, -19}, {8, -2});
    bridge(world, {-20, 8}, {-2, 8});
    bridge(world, {18, 8}, {36, 8});
    heartTree(world);
    for (const auto& building : Buildings) buildHubBuilding(world, building);
    expeditionGateway(world);
    greenery(world);

    for (const auto p : {glm::ivec2(-24, 31), {-11, 33}, {19, 33}, {34, 31}, {-25, 15}, {-25, -4},
                         {-23, -23}, {-8, -23}, {22, -23}, {35, -21}, {35, 1}, {35, 20},
                         {-24, 44}, {-5, 46}, {18, 46}, {32, 43}, {-42, 21}, {54, 17},
                         {-24, -38}, {20, -38}, {47, -29}}) lamp(world, p.x, p.y);
}
} // namespace voxel
