#include "root_crossings.h"
#include "player.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <glm/geometric.hpp>

namespace {
int failures=0;
void check(bool passed, const std::string& message)
{
    if (!passed) { std::cerr<<"FAIL: "<<message<<'\n'; ++failures; }
}

bool supported(const voxel::World& world, glm::vec3 p)
{
    // Footprint contact, including the body's half width near a root's edge.
    for (const float z : {-.28f,0.0f,.28f}) for (const float x : {-.28f,0.0f,.28f})
        if (voxel::isSolid(world.get(glm::ivec3(glm::floor(p+glm::vec3(x,-.02f,z)))))) return true;
    return false;
}

bool traverse(const voxel::World& world, const voxel::RootCrossingCourse& course,
              float frameTime, int& jumps, std::string& problem, bool jumping=true)
{
    voxel::Player player;
    if (!player.setFeetPosition(world,course.spawn) || !player.grounded()) {
        problem="invalid spawn"; return false;
    }
    jumps=0;
    for (std::size_t step=1;step<course.route.size();++step) {
        const auto target=course.route[step];
        bool arrived=false;
        for (int frame=0;frame<2000;++frame) {
            const auto feet=player.feetPosition();
            const glm::vec3 offset{target.x-feet.x,0,target.z-feet.z};
            const float distance=glm::length(offset);
            if (distance<.035f && std::abs(feet.y-target.y)<.04f && player.grounded()) { arrived=true; break; }
            if (feet.y<course.resetY) break;
            const auto direction=distance>.001f?offset/distance:glm::vec3(0,0,-1);
            const bool jump=jumping && player.grounded() && !supported(world,feet+direction*.65f);
            if (jump) ++jumps;
            const float movement=std::min(1.0f,distance/(4.5f*frameTime));
            player.update(world,{0,movement},direction,jump,false,frameTime);
        }
        if (!arrived) {
            const auto p=player.feetPosition();
            problem="segment "+std::to_string(step)+" stopped at "+std::to_string(p.x)+","+
                std::to_string(p.y)+","+std::to_string(p.z)+" target "+std::to_string(target.x)+","+
                std::to_string(target.y)+","+std::to_string(target.z);
            return false;
        }
    }
    return true;
}

std::uint64_t fingerprint(const voxel::World& world)
{
    std::uint64_t value=1469598103934665603ull;
    for (int y=2;y<=47;++y) for (int z=-39;z<=39;++z) for (int x=-35;x<=35;++x) {
        value^=static_cast<unsigned>(world.get({x,y,z})); value*=1099511628211ull;
    }
    return value;
}

void checkRootedLandings(const voxel::World& world, const voxel::RootCrossingCourse& course,
                        const std::string& name)
{
    constexpr int width=71, depth=79, height=46;
    const auto offset=[](glm::ivec3 p) { return p.x+35+width*(p.z+39+depth*(p.y-2)); };
    const auto inside=[](glm::ivec3 p) { return p.x>=-35 && p.x<=35 && p.z>=-39 && p.z<=39 && p.y>=2 && p.y<=47; };
    std::vector<bool> connected(width*depth*height,false);
    std::queue<glm::ivec3> pending;
    pending.push({0,2,0});
    connected[offset({0,2,0})]=true;
    constexpr std::array<glm::ivec3,6> neighbors{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
    while (!pending.empty()) {
        const auto p=pending.front(); pending.pop();
        for (const auto delta : neighbors) {
            const auto next=p+delta;
            if (!inside(next) || connected[offset(next)] || !voxel::isSolid(world.get(next))) continue;
            connected[offset(next)]=true; pending.push(next);
        }
    }
    for (const auto feet : course.route) {
        const auto block=glm::ivec3(glm::floor(feet-glm::vec3(0,.1f,0)));
        check(inside(block) && connected[offset(block)],name+" root landing is physically joined to cave terrain");
    }
}
} // namespace

int main()
{
    using namespace voxel;
    const auto catalog=rootCrossingCatalog();
    check(catalog.size()==10,"ten named course choices are available");
    std::set<std::string_view> names;
    std::set<std::uint64_t> layouts;
    World world(1729);
    for (int index=0;index<RootCrossingCount;++index) {
        const auto info=catalog[index];
        const std::string name(info.name);
        check(!info.name.empty() && !info.description.empty() && !info.difficulty.empty(),"catalog entries describe each course");
        check(names.insert(info.name).second,"course names are unique");
        world.set({100,70,100},Block::Brick);
        const auto course=buildRootCrossing(world,index);
        check(world.seed()==1729,"course construction preserves the world seed");
        check(world.get({100,70,100})==Block::Air,"course transition removes the previous scene outside the room");
        check(course.route.front()==course.spawn && course.route.back()==course.finish,"route contains entrance and finish");
        check(std::find(course.route.begin(),course.route.end(),course.checkpoint)!=course.route.end(),"route visits its checkpoint");
        for (const auto p : {course.spawn,course.checkpoint,course.finish}) {
            Player player;
            check(player.setFeetPosition(world,p) && player.grounded(),name+" safe landing fits a grounded player");
            for (int z=-2;z<=2;++z) for (int x=-2;x<=2;++x) {
                check(player.setFeetPosition(world,p+glm::vec3(x,0,z)) && player.grounded(),name+" landing has a clear five-block footprint");
            }
            check(p.y>course.resetY+3,name+" reset plane is safely below its landings");
        }
        for (const float dt : {1.0f/60.0f,1.0f/30.0f}) {
            int jumps=0; std::string problem;
            const bool crossed=traverse(world,course,dt,jumps,problem);
            check(crossed,name+" actual player walking/jumping traversal: "+problem);
            check(jumps>0,name+" route contains a required jump");
        }
        int jumps=0; std::string problem;
        check(!traverse(world,course,1.0f/60.0f,jumps,problem,false),name+" walking without jumping falls at a real gap");
        checkRootedLandings(world,course,name);
        if (index==2) {
            auto fork=course;
            fork.route={course.spawn,{.5f,18,22.5f},{13.5f,18,9.5f},{13.5f,18,-9.5f},
                        {.5f,18,-23.5f},course.finish};
            check(traverse(world,fork,1.0f/60.0f,jumps,problem),"Forked Grove's optional east route is also physically playable: "+problem);
        }
        const auto hash=fingerprint(world);
        check(layouts.insert(hash).second,name+" produces a distinct world");
        const auto second=buildRootCrossing(world,index);
        check(hash==fingerprint(world) && second.route==course.route,name+" rebuild is deterministic");
        check(world.get({0,7,27})==Block::Water,"crossing has water below its raised route");
    }
    const auto before=world;
    for (const int invalid : {-1,RootCrossingCount}) {
        bool rejected=false;
        try { (void)buildRootCrossing(world,invalid); } catch (const std::out_of_range&) { rejected=true; }
        check(rejected && world.sameContents(before),"invalid course indices reject before mutation");
    }
    if (failures) return 1;
    std::cout<<"Root Crossing course tests passed\n";
}
