#include "heartwood.h"
#include "editing.h"
#include "player.h"
#include "world.h"

#include <cmath>
#include <iostream>
#include <string>
#include <glm/geometric.hpp>

namespace {
int failures=0;
void check(bool value,const std::string& message)
{
    if (!value) { ++failures; std::cerr<<"FAIL: "<<message<<'\n'; }
}

bool walk(voxel::Player& player,const voxel::World& world,glm::vec3 step,int count)
{
    for (int i=0;i<count;++i) {
        const auto before=player.feetPosition();
        player.move(world,step);
        player.move(world,{0,-1,0});
        const auto after=player.feetPosition();
        if (!player.grounded() || std::abs(after.x-before.x-step.x)>.01f
            || std::abs(after.z-before.z-step.z)>.01f || std::abs(after.y-before.y)>1.01f) {
            std::cerr<<"Walk blocked from "<<before.x<<','<<before.y<<','<<before.z
                     <<" to "<<after.x<<','<<after.y<<','<<after.z<<'\n';
            return false;
        }
    }
    return true;
}
}

int main()
{
    using namespace voxel;
    World world;
    buildHeartwood(world);
    Player player;

    // This is a room-sized building, not the former solid trunk in a small ring.
    int openFloor=0;
    for (int z=-15;z<=30;++z) for (int x=-15;x<=30;++x) {
        if (isSolid(world.get({x,8,z})) && world.get({x,9,z})==Block::Air
            && world.get({x,10,z})==Block::Air) ++openFloor;
    }
    check(openFloor>1200,"the hollow tree provides a broad usable ground-floor hall");
    check(world.get({8,16,22})==Block::Air && world.get({8,40,15})==Block::Air,
          "the shrine and balcony overlook share a tall open atrium");
    for (const auto start : {glm::vec3(8.5f,9,38.5f),{38.5f,9,8.5f},{-22.5f,9,8.5f},{8.5f,9,-22.5f}})
        check(player.setFeetPosition(world,start)&&player.grounded(),"each district entrance has a supported landing");

    const auto& layout=heartwoodLayout();
    check(player.setFeetPosition(world,layout.approach)&&player.grounded(),"the seed can be approached at standing height");
    const auto sight=layout.station-player.eyePosition();
    check(glm::length(sight)<6.5f && !raycast(player.eyePosition(),sight,glm::length(sight),
          [&](glm::ivec3 cell) { return world.get(cell); }),"the seed interaction ray is unobstructed");

    for (int lane=-15;lane<=-9;++lane) {
        check(player.setFeetPosition(world,{lane+.5f,9,28.5f})&&player.grounded(),"main stair foot has room for the player");
        const bool climbed=walk(player,world,{0,0,-1},14);
        check(climbed && std::abs(player.feetPosition().y-22)<.01f,"every main stair lane reaches the first gallery without jumping");
        check(walk(player,world,{0,0,1},14)&&std::abs(player.feetPosition().y-9)<.01f,
              "the main stairs can also be descended without a drop");
        check(player.setFeetPosition(world,{lane+.5f,22,12.5f})&&player.grounded(),"upper stair foot meets the gallery");
        check(walk(player,world,{0,0,-1},15)&&std::abs(player.feetPosition().y-35)<.01f,
              "the second stair reaches the archive landing");
    }
    check(heartwoodRooms().size()==3,"the tree has a library, study, and upstairs archive");
    for (const auto& room : heartwoodRooms()) {
        check(player.setFeetPosition(world,room.entrance)&&player.grounded(),std::string(room.name)+" has an open, supported entrance");
        check(walk(player,world,{0,0,-1},4),std::string(room.name)+" can be entered through its actual doorway");
        const glm::ivec3 center=(room.minimum+room.maximum)/2;
        check(world.get({center.x,room.minimum.y-1,center.z})==Block::AgedPlanks,
              std::string(room.name)+" has a finished timber floor");
        check(world.get({center.x,room.maximum.y+1,center.z})==Block::Timber,
              std::string(room.name)+" has a supported room ceiling");
    }
    return failures?1:0;
}
