#include "scene.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace voxel {
namespace {
constexpr int Ground = 8;
constexpr glm::ivec2 Center{8, 8};
using Style = HubBuildingStyle;

// Districts have different footprints, rooflines and public buildings. The last
// number chooses a facade variation; all rooms are ordinary explorable space.
constexpr auto Buildings = std::to_array<HubBuilding>({
    // South: arrival square, market workshops and the large inn.
    {{-42,20,56},17,13,Style::Workshop,0,0},
    {{-20,16,63},11,11,Style::Cottage,0,1},
    {{23,18,57},25,19,Style::Inn,0,0},
    {{57,20,53},17,13,Style::Greenhouse,3,0},
    {{-68,22,47},19,17,Style::Hall,1,1},
    {{-41,18,84},13,11,Style::Cottage,2,2},
    {{-18,18,89},17,15,Style::Storehouse,2,0},
    {{20,18,88},13,13,Style::Cottage,2,0},
    {{39,20,84},13,11,Style::Cottage,2,1},
    {{57,22,82},13,13,Style::Tower,2,1},
    // West: the forge quarter and houses set into rocky alcoves.
    {{-79,20,26},21,17,Style::Workshop,1,1},
    {{-91,24,49},13,11,Style::Cottage,1,0},
    {{-72,22,72},17,13,Style::Storehouse,0,1},
    {{-100,22,25},11,11,Style::Cottage,1,2},
    {{-102,22,6},11,13,Style::Cottage,1,1},
    {{-84,26,1},13,13,Style::Tower,1,0},
    {{-81,28,-64},23,17,Style::Hall,0,0},
    {{-54,28,-58},17,13,Style::Workshop,0,2},
    {{-99,26,-52},13,11,Style::Cottage,1,1},
    {{-78,24,-43},13,11,Style::Cottage,1,2},
    // North: a civic avenue leading to the expedition hall.
    {{-37,26,-72},19,19,Style::Inn,0,1},
    {{-59,30,-87},13,11,Style::Cottage,0,0},
    {{-39,32,-95},13,13,Style::Tower,0,2},
    {{37,26,-71},23,19,Style::Hall,0,1},
    {{41,28,-94},17,15,Style::Storehouse,0,2},
    {{65,28,-83},13,11,Style::Cottage,0,1},
    {{73,26,-62},19,17,Style::Greenhouse,3,1},
    {{94,24,-49},13,11,Style::Cottage,3,2},
    // East: gardens, a long warehouse and a second neighborhood beyond the lake.
    {{51,14,26},17,15,Style::Greenhouse,3,2},
    {{78,18,23},21,17,Style::Workshop,3,0},
    {{97,16,4},11,13,Style::Cottage,3,0},
    {{91,20,48},17,15,Style::Storehouse,3,1},
    {{76,22,72},19,15,Style::Inn,2,2},
    // Smaller waterside residences keep the central tree readable from streets.
    {{-44,12,27},11,13,Style::Cottage,1,0},
    {{-24,12,46},11,9,Style::Cottage,0,2},
    {{25,12,46},11,9,Style::Cottage,0,1},
    {{-23,16,-42},11,11,Style::Cottage,0,1},
    {{27,16,-42},11,11,Style::Cottage,0,2},
});
constexpr std::array<HubBridge, 4> Bridges{{
    {{8,21},{8,47}}, {{8,-31},{8,-5}},
    {{-50,8},{-6,8}}, {{22,8},{65,8}},
}};
constexpr std::array<HubWaterfall, 2> Waterfalls{{
    {{-90,-18},1,39}, {{106,-14},-1,45},
}};
constexpr std::array<glm::ivec3, 9> ExpeditionApproach{{
    {8,8,-31}, {8,8,-35}, {8,14,-43}, {8,14,-46},
    {8,22,-54}, {8,22,-57}, {8,30,-65}, {8,30,-105}, {34,30,-105},
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

int lakebed(int x, int z)
{
    // Keep the cascade landings shallow, including their join to the lake.
    for (const auto& fall : Waterfalls) {
        const int along=(x-fall.lip.x)*fall.direction;
        if (along>=7 && along<=18 && std::abs(z-fall.lip.y)<=3) return 5;
    }
    // Four shallow shore cells preserve the existing exits. Farther in, each
    // additional cell lowers the bed by one block, down to Y2. Sample once per
    // column in all eight directions so opposite shores receive the same slope.
    int distance=7;
    for (const auto side : std::array<glm::ivec2,8>{{
        {1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}
    }}) for (int step=1;step<=6;++step) {
        if (!lake(x+side.x*step,z+side.y*step)) {
            distance=std::min(distance,step);
            break;
        }
    }
    return 5-std::max(0,distance-4);
}

// Rounded highlands meet the low water garden. The shoulders form natural
// ledges; streets later cut comfortable one-block stair flights through them.
int terrainHeight(int x, int z)
{
    struct Highland { glm::vec2 center, radius; float height; };
    float ground = Ground;
    for (const auto& hill : std::array<Highland,6>{{
        {{-63,40},{43,60},22}, {{8,87},{76,35},18},
        {{-57,-65},{51,46},29}, {{59,-65},{50,47},27},
        {{88,41},{39,48},20}, {{8,-81},{36,38},30},
    }}) {
        const float r = std::sqrt(ellipse(x,z,hill.center,hill.radius));
        const float edge = r + .045f*std::sin(x*.14f+z*.09f) + .035f*std::cos(z*.18f-x*.07f);
        const float shoulder = std::clamp((1.12f-edge)/.50f,0.0f,1.0f);
        const float rise = (hill.height-Ground)*shoulder;
        ground = std::max(ground,float(Ground)+std::floor(rise/2)*2);
    }
    float shore=std::max(0.0f,(std::sqrt(ellipse(x,z,Center,{42,36}))-1)*36);
    for (const auto basin : {glm::vec4(-64,-14,25,16),glm::vec4(78,-12,24,17)})
        shore=std::min(shore,std::max(0.0f,(std::sqrt(ellipse(x,z,{basin.x,basin.y},{basin.z,basin.w}))-1)*basin.w));
    ground=glm::mix(float(Ground),ground,std::clamp(shore/9,0.0f,1.0f));
    // Blend each house into a planted ledge, instead of exposing a rectangular
    // foundation pedestal. Keep the surrounding terrain's broad silhouette.
    for (const auto& b : Buildings) {
        const auto size = hubBuildingFootprint(b);
        const float dx = std::max({float(b.origin.x-x),float(x-b.origin.x-size.x+1),0.0f});
        const float dz = std::max({float(b.origin.z-z),float(z-b.origin.z-size.y+1),0.0f});
        const float distance = std::sqrt(dx*dx+dz*dz);
        const float weight = std::clamp((10-distance)/8,0.0f,1.0f);
        ground = glm::mix(ground,float(b.origin.y),weight*weight*(3-2*weight));
    }
    // Preserve a low shore all around the central water garden.
    if (lake(x,z)) return Ground;
    return std::max(Ground,int(std::round(ground)));
}

void landscape(World& world)
{
    for (int z = World::Min.z; z < World::MaxExclusive.z; ++z) {
        for (int x = World::Min.x; x < World::MaxExclusive.x; ++x) {
            const float distance = ellipse(x,z,Center,{108,104});
            const float lobes = .095f*std::sin(x*.051f+z*.021f)
                              + .065f*std::sin(z*.079f-x*.041f);
            const int ceiling = std::clamp(72+int(14*std::max(0.0f,1-distance))
                +int(5*std::sin(x*.055f)*std::cos(z*.067f)),65,91);
            const int ground = terrainHeight(x,z);
            const bool basin = lake(x,z);
            const int bed = basin ? lakebed(x,z) : 5;
            const Block bedMaterial = hash(x/4,z/4,world.seed())%7==0 ? Block::Limestone : Block::Sand;
            for (int y = 0; y < World::MaxExclusive.y; ++y) {
                const bool shell = y == 0 || y == World::MaxExclusive.y-1
                    || x == World::Min.x || x == World::MaxExclusive.x-1
                    || z == World::Min.z || z == World::MaxExclusive.z-1;
                // Sloping, broad mineral seams avoid the old tiled horizontal bands.
                const float seam = y+7*std::sin(x*.033f)+5*std::cos(z*.047f);
                Block block = std::sin(seam*.13f)>.65f ? Block::Slate : Block::Stone;
                if (std::sin(seam*.075f+x*.013f)>.93f) block=Block::Limestone;
                const float wall = 1+lobes+.045f*std::sin(y*.12f+x*.037f-z*.023f);
                if (distance<wall) {
                    if (y>ground && y<ceiling) block=Block::Air;
                    if (y==ground) block=hash(x/3,z/3,world.seed())%9 ? Block::Grass : Block::Dirt;
                    if (y==ground-1) block=Block::Dirt;
                    if (basin && y>=bed && y<=ground)
                        block=y==bed?bedMaterial:y<=6?Block::Water:Block::Air;
                }
                world.set({x,y,z},shell?Block::Bedrock:block);
            }
        }
    }
    for (int z=World::Min.z+2;z<World::MaxExclusive.z-2;++z) {
        for (int x=World::Min.x+2;x<World::MaxExclusive.x-2;++x) {
            if (!lake(x,z)) continue;
            int distance=3;
            for (const auto side : {glm::ivec2(1,0),{-1,0},{0,1},{0,-1}})
                for (int step=1;step<=2;++step)
                    if (!lake(x+side.x*step,z+side.y*step)) distance=std::min(distance,step);
            if (distance<=2) world.set({x,6,z},Block::Limestone);
            if (distance==1) world.set({x,7,z},Block::Limestone);
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

constexpr int MapWidth = World::MaxExclusive.x-World::Min.x;
using StreetMap = std::array<int,MapWidth*MapWidth>;
int mapIndex(glm::ivec2 p) { return (p.y-World::Min.z)*MapWidth+p.x-World::Min.x; }
glm::ivec2 mapPoint(int i) { return {i%MapWidth+World::Min.x,i/MapWidth+World::Min.z}; }

bool streetClear(glm::ivec2 p,int radius)
{
    if (p.x<=World::Min.x+radius || p.x>=World::MaxExclusive.x-radius-1
        || p.y<=World::Min.z+radius || p.y>=World::MaxExclusive.z-radius-1) return false;
    if (ellipse(p.x,p.y,Center,{109,106})>1.08f) return false;
    for (const auto& b : Buildings) {
        const auto size=hubBuildingFootprint(b);
        if (p.x>=b.origin.x-radius && p.x<b.origin.x+size.x+radius
            && p.y>=b.origin.z-radius && p.y<b.origin.z+size.y+radius) return false;
    }
    for (const auto& fall : Waterfalls) {
        const int u=(p.x-fall.lip.x)*fall.direction;
        if (u>=-22 && u<=11 && std::abs(p.y-fall.lip.y)<=15+radius) return false;
    }
    for (int dz=-radius;dz<=radius;++dz) for (int dx=-radius;dx<=radius;++dx)
        if (lake(p.x+dx,p.y+dz)) return false;
    return true;
}

// Route between district waypoints around houses and water. Heights are then
// interpolated along the route, so long flights have shallow, usable treads.
void street(World& world,glm::ivec3 from,glm::ivec3 to,int radius,StreetMap& streets)
{
    const auto closestClear=[&](glm::ivec3 p) {
        for (int r=0;r<24;++r) for (int dz=-r;dz<=r;++dz) for (int dx=-r;dx<=r;++dx) {
            if (std::abs(dx)!=r && std::abs(dz)!=r) continue;
            if (streetClear({p.x+dx,p.z+dz},radius)) return p+glm::ivec3(dx,0,dz);
        }
        return p;
    };
    from=closestClear(from); to=closestClear(to);
    const glm::ivec2 start{from.x,from.z},finish{to.x,to.z};
    const int first=mapIndex(start),last=mapIndex(finish);
    std::vector<float> cost(MapWidth*MapWidth,std::numeric_limits<float>::max());
    std::vector<int> previous(MapWidth*MapWidth,-1);
    using Candidate=std::pair<float,int>;
    std::priority_queue<Candidate,std::vector<Candidate>,std::greater<Candidate>> frontier;
    const auto estimate=[&](glm::ivec2 p) { return .70f*glm::length(glm::vec2(p-finish)); };
    cost[first]=0; frontier.push({estimate(start),first});
    while (!frontier.empty()) {
        const auto [rank,current]=frontier.top(); frontier.pop();
        if (current==last) break;
        const auto p=mapPoint(current);
        if (rank>cost[current]+estimate(p)+.001f) continue;
        for (const auto d : {glm::ivec2(1,0),{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}}) {
            const auto next=p+d;
            if (!streetClear(next,radius)) continue;
            const int i=mapIndex(next);
            const float travel=glm::length(glm::vec2(d));
            // Existing streets are convenient junctions; broad hills gently bend routes.
            const float price=travel*(streets[i]>=0?.72f:1.0f)
                +.012f*std::abs(terrainHeight(next.x,next.y)-terrainHeight(p.x,p.y));
            if (cost[current]+price>=cost[i]) continue;
            cost[i]=cost[current]+price; previous[i]=current;
            frontier.push({cost[i]+estimate(next),i});
        }
    }
    if (first!=last && previous[last]<0) return;
    std::vector<glm::ivec3> route;
    for (int i=last;;i=previous[i]) {
        const auto p=mapPoint(i); route.push_back({p.x,0,p.y});
        if (i==first) break;
    }
    std::reverse(route.begin(),route.end());
    route.front().y=streets[first]>=0?streets[first]:from.y;
    // Every intersection fixes a height before the next sloped flight is paved.
    std::size_t anchor=0;
    for (std::size_t i=1;i<route.size();++i) {
        const int old=streets[mapIndex({route[i].x,route[i].z})];
        if (old<0 && i+1<route.size()) continue;
        const int end=old>=0?old:to.y;
        const int begin=route[anchor].y;
        for (std::size_t j=anchor+1;j<=i;++j)
            route[j].y=int(std::round(glm::mix(float(begin),float(end),float(j-anchor)/float(i-anchor))));
        anchor=i;
    }
    if (route.size()==1) route[0].y=from.y;
    // Each cell uses its nearest centerline tread. This preserves both ends of
    // stairs, unlike repeatedly flattening overlapping square brush stamps.
    StreetMap proposed,nearest;
    proposed.fill(-1); nearest.fill(1000);
    for (const auto p : route) for (int dz=-radius;dz<=radius;++dz) for (int dx=-radius;dx<=radius;++dx) {
        const int i=mapIndex({p.x+dx,p.z+dz}),distance=dx*dx+dz*dz;
        if (distance<nearest[i]) { proposed[i]=p.y; nearest[i]=distance; }
    }
    for (int i=0;i<MapWidth*MapWidth;++i) {
        if (proposed[i]<0 || streets[i]>=0) continue;
        const auto p=mapPoint(i); const int y=proposed[i];
        box(world,{p.x,1,p.y},{p.x,y-1,p.y},Block::Stone);
        world.set({p.x,y,p.y},Block::Path);
        box(world,{p.x,y+1,p.y},{p.x,std::min(std::max(y+8,terrainHeight(p.x,p.y)+2),World::MaxExclusive.y-2),p.y},Block::Air);
        streets[i]=y;
    }
}

void expeditionApproach(World& world,StreetMap& streets)
{
    // Author the main entrance before the district paths. Their intersections
    // then inherit these tread heights, including the raised gateway landing.
    for (std::size_t i=1;i<ExpeditionApproach.size();++i) {
        const auto from=ExpeditionApproach[i-1],to=ExpeditionApproach[i];
        const int length=std::abs(to.x-from.x)+std::abs(to.z-from.z);
        const glm::ivec3 direction((to.x-from.x)/length,0,(to.z-from.z)/length);
        const glm::ivec3 across(-direction.z,0,direction.x);
        for (int step=0;step<=length;++step) {
            const int floor=int(std::round(glm::mix(float(from.y),float(to.y),float(step)/length)));
            for (int side=-3;side<=3;++side) {
                const auto p=from+direction*step+across*side;
                box(world,{p.x,1,p.z},{p.x,floor-1,p.z},Block::Stone);
                world.set({p.x,floor,p.z},Block::Path);
                box(world,{p.x,floor+1,p.z},{p.x,std::max(floor+6,terrainHeight(p.x,p.z)+2),p.z},Block::Air);
                streets[mapIndex({p.x,p.z})]=floor;
            }
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

// Uneven lobes and several layers of branches read as a spreading crown,
// rather than four straight arms supporting identical balls of leaves.
void canopy(World& world,glm::vec3 center,glm::vec3 radius)
{
    const auto low=glm::ivec3(glm::floor(center-radius*1.15f));
    const auto high=glm::ivec3(glm::ceil(center+radius*1.15f));
    for (int y=low.y;y<=high.y;++y) for (int z=low.z;z<=high.z;++z) for (int x=low.x;x<=high.x;++x) {
        const glm::vec3 q=(glm::vec3(x,y,z)-center)/radius;
        const float edge=1+.09f*std::sin(x*.71f+y*.29f)+.08f*std::cos(z*.63f-x*.22f)
                          +.06f*std::sin(y*.91f+z*.32f);
        if (glm::dot(q,q)<edge && world.get({x,y,z})==Block::Air) world.set({x,y,z},Block::Leaves);
    }
}

void heartTree(World& world)
{
    for (const auto root : {glm::vec3(-1,9,-1),{17,9,-1},{-1,9,17},{17,9,17}})
        branch(world,{8,14,8},root,3.1f,1.0f);
    branch(world,{8,10,8},{4,28,11},5.2f,4.0f);
    branch(world,{4,28,11},{10,44,6},4.0f,2.8f);
    branch(world,{10,44,6},{4,64,9},2.8f,1.0f);
    struct Bough { glm::vec3 base,elbow,tip,spread; };
    for (const auto& b : std::array<Bough,8>{{
        {{5,26,10},{-9,33,9},{-24,44,2},{14,7,11}},
        {{5,29,10},{20,34,14},{38,45,21},{13,8,13}},
        {{7,34,8},{0,41,-6},{-6,49,-24},{13,8,14}},
        {{8,35,8},{10,41,24},{18,51,40},{13,8,12}},
        {{9,42,7},{-4,49,18},{-20,58,25},{14,9,13}},
        {{9,44,7},{23,51,-3},{27,60,-12},{17,6,14}},
        {{6,52,8},{-6,58,2},{-12,63,-6},{18,6,16}},
        {{5,57,9},{9,62,14},{11,66,13},{19,6,17}},
    }}) {
        branch(world,b.base,b.elbow,2.8f,1.7f);
        branch(world,b.elbow,b.tip,1.7f,.65f);
        branch(world,b.elbow,b.tip+glm::vec3(-5,1,6),1.2f,.4f);
        canopy(world,b.tip+glm::vec3(0,4,0),b.spread);
        canopy(world,glm::mix(b.elbow,b.tip,.65f)+glm::vec3(0,5,0),b.spread*.72f);
    }
    canopy(world,{-3,59,7},{21,9,19});
    canopy(world,{12,52,17},{22,7,19});
    canopy(world,{-6,49,-9},{16,6,17});
    // Wide, coherent glow accents keep the lower boughs softly visible.
    for (int z=-42;z<=56;++z) for (int x=-42;x<=54;++x) for (int y=38;y<=86;++y) {
        if (world.get({x,y,z})!=Block::Leaves) continue;
        if (hash((x+48)/7+y/6,(z+48)/7,world.seed())%7==0
            && (world.get({x,y-1,z})==Block::Air || world.get({x,y+1,z})==Block::Air))
            world.set({x,y,z},Block::GlowLeaves);
    }
    for (const auto light : {glm::ivec3(3,18,9),{13,27,9},{8,38,4},{-8,32,9},{25,39,18},{10,40,26}}) {
        int support=light.y+1;
        while (support<70 && world.get({light.x,support,light.z})==Block::Air) ++support;
        if (support>=70) continue;
        box(world,light+glm::ivec3(0,1,0),{light.x,support,light.z},Block::Timber);
        world.set(light,Block::Lantern);
    }
    for (int z=-9;z<=25;++z) for (int x=-10;x<=26;++x) {
        const float r=glm::length((glm::vec2(x,z)-glm::vec2(Center))/glm::vec2(18,17));
        if (r>.72f && r<=1) world.set({x,Ground,z},Block::Path);
    }
}

// A rock outcrop is a height field: every column reaches the basin bed. This
// prevents hollow-bottom boulders even when part of a cluster stands in water.
void outcrop(World& world,glm::ivec2 center,int radius,int height,bool ore=false,int floor=-1)
{
    if (floor<0) floor=terrainHeight(center.x,center.y);
    for (int z=center.y-radius;z<=center.y+radius;++z) for (int x=center.x-radius;x<=center.x+radius;++x) {
        const float distance=glm::length(glm::vec2(x-center.x,z-center.y))/radius;
        if (distance>1) continue;
        const int top=floor+std::max(0,int(height*(1-distance*.72f)))+int(hash(x/2,z/2,world.seed())%2);
        for (int y=1;y<=top;++y) {
            Block material=(y/5)%3==0?Block::Limestone:Block::Slate;
            if (ore && y>floor) {
                const auto v=hash(x/2+y*3,z/2,world.seed());
                const auto seam=v%10;
                material=seam<2?Block::CoalOre:seam<4?Block::IronOre:seam<6?Block::CopperOre:Block::Stone;
            }
            world.set({x,y,z},material);
        }
        if (distance>.7f && top>floor+1) world.set({x,top,z},Block::Grass);
    }
}

void crystalGarden(World& world,glm::ivec2 center,int height)
{
    const int floor=terrainHeight(center.x,center.y);
    for (int dz=-2;dz<=2;++dz) for (int dx=-2;dx<=2;++dx) {
        if (std::abs(dx)+std::abs(dz)>3) continue;
        const int top=floor+std::max(0,height-std::abs(dx)-std::abs(dz));
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
        outcrop(world,{fall.lip.x+fall.direction*2,fall.lip.y+side},4,middle-4,false,Ground);
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
    const auto gateBox=[&](glm::ivec3 from,glm::ivec3 to,Block block) {
        if (from.y!=1) from.y+=22;
        to.y+=22; box(world,from,to,block);
    };
    const auto gateSet=[&](glm::ivec3 p,Block block) { p.y+=22; world.set(p,block); };
    gateBox({-14,1,-105},{30,Ground,-65},Block::Limestone);
    box(world,{-14,30,-105},{30,30,-65},Block::Path);
    gateBox({-12,9,-104},{28,43,-76},Block::Air);
    for (const int x : {-11,23}) {
        gateBox({x,9,-84},{x+4,35,-76},Block::Slate);
        gateBox({x-1,34,-85},{x+5,37,-75},Block::Limestone);
        gateBox({x,38,-84},{x+4,40,-76},Block::RoofTiles);
        gateSet({x+2,29,-75},Block::Lantern);
        gateSet({x+2,18,-75},Block::Lantern);
    }
    gateBox({-6,31,-83},{22,34,-77},Block::Limestone);
    gateBox({-2,35,-82},{18,37,-78},Block::Slate);
    gateBox({5,38,-81},{11,40,-79},Block::Crystal);
    // A deep roofed staging hall is visibly connected to the cavern behind it.
    gateBox({-12,9,-104},{-10,33,-85},Block::Slate);
    gateBox({26,9,-104},{28,33,-85},Block::Slate);
    gateBox({-12,32,-104},{28,34,-85},Block::Slate);
    gateBox({-12,9,-105},{28,34,-103},Block::Stone);
    for (const int x : {-6,0})
        gateBox({x,9,-102},{x+1,31,-101},Block::Limestone);
    // Carve a broad L-shaped passage from founded rock. Its right-hand opening
    // starts immediately behind the entrance, before a north wall can read as
    // another blocked door. The east end stays inside the sealed world shell.
    box(world,{3,1,-110},{40,43,-98},Block::Slate);
    box(world,{4,31,-109},{14,41,-98},Block::Air);
    box(world,{4,31,-108},{38,41,-100},Block::Air);
    box(world,{4,30,-109},{14,30,-98},Block::Path);
    box(world,{4,30,-108},{38,30,-100},Block::Path);
    for (const int x : {3,15}) box(world,{x,31,-98},{x,38,-98},Block::Timber);
    box(world,{3,38,-98},{15,39,-98},Block::Timber);
    world.set({14,37,-98},Block::Timber);
    world.set({14,36,-98},Block::Lantern);
    // The next supports and warm lights turn east, leaving the junction open.
    for (const int x : {16,24,32}) {
        for (const int z : {-109,-99}) box(world,{x,31,z},{x,38,z},Block::Timber);
        box(world,{x,38,-109},{x,39,-99},Block::Timber);
        world.set({x,37,-100},Block::Timber);
        world.set({x,36,-100},Block::Lantern);
    }
    // Flush timber bands guide feet around the bend without adding barriers.
    box(world,{4,30,-108},{4,30,-98},Block::Timber);
    box(world,{4,30,-108},{37,30,-108},Block::Timber);
    box(world,{12,30,-102},{12,30,-98},Block::Timber);
    box(world,{12,30,-102},{37,30,-102},Block::Timber);
    for (const int z : {-97,-87}) for (const int x : {-8,24}) lamp(world,x,z,30);
}

void smallTree(World& world,glm::ivec2 p,int height,int spread)
{
    int floor=World::MaxExclusive.y-3;
    while (floor>=Ground && world.get({p.x,floor,p.y})!=Block::Grass) --floor;
    if (floor<Ground) return;
    for (int y=floor+1;y<=floor+height;++y) for (int dz=-2;dz<=2;++dz) for (int dx=-2;dx<=2;++dx)
        if (world.get({p.x+dx,y,p.y+dz})!=Block::Air) return;
    branch(world,{p.x+.5f,floor+1.f,p.y+.5f},{p.x+1.f,floor+height-2.f,p.y},1.1f,.55f);
    branch(world,{p.x,floor+height-6.f,p.y},{p.x-spread*.6f,floor+height-1.f,p.y+2},.65f,.3f);
    canopy(world,{p.x,floor+height+1.f,p.y},{float(spread),4,float(spread)});
    canopy(world,{p.x-spread*.6f,floor+height-1.f,p.y+2},{spread*.7f,3,spread*.7f});
    const glm::ivec3 glow{p.x,floor+height-3,p.y+1};
    if (world.get(glow)==Block::Leaves) world.set(glow,Block::GlowLeaves);
}

void neighborhoods(World& world)
{
    StreetMap streets; streets.fill(-1);
    expeditionApproach(world,streets);
    const auto avenue=[&](std::initializer_list<glm::ivec3> points) {
        for (auto p=points.begin();p+1!=points.end();++p) street(world,*p,*(p+1),2,streets);
    };
    // Each waterfront landing climbs a different district. The roads wind
    // around buildings rather than cutting a rectangular grid through them.
    avenue({{8,8,48},{-2,14,63},{2,18,77},{24,18,79},{53,20,77},{72,20,70},{72,20,52},{70,18,32},{70,12,18},{65,8,8}});
    avenue({{-50,8,8},{-58,12,18},{-57,20,39},{-49,22,54},{-49,22,74},{2,18,77}});
    avenue({{-50,8,8},{-48,12,-9},{-56,20,-30},{-62,26,-44},{-62,28,-67},{-50,28,-79},{-30,28,-81},{8,30,-78}});
    avenue({{65,8,8},{66,10,-5},{63,18,-22},{61,24,-42},{80,26,-43},{86,26,-71},{62,28,-76},{30,28,-77},{8,30,-78}});
    for (int z=World::Min.z+7;z<World::MaxExclusive.z-7;++z) {
        for (int x=World::Min.x+7;x<World::MaxExclusive.x-7;++x) {
            const int y=streets[mapIndex({x,z})];
            if (y<14) continue;
            for (const auto side : {glm::ivec2(1,0),{-1,0},{0,1},{0,-1}}) {
                if (streets[mapIndex(glm::ivec2(x,z)+side)]>=0) continue;
                const int reach=3+int(hash(x/3,z/3,world.seed())%3);
                for (int step=1;step<=reach;++step) {
                    const auto p=glm::ivec2(x,z)+side*step;
                    if (streets[mapIndex(p)]>=0 || !streetClear(p,0)) break;
                    const int original=terrainHeight(p.x,p.y);
                    const int cut=y+step;
                    if (original>cut+1 && isSolid(world.get({p.x,cut,p.y}))) {
                        // Open the stair cutting into planted shoulders rather
                        // than leaving a deep, narrow trench with sheer banks.
                        box(world,{p.x,cut+1,p.y},{p.x,original+2,p.y},Block::Air);
                        world.set({p.x,cut,p.y},Block::Grass);
                        continue;
                    }
                    const int top=y-2*step;
                    if (top<=Ground || isSolid(world.get({p.x,top,p.y}))) continue;
                    box(world,{p.x,1,p.y},{p.x,top-1,p.y},Block::Stone);
                    world.set({p.x,top,p.y},Block::Grass);
                }
            }
        }
    }
    // Founded, soil-edged building terraces are small and distinct from streets.
    for (const auto& b : Buildings) {
        const auto size=hubBuildingFootprint(b);
        box(world,{b.origin.x-1,1,b.origin.z-1},b.origin+glm::ivec3(size.x,-1,size.y),Block::Stone);
        box(world,b.origin+glm::ivec3(-1,0,-1),b.origin+glm::ivec3(size.x,0,size.y),Block::Grass);
        box(world,b.origin+glm::ivec3(-1,1,-1),b.origin+glm::ivec3(size.x,hubBuildingHeight(b)+2,size.y),Block::Air);
    }
    for (const auto& b : Buildings) {
        const auto door=hubBuildingDoor(b)-glm::ivec3(0,1,0);
        const auto d=std::array<glm::ivec2,4>{{{0,1},{1,0},{0,-1},{-1,0}}}[b.facing];
        const auto outside=door+glm::ivec3(d.x*3,0,d.y*3);
        int nearest=-1; float distance=1e9f;
        for (int i=0;i<MapWidth*MapWidth;++i) {
            if (streets[i]<0 || !streetClear(mapPoint(i),1)) continue;
            const auto p=mapPoint(i);
            const float candidate=glm::length(glm::vec2(p-glm::ivec2(outside.x,outside.z)))
                +std::abs(streets[i]-b.origin.y)*1.5f;
            if (candidate<distance) { distance=candidate; nearest=i; }
        }
        if (nearest>=0) {
            const auto p=mapPoint(nearest);
            street(world,outside,{p.x,streets[nearest],p.y},1,streets);
        }
        // Level door landings meet the first stair, with clearance for the body.
        for (int step=0;step<=3;++step) for (int side=-1;side<=1;++side) {
            const auto p=door+glm::ivec3(d.x*step+d.y*side,0,d.y*step-d.x*side);
            box(world,{p.x,1,p.z},{p.x,p.y-1,p.z},Block::Stone);
            world.set(p,Block::Path);
            box(world,p+glm::ivec3(0,1,0),p+glm::ivec3(0,4,0),Block::Air);
        }
    }
    for (const auto& b : Buildings) buildHubBuilding(world,b);
    // Sparse lights follow real street elevations and leave the centerline clear.
    for (int z=World::Min.z+4;z<World::MaxExclusive.z-4;z+=9) {
        for (int x=World::Min.x+4;x<World::MaxExclusive.x-4;x+=9) {
            const int floor=streets[mapIndex({x,z})];
            if (floor<0 || hash(x,z,world.seed())%3==0) continue;
            for (const auto side : {glm::ivec2(3,0),{-3,0},{0,3},{0,-3}}) {
                if (streets[mapIndex(glm::ivec2(x,z)+side)]>=0) continue;
                if (isSolid(world.get({x+side.x,floor,z+side.y}))
                    && world.get({x+side.x,floor+1,z+side.y})==Block::Air) {
                    lamp(world,x+side.x,z+side.y,floor); break;
                }
            }
        }
    }
}

} // namespace
std::span<const HubBuilding> sceneBuildingPlan() { return Buildings; }
const HubBuilding& sceneBlacksmithBuilding() { return Buildings.front(); }
HubBlacksmithLayout sceneBlacksmithLayout()
{
    const auto& building = sceneBlacksmithBuilding();
    const auto origin = glm::vec3(building.origin);
    return {origin + glm::vec3(5.5f,1,7.5f),
            origin + glm::vec3(building.width-2.5f,1,2.5f),
            origin + glm::vec3(4.5f,1,2.5f),
            origin + glm::vec3(4.5f,3,1.02f),
            origin + glm::vec3(0.5f,5.8f,building.depth-0.01f)};
}
glm::vec3 sceneBlacksmithStation() { return sceneBlacksmithLayout().anvil + glm::vec3(0,1.4f,0); }
glm::vec3 sceneBlacksmithApproach() { return sceneBlacksmithLayout().anvil + glm::vec3(0,0,2.5f); }
std::span<const HubBridge> sceneBridgePlan() { return Bridges; }
std::span<const HubWaterfall> sceneWaterfallPlan() { return Waterfalls; }
std::span<const glm::ivec3> sceneExpeditionApproach() { return ExpeditionApproach; }

void buildUndergroundScene(World& world)
{
    landscape(world);
    neighborhoods(world);
    // Real voxel plinths make the work stations solid. The models add the
    // shaped iron, coals, tools and joinery without blocking the central aisle.
    const auto smith = sceneBlacksmithLayout();
    const auto anvil = glm::ivec3(glm::floor(smith.anvil));
    world.set(anvil, Block::Timber);
    const auto forge = glm::ivec3(glm::floor(smith.forge));
    box(world, forge + glm::ivec3(-1,0,-1), forge + glm::ivec3(1,0,1), Block::Brick);
    const auto bench = glm::ivec3(glm::floor(smith.workbench));
    box(world, bench + glm::ivec3(-2,0,0), bench + glm::ivec3(2,0,0), Block::Timber);
    // A solid rear panel carries the tool rack instead of hanging across a window.
    const auto wall = sceneBlacksmithBuilding().origin;
    box(world, wall + glm::ivec3(2,2,0), wall + glm::ivec3(7,5,0), Block::Timber);
    for (const auto& b : Bridges) bridge(world,b);
    heartTree(world);
    expeditionGateway(world);
    // These reserves do not overlap any room, door spur, or cleared road.
    for (const auto& fall : Waterfalls) waterfall(world,fall);
    for (const auto p : {glm::ivec2(-62,6),{87,9},{-49,-30},{54,-29}}) outcrop(world,p,4,7,true);
    for (const auto p : {glm::ivec2(-23,-10),{39,24},{-15,33},{29,-18},{-50,39},{69,4}}) crystalGarden(world,p,3);
    for (const auto p : {glm::ivec2(-43,45),{48,49},{-74,61},{70,47},{-43,-67},{64,-65},
                        {-30,22},{42,33},{-23,-26},{41,-26},{-61,46},{58,18},
                        {-46,17},{-37,43},{-18,55},{37,41},{52,17},{69,-29},
                        {-57,5},{-54,-21},{-29,-52},{23,-58},{-44,76},{50,76},
                        {-88,18},{100,34},{-64,-73},{64,-34}})
        smallTree(world,p,9+int(hash(p.x,p.y,world.seed())%6),4+int(hash(p.y,p.x,world.seed())%2));
}
} // namespace voxel
