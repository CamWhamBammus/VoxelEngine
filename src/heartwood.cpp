#include "heartwood.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace voxel {
namespace {
constexpr HeartwoodLayout Layout{
    {8.5f,9,11.5f}, {8.5f,11.0f,11.5f}, {8.5f,9,16.5f},
    {8.5f,11.5f,39.5f}, {6.5f,22.0f,0.5f}
};
constexpr std::array<HeartwoodRoom,3> Rooms{{
    {HeartwoodRoomKind::Library, "Root Library", {-6,22,-16}, {2,29,-7}, {-2.5f,22,-3.5f}},
    {HeartwoodRoomKind::ExpeditionLoft, "Expedition Loft", {20,22,-16}, {32,29,-5}, {26.5f,22,-1.5f}},
    {HeartwoodRoomKind::Conservatory, "Canopy Conservatory", {25,35,1}, {33,43,17}, {28.5f,35,20.5f}}
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
        if (footprint(x,z)) box(world,{x,9,z},{x,77,z},Block::Air);
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
    // Broad living buttresses below become a narrower bole above the galleries.
    // Vertical bark ribs replace the horizontal bands that made the old hall
    // read as a square building with a flat roof.
    for (int z=-20;z<=36;++z) for (int x=-20;x<=36;++x) {
        if (!footprint(x,z)) continue;
        for (int y=9;y<=49;++y) {
            const int taper=y>42?2:y>30?1:0;
            const int rib=(std::abs(x*7+z*3)%9<2)?1:0;
            if (!footprint(x,z,taper) || footprint(x,z,taper+2+rib)) continue;
            const bool bark=(std::abs(x*3-z*5+(y/13))%7<2);
            const auto material=bark?Block::RootWood:Block::Timber;
            world.set({x,y,z},material);
            // At a chamfer, a deeper bark rib can otherwise touch the shell
            // only diagonally. Bond its inner edge to the solid wall behind it.
            if (rib && footprint(x,z,taper+2)) {
                const bool alongX=std::abs(x-8)>=std::abs(z-8);
                world.set({x+(alongX?(x<8?-1:1):0),y,
                           z+(alongX?0:(z<8?-1:1))},material);
            }
        }
    }
    // Tall open arches frame the water and districts on all four sides.
    box(world,{-11,9,30},{27,43,37},Block::Air);
    box(world,{30,9,-1},{37,35,27},Block::Air);
    box(world,{-21,9,1},{-15,19,15},Block::Air);
    box(world,{3,9,-21},{13,19,-15},Block::Air);
    for (int step=0;step<8;++step) {
        box(world,{-11+step,35+step,31},{-11+step,44,34},Block::Timber);
        box(world,{27-step,35+step,31},{27-step,44,34},Block::Timber);
        box(world,{31,28+step,-1+step},{34,36,-1+step},Block::Timber);
        box(world,{31,28+step,27-step},{34,36,27-step},Block::Timber);
    }
    for (const auto p : std::array<glm::ivec3,6>{{
        {-17,0,-17},{3,0,-17},{18,0,-17},{33,0,-17},
        {-17,0,28},{33,0,28}
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
    // Open timber ribs carry the lanterns. Above them the trunk continues into
    // a tall irregular vault instead of ending at the gallery roofline.
    for (const int x : {-12,8,28}) box(world,{x,48,-18},{x+1,49,34},Block::Timber);
    for (const int z : {-13,9,29}) box(world,{-18,48,z},{34,49,z+1},Block::Timber);
    for (int y=50;y<=76;++y) {
        const float rise=float(y-50)/26.f;
        const float centerX=8.f+2.f*rise,centerZ=8.f-rise;
        const float radiusX=27.f-13.f*rise,radiusZ=26.f-12.f*rise;
        for (int z=-21;z<=37;++z) for (int x=-21;x<=37;++x) {
            const float dx=(float(x)-centerX)/radiusX,dz=(float(z)-centerZ)/radiusZ;
            const float angle=std::atan2(dz,dx);
            const float bark=.035f*std::sin(angle*7.f+rise*.8f);
            const float distance=std::sqrt(dx*dx+dz*dz);
            if (distance>1.f+bark || distance<.83f+bark) continue;
            const bool seam=std::sin(angle*11.f+rise*1.8f)>.6f;
            world.set({x,y,z},seam?Block::RootWood:Block::Timber);
        }
    }
    // Branch-shaped haunches rise toward the neck, visible through the atrium.
    for (const int side : {-1,1}) for (int rise=0;rise<=20;++rise) {
        const int x=8+side*(23-rise/2);
        const int z=8+side*(21-rise/2);
        box(world,{x-1,48+rise,-10},{x+1,49+rise,-8},Block::RootWood);
        box(world,{-2,48+rise,z-1},{0,49+rise,z+1},Block::RootWood);
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
            const bool ring=x<=-6 || x>=21 || z<=-5;
            const bool porch=z<=0;
            if (!ring && !porch) continue;
            world.set({x,floor,z},Block::AgedPlanks);
            world.set({x,floor-1,z},Block::Timber);
        }
        // Deep inner fascias and corbel steps expose real depth below the rail.
        for (const int x : {-6,21}) box(world,{x,floor-2,1},{x,floor-1,26},Block::Timber);
        for (const int x : {-6,21}) for (const int z : {3,9,15,21,25}) {
            box(world,{x,floor+1,z},{x,floor+2,z},Block::Timber);
            if (z==3||z==15||z==25) world.set({x,floor+3,z},Block::Lantern);
            for (int drop=1;drop<=3;++drop) {
                const int inward=x==-6?-1:1;
                const int first=x+inward*drop,last=x+inward*(drop+1);
                box(world,{std::min(first,last),floor-2-drop,z},{std::max(first,last),floor-1,z},Block::Timber);
            }
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
    // Rooms occupy alcoves around the promenade. Their whole fronts remain
    // open; neither flight ends at a room wall or requires threading a door.
    for (const auto& room : Rooms) {
        const auto low=room.minimum,high=room.maximum;
        const int floor=low.y-1;
        box(world,{low.x-1,floor,low.z-1},{high.x+1,floor,high.z+1},Block::AgedPlanks);
        box(world,low,high,Block::Air);
        box(world,{low.x,low.y,high.z+1},{high.x,high.y,high.z+1},Block::Air);
        if (room.kind==HeartwoodRoomKind::Library) {
            // A recessed, low timber reading alcove with a curved lintel and
            // one solid side for books, opening onto the broad landing.
            box(world,{low.x-1,low.y,low.z-1},{high.x+1,high.y,low.z-1},Block::AgedPlanks);
            box(world,{low.x-1,low.y,low.z-1},{low.x-1,high.y,high.z},Block::RootWood);
            box(world,{high.x+1,low.y,low.z-1},{high.x+1,high.y,high.z},Block::RootWood);
            box(world,{low.x-1,high.y+1,low.z-1},{high.x+1,high.y+1,high.z+1},Block::Timber);
            for (int step=0;step<3;++step) {
                box(world,{low.x+step,low.y+5+step,high.z+1},
                    {low.x+step,high.y+1,high.z+1},Block::RootWood);
                box(world,{high.x-step,low.y+5+step,high.z+1},
                    {high.x-step,high.y+1,high.z+1},Block::RootWood);
            }
            for (const int x : {low.x+1,high.x-1}) world.set({x,low.y+5,low.z-1},Block::Lantern);
        } else if (room.kind==HeartwoodRoomKind::ExpeditionLoft) {
            // A wide map loft uses a pitched awning, exposed rafters and open
            // side windows. It has a different outline from the reading nook.
            box(world,{low.x-1,low.y,low.z-1},{high.x+1,high.y,low.z-1},Block::RootWood);
            for (const int x : {low.x-1,high.x+1}) {
                box(world,{x,low.y,low.z},{x,low.y+1,high.z},Block::MossBrick);
                for (const int z : {low.z,high.z}) box(world,{x,low.y,z},{x,high.y+1,z},Block::Timber);
            }
            const int center=(low.x+high.x)/2;
            for (int x=low.x-1;x<=high.x+1;++x) {
                const int top=high.y+1+(6-std::min(6,std::abs(x-center)))/3;
                box(world,{x,top,low.z-1},{x,top,high.z+1},Block::AgedPlanks);
            }
            box(world,{center,high.y+1,low.z-1},{center,high.y+1,high.z+1},Block::Timber);
            for (const int z : {low.z+1,high.z-1})
                box(world,{low.x-1,high.y+1,z},{high.x+1,high.y+1,z},Block::Timber);
            world.set({low.x+1,low.y+5,low.z-1},Block::Lantern);
            world.set({high.x-1,low.y+5,low.z-1},Block::Lantern);
            // The survey telescope looks through the full thickness of the
            // trunk, across the water toward the eastern village terraces.
            box(world,{high.x+1,low.y+2,low.z+4},{37,high.y-1,high.z-1},Block::Air);
            for (const int z : {low.z+3,high.z})
                box(world,{high.x+1,low.y,z},{high.x+1,high.y+1,z},Block::Timber);
            box(world,{high.x+1,low.y+1,low.z+3},{high.x+1,low.y+1,high.z},Block::Timber);
            box(world,{high.x+1,high.y,low.z+3},{high.x+1,high.y,high.z},Block::Timber);
        } else {
            // The upper garden is a screened pergola beside an open tree
            // window, not another enclosed study stacked over the library.
            box(world,{low.x-1,low.y,low.z-1},{high.x+1,high.y,high.z+1},Block::Air);
            box(world,{high.x+1,low.y+2,low.z+1},{37,high.y-1,high.z-1},Block::Air);
            for (const int x : {low.x-1,high.x+1}) {
                for (const int z : {low.z-1,high.z+1})
                    box(world,{x,low.y,z},{x,high.y+1,z},Block::RootWood);
                box(world,{x,high.y+1,low.z-1},{x,high.y+1,high.z+1},Block::Timber);
            }
            for (int z=low.z-1;z<=high.z+1;z+=3)
                box(world,{low.x-1,high.y+1,z},{high.x+1,high.y+1,z},Block::Timber);
            // A long central beam anchors pendant lamps and trailing plants.
            const int center=(low.x+high.x)/2;
            box(world,{center,high.y+1,low.z-1},{center,high.y+1,high.z+1},Block::Timber);
            box(world,{high.x,low.y,low.z},{high.x,low.y,high.z},Block::MossBrick);
            box(world,{low.x,low.y,low.z},{high.x,low.y,low.z},Block::MossBrick);
            // The window lamps hang from short brackets on full timber posts.
            // Their supports continue into the gallery and pergola frame.
            for (const int z : {low.z+2,high.z-2}) {
                box(world,{high.x+2,floor,z},{high.x+2,high.y+1,z},Block::Timber);
                world.set({high.x+1,low.y+5,z},Block::Timber);
                world.set({high.x+1,low.y+4,z},Block::Lantern);
            }
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
    // A continuous western promenade passes beside the rising second flight.
    // Its broad landing also lets players turn toward the open library front.
    box(world,{-19,21,-7},{-17,21,14},Block::AgedPlanks);
    box(world,{-19,22,-7},{-17,28,14},Block::Air);
    box(world,{-19,21,-4},{32,21,0},Block::AgedPlanks);
    box(world,{-19,22,-4},{32,28,0},Block::Air);
    // The upper flight emerges into a wide balcony, with the garden reached
    // along a separate promenade instead of walking through another room.
    box(world,{-16,34,-4},{32,34,-2},Block::AgedPlanks);
    box(world,{-8,34,-1},{32,34,0},Block::AgedPlanks);
    box(world,{-16,35,-4},{32,41,0},Block::Air);
    box(world,{22,34,0},{23,34,21},Block::AgedPlanks);
    box(world,{22,35,0},{23,41,21},Block::Air);
    box(world,{22,34,19},{33,34,22},Block::AgedPlanks);
    box(world,{22,35,19},{33,41,22},Block::Air);
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
    // Broad banks beside the approaches give the lush planting pass enough
    // real soil for tree ferns, small trees, and mushrooms outside the hall.
    for (const auto center : std::array<glm::ivec3,6>{{
        {-29,8,1},{-29,8,19},{46,8,1},{46,8,19},{-5,8,-29},{19,8,-29}
    }}) {
        for (int dz=-4;dz<=4;++dz) for (int dx=-4;dx<=4;++dx) {
            const int edge=std::max(std::abs(dx),std::abs(dz));
            if (edge<3 || std::abs(dx)+std::abs(dz)>6) continue;
            const auto p=center+glm::ivec3(dx,0,dz);
            if (isSolid(world.get(p))) continue;
            const int top=edge==3?7:6;
            box(world,{p.x,1,p.z},{p.x,top-1,p.z},Block::Stone);
            world.set({p.x,top,p.z},Block::MossStone);
        }
        box(world,center-glm::ivec3(2,7,2),center+glm::ivec3(2,-1,2),Block::Stone);
        box(world,center-glm::ivec3(2,0,2),center+glm::ivec3(2,0,2),Block::Grass);
        box(world,center+glm::ivec3(-2,1,-2),center+glm::ivec3(2,8,2),Block::Air);
    }
}

void groundLighting(World& world)
{
    const auto mounted=[&](glm::ivec3 cell,glm::ivec3 behind) {
        if (isSolid(world.get(cell+behind))) world.set(cell,Block::Lantern);
    };
    // Low lamps sit just above headroom, where their warm pool reaches the
    // timber and paving. High pendants still describe the atrium's full height.
    for (const int z : {-10,3,20}) {
        mounted({-17,14,z},{-1,0,0});
        mounted({33,14,z},{1,0,0});
    }
    for (const int x : {-9,25}) mounted({x,14,-17},{0,0,-1});
    mounted({-15,14,28},{-1,0,0});
    mounted({31,14,28},{1,0,0});
    mounted({33,14,-15},{0,0,-1});
    for (const int x : {-8,24}) {
        mounted({x,15,38},{0,0,-1});
        mounted({x,25,36},{0,0,-1});
    }
    for (const int x : {-10,26}) mounted({x,39,35},{0,0,-1});
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
    groundLighting(world);
}
} // namespace voxel
