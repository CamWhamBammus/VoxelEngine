#include "heartwood.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace voxel {
namespace {
constexpr HeartwoodLayout Layout{
    {8.5f,9,11.5f}, {8.5f,11.0f,11.5f}, {8.5f,9,16.5f},
    {2.5f,12.0f,29.5f}, {8.5f,24.0f,-8.5f}
};
constexpr std::array<HeartwoodRoom,3> Rooms{{
    {"Root Library", {-13,22,-16}, {1,29,-5}, {-2.5f,22,-2.5f}},
    {"Expedition Study", {20,22,-16}, {33,29,-4}, {26.5f,22,-1.5f}},
    {"Upper Archive", {-13,35,-16}, {1,42,-5}, {-2.5f,35,-2.5f}}
}};

void box(World& world, glm::ivec3 low, glm::ivec3 high, Block block)
{
    for (int y=low.y;y<=high.y;++y)
        for (int z=low.z;z<=high.z;++z)
            for (int x=low.x;x<=high.x;++x) world.set({x,y,z},block);
}

bool footprint(int x,int z,int inset=0)
{
    const int dx=std::abs(x-8),dz=std::abs(z-8);
    return std::max(dx,dz)<=28-inset && dx+dz<=48-inset*2;
}

void foundation(World& world)
{
    // The hall sits on a real enlarged island, not a platform over the old pond.
    for (int z=-25;z<=41;++z) for (int x=-25;x<=41;++x) {
        if (!footprint(x,z,-3)) continue;
        box(world,{x,1,z},{x,7,z},Block::Stone);
        world.set({x,8,z},footprint(x,z)?Block::AgedPlanks:Block::Path);
        if (footprint(x,z)) box(world,{x,9,z},{x,61,z},Block::Air);
    }
    // Fitted stone surrounds the golden seed. Timber bands connect the doors.
    box(world,{-2,8,2},{18,8,22},Block::Path);
    for (int z=-19;z<=35;++z) for (const int x : {-3,19})
        world.set({x,8,z},Block::Timber);
    for (int x=-19;x<=35;++x) for (const int z : {1,23})
        world.set({x,8,z},Block::Timber);
    box(world,{3,8,25},{13,8,39},Block::Path);
    box(world,{3,8,-23},{13,8,-16},Block::Path);
    box(world,{-23,8,3},{-16,8,13},Block::Path);
    box(world,{29,8,3},{39,8,13},Block::Path);
}

void shell(World& world)
{
    for (int z=-20;z<=36;++z) for (int x=-20;x<=36;++x) {
        if (!footprint(x,z) || footprint(x,z,3)) continue;
        const int rib=(std::abs(x*7+z*3)%7==0)?2:0;
        for (int y=9;y<=54+rib;++y) {
            const bool band=y==20||y==33||y==47||y==53;
            const bool bark=(std::abs(x+z)%4==0);
            world.set({x,y,z},band?Block::Timber:bark?Block::RootWood:Block::Timber);
        }
    }
    // Tall open arches frame the water and districts on all four sides.
    box(world,{-1,9,31},{17,27,37},Block::Air);
    box(world,{31,9,-1},{37,27,23},Block::Air);
    box(world,{-21,9,1},{-15,19,15},Block::Air);
    box(world,{3,9,-21},{13,19,-15},Block::Air);
    // Stepped shoulders make the openings part of a living timber arch.
    for (int step=0;step<5;++step) {
        box(world,{-1+step,24+step,32},{-1+step,28,36},Block::Timber);
        box(world,{17-step,24+step,32},{17-step,28,36},Block::Timber);
        box(world,{32,24+step,-1+step},{36,28,-1+step},Block::Timber);
        box(world,{32,24+step,23-step},{36,28,23-step},Block::Timber);
    }
    // Inner columns carry the balconies, with broad rooted stone feet.
    for (const auto p : std::array<glm::ivec3,8>{{
        {-17,0,-17},{3,0,-17},{18,0,-17},{33,0,-17},
        {-17,0,28},{-1,0,32},{18,0,32},{33,0,28}
    }}) {
        box(world,{p.x-1,9,p.z-1},{p.x+1,49,p.z+1},Block::Timber);
        box(world,{p.x-2,9,p.z-2},{p.x+2,10,p.z+2},Block::MossBrick);
        for (int y=11;y<=18;++y) {
            const int root=std::max(0,3-(y-11)/2);
            for (int dx=-root;dx<=root;++dx) for (int dz=-root;dz<=root;++dz)
                if (std::abs(dx)+std::abs(dz)<=root+1)
                    world.set({p.x+dx,y,p.z+dz},Block::RootWood);
        }
    }
    // Crossed roof ribs support every high hanging lantern and the canopy.
    for (const int x : {-12,8,28}) box(world,{x,48,-18},{x+1,49,34},Block::Timber);
    for (const int z : {-13,9,29}) box(world,{-18,48,z},{34,49,z+1},Block::Timber);
    for (int y=50;y<=59;++y) {
        const int inset=(y-50)/2;
        for (int z=-20+inset;z<=36-inset;++z) for (int x=-20+inset;x<=36-inset;++x) {
            if (!footprint(x,z,inset) || footprint(x,z,inset+1)) continue;
            world.set({x,y,z},Block::Timber);
        }
    }
}

void sapWall(World& world)
{
    // A recessed blue seam climbs the back wall; it never cuts through a path.
    box(world,{5,9,-13},{12,56,-8},Block::RootWood);
    for (int y=10;y<=56;++y) {
        const int x=8+int(std::round(std::sin(y*.22f)*1.4f));
        world.set({x,y,-7},Block::Crystal);
        if (y%4!=0) world.set({x+1,y,-7},Block::GlowMoss);
        world.set({x,y,-8},Block::Crystal);
    }
    box(world,{3,9,-6},{14,9,-4},Block::MossBrick);
    for (const auto p : {glm::ivec3(3,10,-6),{14,10,-6},{5,10,-4},{12,10,-4}})
        world.set(p,Block::Crystal);
}

} // namespace

const HeartwoodLayout& heartwoodLayout() { return Layout; }
std::span<const HeartwoodRoom> heartwoodRooms() { return Rooms; }

void buildHeartwood(World& world)
{
    foundation(world);
    shell(world);
    sapWall(world);
}
} // namespace voxel
