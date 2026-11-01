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

void galleries(World& world)
{
    for (const int floor : {HeartwoodGalleryFloor,HeartwoodUpperFloor}) {
        for (int z=-17;z<=33;++z) for (int x=-17;x<=33;++x) {
            if (!footprint(x,z,2)) continue;
            const bool ring=x<=-6 || x>=21 || z<=-5 || z>=26;
            const bool porch=z<=0;
            if (!ring && !porch) continue;
            world.set({x,floor,z},Block::AgedPlanks);
            world.set({x,floor-1,z},Block::Timber);
        }
        // Deep inner fascias and corbel steps expose real depth below the rail.
        for (const int x : {-6,21}) box(world,{x,floor-2,1},{x,floor-1,26},Block::Timber);
        box(world,{-6,floor-2,26},{21,floor-1,26},Block::Timber);
        for (const int x : {-6,21}) for (const int z : {3,9,15,21,25}) {
            box(world,{x,floor+1,z},{x,floor+2,z},Block::Timber);
            if (z==3||z==15||z==25) world.set({x,floor+3,z},Block::Lantern);
            for (int drop=1;drop<=3;++drop) {
                const int inward=x==-6?-1:1;
                const int first=x+inward*drop,last=x+inward*(drop+1);
                box(world,{std::min(first,last),floor-2-drop,z},{std::max(first,last),floor-1,z},Block::Timber);
            }
        }
        for (const int x : {-5,1,7,13,19}) {
            box(world,{x,floor+1,26},{x,floor+2,26},Block::Timber);
            if (x==1||x==13) world.set({x,floor+3,26},Block::Lantern);
        }
    }
    // Both stairwells are cut before placing their treads. The flight below
    // must keep full headroom right up to the landing above it.
    box(world,{-15,19,16},{-9,23,28},Block::Air);
    box(world,{-15,32,0},{-9,36,12},Block::Air);
}

void stairs(World& world)
{
    const auto flight=[&](int lowFloor,int firstZ) {
        for (int step=0;step<13;++step) {
            const int z=firstZ-step,top=lowFloor+1+step;
            // One voxel stair supplies two half-height treads; the player
            // climbs with the same shape and collision used in the editor.
            box(world,{-15,lowFloor+1,z},{-9,top-1,z},Block::Timber);
            box(world,{-15,top,z},{-9,top,z},Block::PlankStairs);
            for (const int x : {-16,-8}) {
                world.set({x,top,z},Block::Timber);
                if (step%4==0) {
                    box(world,{x,top+1,z},{x,top+2,z},Block::Timber);
                    world.set({x,top+3,z},Block::Lantern);
                }
            }
        }
    };
    flight(HeartwoodFloor,27);
    flight(HeartwoodGalleryFloor,11);
}

void rooms(World& world)
{
    for (const auto& room : Rooms) {
        const auto low=room.minimum,high=room.maximum;
        const int floor=low.y-1;
        box(world,{low.x-1,floor,low.z-1},{high.x+1,floor,high.z+1},Block::AgedPlanks);
        box(world,low,high,Block::Air);
        box(world,{low.x-1,low.y,low.z-1},{high.x+1,high.y,low.z-1},Block::AgedPlanks);
        box(world,{low.x-1,low.y,high.z+1},{high.x+1,high.y,high.z+1},Block::AgedPlanks);
        box(world,{low.x-1,low.y,low.z-1},{low.x-1,high.y,high.z+1},Block::AgedPlanks);
        box(world,{high.x+1,low.y,low.z-1},{high.x+1,high.y,high.z+1},Block::AgedPlanks);
        box(world,{low.x-1,high.y+1,low.z-1},{high.x+1,high.y+1,high.z+1},Block::Timber);
        for (const int x : {low.x-1,high.x+1}) for (const int z : {low.z-1,high.z+1})
            box(world,{x,low.y,z},{x,high.y,z},Block::Timber);
        for (int x=low.x+2;x<high.x;x+=4) {
            box(world,{x,low.y,low.z-1},{x,high.y,low.z-1},Block::Timber);
            box(world,{x,low.y,high.z+1},{x,high.y,high.z+1},Block::Timber);
        }
        // Wide door frames keep future furniture away from the approach.
        const int door=int(std::floor(room.entrance.x));
        box(world,{door-1,low.y,high.z+1},{door+2,low.y+5,high.z+1},Block::Air);
        box(world,{door-2,low.y,high.z+1},{door-2,low.y+6,high.z+1},Block::Timber);
        box(world,{door+3,low.y,high.z+1},{door+3,low.y+6,high.z+1},Block::Timber);
        box(world,{door-2,low.y+6,high.z+1},{door+3,low.y+6,high.z+1},Block::Timber);
        // An open reading window looks across the hall from either room.
        const int windowX=door<8?high.x-1:low.x+1;
        box(world,{windowX,low.y+2,high.z+1},{windowX+1,low.y+4,high.z+1},Block::Air);
        for (const int x : {low.x+1,high.x-1}) {
            world.set({x,low.y+4,low.z-1},Block::Lantern);
            world.set({x,low.y+5,low.z-1},Block::Timber);
        }
    }
}

void rootedSilhouette(World& world)
{
    // Buttress roots spread out from the trunk's corners. Every slice reaches
    // the island bed so the tree has weight at the water, with no floating toes.
    for (const auto direction : std::array<glm::ivec3,8>{{
        {-1,0,-1},{1,0,-1},{-1,0,1},{1,0,1},
        {-1,0,0},{1,0,0},{0,0,-1},{0,0,1}
    }}) {
        const bool diagonal=direction.x!=0 && direction.z!=0;
        const int side=diagonal?25:27;
        const glm::ivec3 center{8+direction.x*side,8,8+direction.z*side};
        // Cardinal routes remain open: their buttresses split to both sides.
        for (const int offset : (diagonal?std::array{-1,1}:std::array{-16,16})) {
            const glm::ivec3 across{-direction.z,0,direction.x};
            for (int run=0;run<=8;++run) {
                const auto p=center+direction*run+across*offset;
                const int radius=run<3?2:1;
                const int height=std::max(1,18-run*2);
                for (int dz=-radius;dz<=radius;++dz) for (int dx=-radius;dx<=radius;++dx) {
                    if (std::abs(dx)+std::abs(dz)>radius+1) continue;
                    const int x=p.x+dx,z=p.z+dz;
                    const bool roomInterior=std::any_of(Rooms.begin(),Rooms.end(),[&](const auto& room) {
                        return x>=room.minimum.x && x<=room.maximum.x && z>=room.minimum.z && z<=room.maximum.z;
                    });
                    if (roomInterior) continue;
                    const int top=8+std::max(1,height-std::abs(dx)-std::abs(dz));
                    box(world,{p.x+dx,1,p.z+dz},{p.x+dx,7,p.z+dz},Block::Stone);
                    box(world,{p.x+dx,8,p.z+dz},{p.x+dx,top,p.z+dz},
                        (dx+dz+run)%3==0?Block::RootWood:Block::Timber);
                }
            }
        }
    }
    // Recessed bark edges break up the long wall planes in silhouette. Place
    // them high enough to leave every ground-floor entrance untouched.
    for (const int z : {-10,0,12,24}) for (const int x : {-21,37}) {
        const int inward=x<8?1:-1;
        for (int y=30;y<=52;++y) {
            const int width=(y/6+z+48)%3==0?2:1;
            box(world,{std::min(x,x+inward*width),y,z},
                {std::max(x,x+inward*width),y,z+1},Block::RootWood);
        }
    }
    // The inner balcony arches carry the straight deck on stepped wooden
    // haunches, echoing the large doorway rather than a flat square rim.
    for (const int floor : {HeartwoodGalleryFloor,HeartwoodUpperFloor}) {
        for (const int z : {3,15,25}) for (int step=0;step<4;++step) {
            box(world,{-5+step,floor-5+step,z},{-5+step,floor-2,z},Block::RootWood);
            box(world,{20-step,floor-5+step,z},{20-step,floor-2,z},Block::RootWood);
        }
    }
}

void walkingClearance(World& world)
{
    // All joinery is in place before clearing the travelling volume. In
    // particular, a gallery corbel must never enter the stair below its deck.
    for (const auto flight : {glm::ivec3(0,HeartwoodFloor,27),glm::ivec3(0,HeartwoodGalleryFloor,11)}) {
        for (int step=0;step<13;++step) {
            const int z=flight.z-step,top=flight.y+1+step;
            box(world,{-15,top+1,z},{-9,top+4,z},Block::Air);
        }
        box(world,{-15,flight.y+1,flight.z+1},{-9,flight.y+4,flight.z+1},Block::Air);
        box(world,{-15,flight.y+14,flight.z-13},{-9,flight.y+17,flight.z-13},Block::Air);
    }
    box(world,{3,9,37},{13,15,39},Block::Air);
    box(world,{3,9,-23},{13,15,-21},Block::Air);
    box(world,{-23,9,3},{-21,15,13},Block::Air);
    box(world,{37,9,3},{39,15,13},Block::Air);
}

void rootGardens(World& world)
{
    for (const auto center : std::array<glm::ivec3,4>{{{-4,9,-1},{21,9,-1},{-4,9,21},{23,9,22}}}) {
        box(world,center-glm::ivec3(2,0,2),center+glm::ivec3(2,0,2),Block::MossBrick);
        box(world,center-glm::ivec3(1,0,1),center+glm::ivec3(1,0,1),Block::Moss);
    }
}

} // namespace

const HeartwoodLayout& heartwoodLayout() { return Layout; }
std::span<const HeartwoodRoom> heartwoodRooms() { return Rooms; }

void buildHeartwood(World& world)
{
    foundation(world);
    shell(world);
    sapWall(world);
    galleries(world);
    stairs(world);
    rooms(world);
    rootedSilhouette(world);
    rootGardens(world);
    walkingClearance(world);
}
} // namespace voxel
