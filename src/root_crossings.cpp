#include "root_crossings.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

constexpr std::array<RootCrossingInfo, RootCrossingCount> Catalog{{
    {"Fallen Elder", "A broad fallen root with two broken spans above a quiet pool.", "INTRO"},
    {"Braided Roots", "Follow weaving roots from one side of the grotto to the other.", "EASY"},
    {"Forked Grove", "Choose the broad west root or the narrower east shortcut.", "EASY"},
    {"Root Stair", "Climb a stepped root ridge, then descend into the exit garden.", "EASY"},
    {"Elder Spiral", "Circle an ancient trunk and climb to its upper roots.", "MEDIUM"},
    {"Hanging Garden", "Cross high roots suspended from the living cave canopy.", "MEDIUM"},
    {"Waterfall Ledge", "Traverse roots along the wall beside a luminous cascade.", "EASY"},
    {"Garden Knots", "Hop between mossy root knots and small garden islands.", "EASY"},
    {"Broken Crown", "Balance along narrower branches in the crown of a fallen tree.", "MEDIUM"},
    {"Tangled Heart", "A longer winding crossing with turns, climbs, and broken roots.", "MEDIUM"},
}};

struct Plan {
    std::vector<glm::ivec3> points;
    std::vector<int> breaks;
    float width = 1.45f;
};

Plan plan(int index)
{
    // Each point is a feet position. Gentle rises are ordinary one-block steps;
    // breaks are deliberate jumps cut into selected spans after construction.
    switch (index) {
    case 0: return {{{0,18,32},{0,18,20},{0,18,8},{0,18,-8},{0,18,-20},{0,18,-32}}, {1,3}, 1.9f};
    case 1: return {{{0,18,32},{-8,18,24},{-8,18,12},{8,18,4},{8,18,-8},{-8,18,-16},{-8,18,-24},{0,18,-32}}, {1,3,5}};
    case 2: return {{{0,18,32},{0,18,22},{-14,18,14},{-14,18,0},{-14,18,-14},{0,18,-24},{0,18,-32}}, {2,3}, 1.8f};
    case 3: return {{{0,18,32},{-10,18,24},{-10,20,12},{0,22,4},{10,24,-4},{10,22,-16},{0,20,-24},{0,18,-32}}, {2,4}};
    case 4: return {{{0,18,32},{-12,18,22},{-16,19,8},{-16,20,-10},{0,21,-16},{16,22,-10},{16,23,8},{0,24,16},{-8,25,0},{0,27,-8},{10,27,-20},{0,18,-32}}, {2,5}, 1.6f};
    case 5: return {{{0,18,32},{0,20,22},{-10,22,14},{-10,24,0},{10,24,-12},{0,21,-22},{0,18,-32}}, {3}, 1.7f};
    case 6: return {{{0,18,32},{18,18,24},{24,19,12},{24,20,0},{24,20,-12},{18,19,-24},{0,18,-32}}, {2,3}, 1.8f};
    case 7: return {{{0,18,32},{-10,18,22},{0,19,12},{10,18,4},{0,19,-6},{-10,18,-16},{0,18,-24},{0,18,-32}}, {1,3,5}, 1.65f};
    case 8: return {{{0,18,32},{0,19,22},{12,21,14},{12,22,0},{0,23,-8},{-12,22,-16},{-12,20,-24},{0,18,-32}}, {2,4}, 1.15f};
    default: return {{{0,18,32},{-16,18,24},{-16,19,10},{8,20,10},{16,21,0},{8,22,-10},{-12,23,-10},{-18,21,-22},{0,18,-32}}, {1,2,5}, 1.4f};
    }
}

void box(World& world, glm::ivec3 low, glm::ivec3 high, Block block)
{
    for (int y=low.y;y<=high.y;++y)
        for (int z=low.z;z<=high.z;++z)
            for (int x=low.x;x<=high.x;++x) world.set({x,y,z},block);
}

void clear(World& world)
{
    // Most of the fixed world is already air. Avoid dirtying its empty chunks.
    for (std::size_t i=0;i<World::ChunkCount;++i) {
        const auto origin=world.origin(i);
        const auto& chunk=world.chunk(i);
        for (int y=0;y<Chunk::Size;++y)
            for (int z=0;z<Chunk::Size;++z)
                for (int x=0;x<Chunk::Size;++x)
                    if (chunk.get(x,y,z)!=Block::Air) world.set(origin+glm::ivec3(x,y,z),Block::Air);
    }
}

void ball(World& world, glm::vec3 center, glm::vec3 radius, Block block, bool onlyAir=false)
{
    const glm::ivec3 low=glm::floor(center-radius), high=glm::ceil(center+radius);
    for (int y=low.y;y<=high.y;++y)
        for (int z=low.z;z<=high.z;++z)
            for (int x=low.x;x<=high.x;++x) {
                const auto distance=(glm::vec3(x,y,z)-center)/radius;
                if (glm::dot(distance,distance)<=1 && (!onlyAir || world.get({x,y,z})==Block::Air))
                    world.set({x,y,z},block);
            }
}

void branch(World& world, glm::vec3 from, glm::vec3 to, float radius)
{
    const int steps=std::max(1,int(glm::length(to-from)*3));
    for (int i=0;i<=steps;++i) ball(world,glm::mix(from,to,float(i)/steps),glm::vec3(radius),Block::Timber);
}

void root(World& world, glm::ivec3 from, glm::ivec3 to, float width)
{
    const int steps=std::max(1,int(glm::length(glm::vec3(to-from))*4));
    for (int i=0;i<=steps;++i) {
        const glm::vec3 p=glm::mix(glm::vec3(from),glm::vec3(to),float(i)/steps);
        const int top=int(std::floor(p.y+.0001f))-1;
        for (int z=int(std::floor(p.z-width));z<=int(std::ceil(p.z+width));++z)
            for (int x=int(std::floor(p.x-width));x<=int(std::ceil(p.x+width));++x) {
                const float d=std::hypot(x-p.x,z-p.z);
                if (d>width) continue;
                const int depth=d<width*.65f?3:2;
                box(world,{x,top-depth+1,z},{x,top,z},Block::Timber);
            }
    }
}

void landing(World& world, glm::ivec3 p, int radius)
{
    for (int z=-radius;z<=radius;++z)
        for (int x=-radius;x<=radius;++x) {
            if (x*x+z*z>radius*radius+1) continue;
            box(world,p+glm::ivec3(x,-3,z),p+glm::ivec3(x,-2,z),Block::Timber);
            world.set(p+glm::ivec3(x,-1,z),Block::Grass);
            box(world,p+glm::ivec3(x,0,z),p+glm::ivec3(x,3,z),Block::Air);
        }
}

void tree(World& world, glm::ivec3 base, int height, int variant)
{
    branch(world,glm::vec3(base),glm::vec3(base+glm::ivec3(variant%3-1,height,0)),1.6f);
    for (int side : {-1,1}) {
        branch(world,glm::vec3(base+glm::ivec3(0,height-4,0)),
               glm::vec3(base+glm::ivec3(side*4,height+1,side*2)),1.05f);
        ball(world,glm::vec3(base+glm::ivec3(side*3,height+2,side*2)),{4.5f,2.8f,4.0f},
             side<0?Block::Leaves:Block::GlowLeaves,true);
    }
    ball(world,glm::vec3(base+glm::ivec3(0,height+4,0)),{4,2.5f,3.5f},Block::Leaves,true);
}

void cavern(World& world, int variant)
{
    for (int z=-39;z<=39;++z) for (int x=-35;x<=35;++x) {
        const float r=std::max(std::abs(x)/35.0f,std::abs(z)/39.0f);
        const int ceiling=44-int(7*r*r)+int(std::sin(x*.22f)*std::cos(z*.18f)*2);
        const int wall=std::abs(x)>=33 || std::abs(z)>=37 ? 1 : 0;
        for (int y=2;y<=47;++y) {
            Block material=Block::Air;
            if (y<=4 || wall || y>=ceiling) {
                material=(y/5+(x+z+200)/9)%3==0?Block::Limestone:Block::Slate;
                if (y==2) material=Block::Bedrock;
            } else if (y<=7) material=Block::Water;
            world.set({x,y,z},material);
        }
        // Raised arrival and exit gardens meet the cave walls; the deep water
        // between them is a fall zone, never an accidental walkable shortcut.
        if (std::abs(z)>=28 && std::abs(x)<14) {
            const int top=17-std::max(0,std::abs(x)-10);
            box(world,{x,5,z},{x,top-1,z},Block::Limestone);
            world.set({x,top,z},Block::Grass);
        }
    }
    // Rooted side groves frame the silhouette without filling the jump route.
    for (int side : {-1,1}) for (int z : {-25,25}) {
        const glm::ivec3 base{side*25,10,z};
        ball(world,glm::vec3(base-glm::ivec3(0,4,0)),{7,5,7},Block::Limestone);
        ball(world,glm::vec3(base-glm::ivec3(0,1,0)),{5,1.5f,5},Block::Grass);
        tree(world,base,16+(variant%3),variant+side);
    }
    // Small bioluminescent mineral patches make the water and cave readable.
    for (int side : {-1,1}) for (int z : {-18,0,18}) {
        ball(world,{float(side*32),12.0f,float(z)},{1.5f,2,2.5f},Block::Crystal);
        ball(world,{float(side*31),20.0f,float(z+3)},{1,3,1.5f},Block::GlowLeaves,true);
    }
}

void marker(World& world, glm::ivec3 feet, Block light)
{
    // Offset the short paired markers outside a clear three-block doorway.
    for (int x : {-3,3}) {
        box(world,feet+glm::ivec3(x,-2,0),feet+glm::ivec3(x,1,0),Block::Timber);
        world.set(feet+glm::ivec3(x,2,0),light);
    }
}

void cutBreak(World& world, glm::ivec3 from, glm::ivec3 to)
{
    const glm::vec3 midpoint=(glm::vec3(from)+glm::vec3(to))*.5f;
    const glm::vec2 direction=glm::normalize(glm::vec2(to.x-from.x,to.z-from.z));
    for (int z=int(midpoint.z)-5;z<=int(midpoint.z)+5;++z)
        for (int x=int(midpoint.x)-5;x<=int(midpoint.x)+5;++x) {
            const glm::vec2 delta(x-midpoint.x,z-midpoint.z);
            // Level spans lose two rows; ascending spans lose one so a walking
            // jump has enough time to reach the higher landing. Diagonal cuts
            // use the same physical width.
            const float along=glm::dot(delta,direction);
            const float across=std::abs(delta.x*direction.y-delta.y*direction.x);
            const float end=from.y==to.y?1.5f:.5f;
            if (along>=-.5f && along<end && across<=3.0f)
                box(world,{x,11,z},{x,int(midpoint.y)+4,z},Block::Air);
        }
}

} // namespace

std::span<const RootCrossingInfo> rootCrossingCatalog() { return Catalog; }

RootCrossingCourse buildRootCrossing(World& world, int index)
{
    if (index<0 || index>=RootCrossingCount) throw std::out_of_range("Root Crossing index must be 0 through 9");
    const Plan layout=plan(index);
    clear(world);
    cavern(world,index);
    for (std::size_t i=1;i<layout.points.size();++i)
        root(world,layout.points[i-1],layout.points[i],layout.width);

    // Every root run joins a planted knot or a cave-side tree. Broken spans
    // therefore have visible grown supports instead of hovering log platforms.
    for (std::size_t i=1;i+1<layout.points.size();++i) {
        const auto p=layout.points[i];
        const int side=p.x<0?-1:1;
        const glm::vec3 below{float(p.x+side*5),7.0f,float(p.z+3)};
        branch(world,below,glm::vec3(p-glm::ivec3(0,3,0)),1.7f);
        branch(world,below,glm::vec3(p+glm::ivec3(side*7,4,2)),1.3f);
        ball(world,below-glm::vec3(0,2,0),{4,3,4},Block::Limestone);
        ball(world,glm::vec3(p+glm::ivec3(side*7,5,2)),{3,2,3},
             i%2?Block::GlowLeaves:Block::Leaves,true);
    }

    if (index==2) {
        const std::array<glm::ivec3,4> shortcut{{{0,18,22},{13,18,9},{13,18,-10},{0,18,-24}}};
        for (std::size_t i=1;i<shortcut.size();++i) root(world,shortcut[i-1],shortcut[i],1.05f);
        branch(world,{20,6,0},{13,16,0},2);
        cutBreak(world,shortcut[1],shortcut[2]);
    }
    if (index==4) tree(world,{0,7,0},25,index);
    if (index==5) {
        for (const auto p : {layout.points[2],layout.points[3],layout.points[4]}) {
            branch(world,glm::vec3(p+glm::ivec3(4,0,0)),{float(p.x+7),39,float(p.z)},1.2f);
            ball(world,{float(p.x+7),37,float(p.z)},{4,2,4},Block::GlowLeaves,true);
        }
    }
    if (index==6) {
        // The spring is cut into the wall and flows down a continuous rock face.
        box(world,{27,6,-5},{32,28,5},Block::Limestone);
        box(world,{27,28,-3},{31,29,3},Block::Water);
        box(world,{26,8,-3},{26,28,3},Block::Water);
        box(world,{24,7,-6},{27,8,6},Block::Water);
    }
    if (index==7) for (std::size_t i=1;i+1<layout.points.size();++i)
        landing(world,layout.points[i],3);
    if (index==8) {
        branch(world,{-22,7,2},{22,15,2},2.6f);
        branch(world,{18,13,2},{28,28,6},1.8f);
        ball(world,{28,29,6},{5,3,5},Block::GlowLeaves,true);
    }

    const auto checkpoint=layout.points[layout.points.size()/2];
    landing(world,layout.points.front(),4);
    landing(world,layout.points.back(),4);
    landing(world,checkpoint,3);
    marker(world,layout.points.front(),Block::Lantern);
    marker(world,layout.points.back(),Block::Lantern);
    marker(world,checkpoint,Block::Crystal);
    for (const int edge : layout.breaks) cutBreak(world,layout.points[edge],layout.points[edge+1]);

    RootCrossingCourse course{};
    for (const auto p : layout.points) course.route.emplace_back(glm::vec3(p)+glm::vec3(.5f,0,.5f));
    course.spawn=course.route.front();
    course.finish=course.route.back();
    course.checkpoint=glm::vec3(checkpoint)+glm::vec3(.5f,0,.5f);
    course.resetY=12.0f;
    const auto direction=course.route[1]-course.spawn;
    course.spawnYaw=std::atan2(direction.z,direction.x)*180.0f/3.14159265358979323846f;
    return course;
}

} // namespace voxel
