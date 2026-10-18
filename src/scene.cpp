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

// Districts have different footprints, rooflines and public buildings. The last
// number chooses a facade variation; all rooms are ordinary explorable space.
constexpr auto Buildings = std::to_array<HubBuilding>({
    // South: arrival square, market workshops and the large inn.
    {{-42,8,56},17,13,Style::Workshop,0,0},
    {{-20,8,63},11,11,Style::Cottage,0,1},
    {{23,8,57},25,19,Style::Inn,0,0},
    {{57,8,53},17,13,Style::Greenhouse,3,0},
    {{-68,8,47},19,17,Style::Hall,1,1},
    {{-41,8,84},13,11,Style::Cottage,2,2},
    {{-18,8,89},17,15,Style::Storehouse,2,0},
    {{20,8,88},13,13,Style::Cottage,2,0},
    {{39,8,84},13,11,Style::Cottage,2,1},
    {{57,8,82},13,13,Style::Tower,2,1},
    // West: the forge quarter and houses set into rocky alcoves.
    {{-79,8,26},21,17,Style::Workshop,1,1},
    {{-91,8,49},13,11,Style::Cottage,1,0},
    {{-72,8,72},17,13,Style::Storehouse,0,1},
    {{-100,8,25},11,11,Style::Cottage,1,2},
    {{-102,8,6},11,13,Style::Cottage,1,1},
    {{-84,8,1},13,13,Style::Tower,1,0},
    {{-81,8,-64},23,17,Style::Hall,0,0},
    {{-54,8,-58},17,13,Style::Workshop,0,2},
    {{-99,8,-52},13,11,Style::Cottage,1,1},
    {{-78,8,-43},13,11,Style::Cottage,1,2},
    // North: a civic avenue leading to the expedition hall.
    {{-37,8,-72},19,19,Style::Inn,0,1},
    {{-59,8,-87},13,11,Style::Cottage,0,0},
    {{-39,8,-95},13,13,Style::Tower,0,2},
    {{37,8,-71},23,19,Style::Hall,0,1},
    {{41,8,-94},17,15,Style::Storehouse,0,2},
    {{65,8,-83},13,11,Style::Cottage,0,1},
    {{73,8,-62},19,17,Style::Greenhouse,3,1},
    {{94,8,-49},13,11,Style::Cottage,3,2},
    // East: gardens, a long warehouse and a second neighborhood beyond the lake.
    {{51,8,26},17,15,Style::Greenhouse,3,2},
    {{78,8,23},21,17,Style::Workshop,3,0},
    {{97,8,4},11,13,Style::Cottage,3,0},
    {{91,8,48},17,15,Style::Storehouse,3,1},
    {{76,8,72},19,15,Style::Inn,2,2},
    // Smaller waterside residences keep the central tree readable from streets.
    {{-44,8,27},11,13,Style::Cottage,1,0},
    {{-24,8,46},11,9,Style::Cottage,0,2},
    {{25,8,46},11,9,Style::Cottage,0,1},
    {{-23,8,-42},11,11,Style::Cottage,0,1},
    {{27,8,-42},11,11,Style::Cottage,0,2},
});
constexpr std::array<HubBridge, 4> Bridges{{
    {{8,21},{8,47}}, {{8,-31},{8,-5}},
    {{-50,8},{-6,8}}, {{22,8},{65,8}},
}};
constexpr std::array<HubWaterfall, 2> Waterfalls{{
    {{-90,-18},1,39}, {{106,-14},-1,45},
}};

std::uint32_t hash(int x, int z, std::uint32_t seed)
{
    auto value = seed ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u)
                      ^ (static_cast<std::uint32_t>(z) * 0x85ebca6bu);
    value ^= value >> 16; value *= 0x7feb352du;
    return value ^ (value >> 15);
}

void box(World& world, glm::ivec3 from, glm::ivec3 to, Block block)
{
    for (int y = from.y; y <= to.y; ++y)
        for (int z = from.z; z <= to.z; ++z)
            for (int x = from.x; x <= to.x; ++x) world.set({x,y,z}, block);
}

float ellipse(int x, int z, glm::vec2 center, glm::vec2 radius)
{
    const auto offset = (glm::vec2(x,z) - center) / radius;
    return glm::dot(offset, offset);
}

bool lake(int x, int z)
{
    return (ellipse(x,z,Center,{42,36}) < 1 && ellipse(x,z,Center,{18,17}) > 1)
        || ellipse(x,z,{-64,-14},{25,16}) < 1
        || ellipse(x,z,{78,-12},{24,17}) < 1
        || ellipse(x,z,{-34,0},{23,9}) < 1
        || ellipse(x,z,{51,0},{22,9}) < 1;
}

void landscape(World& world)
{
    for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
        for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
            const float distance = ellipse(x,z,Center,{108,104});
            // Broad lobes and ledges replace the old almost cylindrical wall.
            const float lobes = 0.045f * std::sin(x * .073f + z * .021f)
                              + 0.035f * std::sin(z * .089f - x * .031f);
            const bool chamber = distance < 1.0f + lobes;
            const auto variation = hash(x / 7,z / 7,world.seed());
            const int ceiling = std::clamp(56 + int(32 * std::max(0.0f,1-distance))
                + int(3 * std::sin(x*.08f) * std::cos(z*.07f)), 50, 91);
            const int ledge = distance > .77f ? int((distance-.77f)*65)
                + int(2 * std::sin(x*.11f+z*.13f)) : 0;
            const int ground = Ground + std::max(0,ledge);
            for (int y = 0; y < World::MaxExclusive.y; ++y) {
                const bool shell = y == 0 || y == World::MaxExclusive.y-1
                    || x == World::Min.x || x == World::MaxExclusive.x-1
                    || z == World::Min.z || z == World::MaxExclusive.z-1;
                Block block = (y / 9 + int(variation % 3)) % 4 == 0 ? Block::Slate : Block::Stone;
                if ((y / 13 + int(variation % 2)) % 4 == 0) block = Block::Limestone;
                if (chamber) {
                    if (y > ground && y < ceiling) block = Block::Air;
                    if (y == ground) block = hash(x/3,z/3,world.seed()) % 7 ? Block::Grass : Block::Dirt;
                    if (y == ground-1) block = Block::Dirt;
                    if (lake(x,z) && y >= 5 && y <= ground)
                        block = y == 5 ? Block::Sand : y == 6 ? Block::Water : Block::Air;
                }
                world.set({x,y,z},shell ? Block::Bedrock : block);
            }
        }
    }
    // Continuous shallow shelves surround every basin, including the river arms.
    for (int z = World::Min.z+2; z < World::MaxExclusive.z-2; ++z) {
        for (int x = World::Min.x+2; x < World::MaxExclusive.x-2; ++x) {
            if (!lake(x,z)) continue;
            int distance = 3;
            for (const auto side : {glm::ivec2(1,0),{-1,0},{0,1},{0,-1}})
                for (int step = 1; step <= 2; ++step)
                    if (!lake(x+side.x*step,z+side.y*step)) distance = std::min(distance,step);
            if (distance <= 2) world.set({x,6,z},Block::Limestone);
            if (distance == 1) world.set({x,7,z},Block::Limestone);
        }
    }
}

void sphere(World& world, glm::vec3 center, glm::vec3 radii, Block material, bool onlyAir = false)
{
    const auto low = glm::ivec3(glm::floor(center-radii)), high = glm::ivec3(glm::ceil(center+radii));
    for (int y=low.y; y<=high.y; ++y) for (int z=low.z; z<=high.z; ++z) for (int x=low.x; x<=high.x; ++x) {
        const auto offset = (glm::vec3(x,y,z)-center)/radii;
        if (glm::dot(offset,offset)<=1 && (!onlyAir || world.get({x,y,z})==Block::Air)) world.set({x,y,z},material);
    }
}

void path(World& world, glm::ivec2 from, glm::ivec2 to, int radius=2, Block material=Block::Limestone)
{
    const int steps=std::max(std::abs(to.x-from.x),std::abs(to.y-from.y));
    for (int step=0; step<=steps; ++step) {
        const auto p=glm::ivec2(glm::round(glm::mix(glm::vec2(from),glm::vec2(to),steps?float(step)/steps:0)));
        for (int z=p.y-radius; z<=p.y+radius; ++z) for (int x=p.x-radius; x<=p.x+radius; ++x) {
            // Roads have real foundations. Water crossings are built separately.
            if (world.get({x,6,z})==Block::Water) continue;
            box(world,{x,1,z},{x,Ground-1,z},Block::Stone);
            world.set({x,Ground,z},material);
            box(world,{x,Ground+1,z},{x,Ground+5,z},Block::Air);
        }
    }
}

void lamp(World& world,int x,int z,int floor=Ground)
{
    // Bridge lights have their own piers; street posts stay off the main aisle.
    if (world.get({x,floor,z})==Block::Planks || (floor==Ground && std::abs(x-Center.x)<=4)) return;
    if (!isSolid(world.get({x,floor,z})) || world.get({x,floor+1,z})!=Block::Air) return;
    box(world,{x,floor+1,z},{x,floor+2,z},Block::Timber);
    world.set({x,floor+3,z},Block::Lantern);
}

void bridge(World& world,const HubBridge& bridge)
{
    const bool northSouth=bridge.from.x==bridge.to.x;
    const int begin=northSouth?std::min(bridge.from.y,bridge.to.y):std::min(bridge.from.x,bridge.to.x);
    const int end=northSouth?std::max(bridge.from.y,bridge.to.y):std::max(bridge.from.x,bridge.to.x);
    for (int position=begin; position<=end; ++position) {
        for (int side=-3; side<=3; ++side) {
            const auto p=northSouth?glm::ivec2(bridge.from.x+side,position):glm::ivec2(position,bridge.from.y+side);
            world.set({p.x,Ground,p.y},Block::Planks);
            box(world,{p.x,Ground+1,p.y},{p.x,Ground+4,p.y},Block::Air);
        }
        for (const int side : {-4,4}) {
            const auto p=northSouth?glm::ivec2(bridge.from.x+side,position):glm::ivec2(position,bridge.from.y+side);
            world.set({p.x,Ground,p.y},Block::Timber);
            // Open gaps between supported piers reveal the water from walking height.
            if ((position-begin)%7==0 || position==end) {
                box(world,{p.x,5,p.y},{p.x,Ground+2,p.y},Block::Timber);
                world.set({p.x,Ground+3,p.y},Block::Lantern);
                const auto center=northSouth?glm::ivec2(bridge.from.x,position):glm::ivec2(position,bridge.from.y);
                if ((position-begin)%14==0) box(world,{center.x,5,center.y},{center.x,Ground-1,center.y},Block::Slate);
            }
        }
    }
}

void branch(World& world,glm::vec3 from,glm::vec3 to,float startWidth,float endWidth)
{
    const int steps=std::max(1,int(glm::distance(from,to)*2));
    for (int i=0;i<=steps;++i) {
        const float t=float(i)/steps;
        sphere(world,glm::mix(from,to,t),glm::vec3(glm::mix(startWidth,endWidth,t)),Block::Timber);
    }
}

void heartTree(World& world)
{
    for (const auto root : {glm::vec3(-1,9,-1),{17,9,-1},{-1,9,17},{17,9,17}})
        branch(world,{8,14,8},root,3.1f,1.0f);
    branch(world,{8,10,8},{6,33,9},5.0f,3.4f);
    branch(world,{6,33,9},{11,54,8},3.4f,1.6f);
    for (const auto tip : {glm::vec3(-18,52,-1),{35,56,18},{2,58,-22},{7,55,37}}) {
        const auto elbow=glm::mix(glm::vec3(8,32,8),tip,.58f)-glm::vec3(0,3,0);
        branch(world,{8,30,8},elbow,2.8f,1.8f);
        branch(world,elbow,tip,1.8f,.75f);
    }
    struct Crown { glm::vec3 center,radius; };
    for (const auto& c : std::array<Crown,12>{{
        {{10,66,8},{11,10,10}},{{-13,56,0},{12,7,10}},{{-24,60,4},{9,6,8}},
        {{31,60,17},{12,7,10}},{{40,64,15},{8,6,8}},{{2,63,-14},{11,9,12}},
        {{-2,68,-24},{8,7,8}},{{7,61,32},{12,9,12}},{{12,66,42},{8,7,8}},
        {{-9,65,25},{11,7,10}},{{24,69,-3},{11,8,10}},{{-1,72,6},{10,7,10}},
    }}) sphere(world,c.center,c.radius,Block::Leaves,true);
    for (int z=-36;z<=52;++z) for (int x=-36;x<=52;++x) for (int y=44;y<=80;++y) {
        if (world.get({x,y,z})!=Block::Leaves) continue;
        if (hash((x+40)/4+y/4,(z+40)/4,world.seed())%6==0
            && (world.get({x,y-1,z})==Block::Air || world.get({x,y+1,z})==Block::Air))
            world.set({x,y,z},Block::GlowLeaves);
    }
    for (const auto light : {glm::ivec3(3,20,9),{13,29,9},{8,40,4},{-8,46,2},{25,47,15},{5,53,29}}) {
        // Connect each lantern upward to an actual branch, not a floating short post.
        int support=light.y+1;
        while (support<62 && world.get({light.x,support,light.z})==Block::Air) ++support;
        if (support>=62) continue;
        box(world,light+glm::ivec3(0,1,0),{light.x,support,light.z},Block::Timber);
        world.set(light,Block::Lantern);
    }
    for (int z=-9;z<=25;++z) for (int x=-10;x<=26;++x) {
        const float r=glm::length((glm::vec2(x,z)-glm::vec2(Center))/glm::vec2(18,17));
        if (r>.72f && r<=1) world.set({x,Ground,z},Block::Limestone);
    }
}

// A rock outcrop is a height field: every column reaches the basin bed. This
// prevents hollow-bottom boulders even when part of a cluster stands in water.
void outcrop(World& world,glm::ivec2 center,int radius,int height,bool ore=false)
{
    for (int z=center.y-radius;z<=center.y+radius;++z) for (int x=center.x-radius;x<=center.x+radius;++x) {
        const float distance=glm::length(glm::vec2(x-center.x,z-center.y))/radius;
        if (distance>1) continue;
        const int top=Ground+std::max(0,int(height*(1-distance*.72f)))+int(hash(x/2,z/2,world.seed())%2);
        for (int y=1;y<=top;++y) {
            Block material=(y/5)%3==0?Block::Limestone:Block::Slate;
            if (ore && y>Ground) {
                const auto v=hash(x/2+y*3,z/2,world.seed());
                material=v%5==0?Block::CopperOre:v%7==0?Block::IronOre:v%11==0?Block::CoalOre:Block::Stone;
            }
            world.set({x,y,z},material);
        }
        if (distance>.7f && top>Ground+1) world.set({x,top,z},Block::Grass);
    }
}

void crystalGarden(World& world,glm::ivec2 center,int height)
{
    for (int dz=-2;dz<=2;++dz) for (int dx=-2;dx<=2;++dx) {
        if (std::abs(dx)+std::abs(dz)>3) continue;
        const int top=Ground+std::max(0,height-std::abs(dx)-std::abs(dz));
        box(world,{center.x+dx,1,center.y+dz},{center.x+dx,top,center.y+dz},Block::Slate);
        world.set({center.x+dx,top+1,center.y+dz},Block::Crystal);
    }
}

void waterfall(World& world,const HubWaterfall& fall)
{
    const auto cell=[&](int u,int y,int side) { return glm::ivec3(fall.lip.x+fall.direction*u,y,fall.lip.y+side); };
    const int middle=fall.top-13;
    // Continuous cliff mass joins the cavern wall behind the source channel.
    for (int u=-20;u<=9;++u) for (int side=-13;side<=13;++side) {
        const auto p=cell(u,0,side);
        if (p.x<=World::Min.x || p.x>=World::MaxExclusive.x-1) continue;
        const int edge=std::abs(side);
        const int mainHeight=u<0?fall.top:u<7?middle-1:5;
        const int bank=edge>3?std::max(8,mainHeight+3-int((edge-4)*1.5f)):mainHeight;
        const int ridge=edge>3?int(2.4f*std::sin(u*.43f+side*.61f)
                                  +2.0f*std::cos(side*.9f-u*.23f)):0;
        const int top=std::max(5,bank+ridge);
        box(world,{p.x,1,p.z},{p.x,top,p.z},Block::Slate);
        if (edge>3) world.set({p.x,top,p.z},Block::Grass);
    }
    // Founded buttresses break up the straight ledges beside each curtain.
    for (const int side : {-10,10})
        outcrop(world,{fall.lip.x+fall.direction*2,fall.lip.y+side},4,middle-4);
    // Source reservoir, first curtain, catch pool, second curtain, plunge pool.
    for (int side=-2;side<=2;++side) {
        for (int u=-10;u<=-1;++u) {
            world.set(cell(u,fall.top,side),Block::Water);
            box(world,cell(u,fall.top+1,side),cell(u,fall.top+5,side),Block::Air);
        }
        for (int y=middle;y<=fall.top;++y) world.set(cell(0,y,side),Block::Water);
        for (int u=0;u<=6;++u) world.set(cell(u,middle,side),Block::Water);
        for (int y=6;y<=middle;++y) world.set(cell(7,y,side),Block::Water);
        for (int u=8;u<=16;++u) {
            world.set(cell(u,5,side),Block::Sand);
            world.set(cell(u,6,side),Block::Water);
            box(world,cell(u,7,side),cell(u,Ground+1,side),Block::Air);
        }
    }
    // Small glowing mineral seams belong to the banks, away from the curtains.
    for (const int side : {-7,7}) {
        for (const int u : {-3,3}) {
            int top=fall.top+8;
            while (top>Ground && !isSolid(world.get(cell(u,top,side)))) --top;
            world.set(cell(u,top+1,side),u<0?Block::GlowLeaves:Block::Crystal);
        }
    }
}

void expeditionGateway(World& world)
{
    box(world,{-14,1,-105},{30,Ground,-65},Block::Limestone);
    box(world,{-12,9,-104},{28,43,-76},Block::Air);
    for (const int x : {-11,23}) {
        box(world,{x,9,-84},{x+4,35,-76},Block::Slate);
        box(world,{x-1,34,-85},{x+5,37,-75},Block::Limestone);
        box(world,{x,38,-84},{x+4,40,-76},Block::RoofTiles);
        world.set({x+2,29,-75},Block::Lantern);
        world.set({x+2,18,-75},Block::Lantern);
    }
    box(world,{-6,31,-83},{22,34,-77},Block::Limestone);
    box(world,{-2,35,-82},{18,37,-78},Block::Slate);
    box(world,{5,38,-81},{11,40,-79},Block::Crystal);
    // A deep roofed staging hall is visibly connected to the cavern behind it.
    box(world,{-12,9,-104},{-10,33,-85},Block::Slate);
    box(world,{26,9,-104},{28,33,-85},Block::Slate);
    box(world,{-12,32,-104},{28,34,-85},Block::Slate);
    box(world,{-12,9,-105},{28,34,-103},Block::Stone);
    box(world,{3,1,-110},{39,Ground,-102},Block::Slate);
    box(world,{4,9,-109},{14,24,-102},Block::Air);
    box(world,{4,9,-109},{38,24,-106},Block::Air);
    for (const int x : {-6,0,16,22})
        box(world,{x,9,-102},{x+1,31,-101},Block::Limestone);
    for (const int x : {2,15})
        box(world,{x,9,-102},{x+1,24,-100},Block::Limestone);
    box(world,{2,25,-102},{16,27,-100},Block::Limestone);
    for (const int x : {3,39}) world.set({x,14,-108},Block::Lantern);
    world.set({25,17,-110},Block::Crystal);
    for (const int z : {-97,-87}) for (const int x : {-8,24}) lamp(world,x,z);
    for (int z=-78;z<=-61;++z) {
        world.set({8,Ground,z},Block::RoofTiles);
        if ((z+78)%5==0) for (int dx=1;dx<=4;++dx) {
            world.set({8-dx,Ground,z+dx},Block::RoofTiles);
            world.set({8+dx,Ground,z+dx},Block::RoofTiles);
        }
    }
}

void smallTree(World& world,glm::ivec2 p,int height,int spread)
{
    if (world.get({p.x,Ground,p.y})!=Block::Grass) return;
    branch(world,{p.x+.5f,9,p.y+.5f},{p.x+1.f,Ground+height-2.f,p.y},1.1f,.55f);
    branch(world,{p.x,Ground+height-6.f,p.y},{p.x-spread*.6f,Ground+height-1.f,p.y+2},.65f,.3f);
    sphere(world,{p.x,Ground+height+1.f,p.y},{float(spread),4,float(spread)},Block::Leaves,true);
    sphere(world,{p.x-spread*.6f,Ground+height-1.f,p.y+2},{spread*.7f,3,spread*.7f},Block::Leaves,true);
    const glm::ivec3 glow{p.x,Ground+height-3,p.y+1};
    if (world.get(glow)==Block::Leaves) world.set(glow,Block::GlowLeaves);
}

void neighborhoods(World& world)
{
    // Wide roads surround rather than cut through the lake and cliff reserves.
    path(world,{-54,78},{72,78},3);
    path(world,{-54,48},{71,48},3);
    path(world,{-55,20},{-55,79},3);
    path(world,{72,19},{72,79},3);
    path(world,{-83,-43},{91,-43},3);
    path(world,{-54,-76},{-54,-43},2);
    path(world,{67,-75},{67,-40},2);
    path(world,{8,-99},{8,-29},4);
    path(world,{8,46},{8,94},4);
    path(world,{-87,16},{-36,16},2);
    path(world,{51,17},{103,17},2);
    path(world,{-80,63},{-54,63},2);
    path(world,{72,66},{102,66},2);
    // Each building gets a founded pad and full-height clearance. This happens
    // before any cliffs, trees or mineral beds are built, so it cannot sever them.
    for (const auto& b : Buildings) {
        const auto size=hubBuildingFootprint(b);
        box(world,{b.origin.x-1,1,b.origin.z-1},b.origin+glm::ivec3(size.x,0,size.y),Block::Limestone);
        box(world,b.origin+glm::ivec3(-1,1,-1),b.origin+glm::ivec3(size.x,hubBuildingHeight(b)+2,size.y),Block::Air);
        const auto door=hubBuildingDoor(b);
        const auto direction=std::array<glm::ivec2,4>{{{0,1},{1,0},{0,-1},{-1,0}}}[b.facing];
        path(world,{door.x,door.z},glm::ivec2(door.x,door.z)+direction*6,2);
    }
    for (const auto& b : Buildings) buildHubBuilding(world,b);
}

} // namespace
std::span<const HubBuilding> sceneBuildingPlan() { return Buildings; }
std::span<const HubBridge> sceneBridgePlan() { return Bridges; }
std::span<const HubWaterfall> sceneWaterfallPlan() { return Waterfalls; }

void buildUndergroundScene(World& world)
{
    landscape(world);
    neighborhoods(world);
    for (const auto& b : Bridges) bridge(world,b);
    heartTree(world);
    expeditionGateway(world);
    // These reserves do not overlap any room, door spur, or cleared road.
    for (const auto& fall : Waterfalls) waterfall(world,fall);
    for (const auto p : {glm::ivec2(-62,6),{87,9},{-49,-30},{54,-29}}) outcrop(world,p,4,7,true);
    for (const auto p : {glm::ivec2(-23,-10),{39,24},{-15,33},{29,-18},{-50,39},{69,4}}) crystalGarden(world,p,3);
    for (const auto p : {glm::ivec2(-43,45),{48,49},{-74,61},{70,47},{-43,-67},{64,-65},
                        {-30,22},{42,33},{-23,-26},{41,-26},{-61,46},{58,18}})
        smallTree(world,p,11+int(hash(p.x,p.y,world.seed())%5),4+int(hash(p.y,p.x,world.seed())%2));
    // Lighting follows streets and entrances, leaving the waterfall and lake open.
    for (const int z : {47,78}) for (int x=-48;x<=65;x+=14) lamp(world,x,z-4);
    for (const int x : {-55,72}) for (int z=25;z<=73;z+=14) lamp(world,x-4,z);
    for (int x=-77;x<=87;x+=14) lamp(world,x,-39);
    for (int z=-68;z<=-32;z+=12) for (const int x : {2,14}) lamp(world,x,z);
    for (int z=51;z<=88;z+=12) for (const int x : {2,14}) lamp(world,x,z);
}
} // namespace voxel
