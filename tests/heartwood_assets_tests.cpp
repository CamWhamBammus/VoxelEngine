#include "asset_collision.h"
#include "heartwood.h"
#include "heartwood_assets.h"
#include "player.h"
#include "scene.h"
#include "sections.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <queue>
#include <tuple>
#include <vector>
#include <string>

namespace {
using namespace voxel;
int failures=0;
void check(bool condition,const char* message)
{
    if (!condition) {std::cerr<<"FAIL: "<<message<<'\n';++failures;}
}

constexpr std::array Kinds{HubAssetKind::HeartSeed,HubAssetKind::ChainLantern,
    HubAssetKind::LibraryBookcase,HubAssetKind::ReadingDesk,HubAssetKind::ReadingSeat,
    HubAssetKind::SapCluster,HubAssetKind::RootCorbel,HubAssetKind::ReliefMapTable,
    HubAssetKind::SurveyInstrument,HubAssetKind::NurseryBed,HubAssetKind::PottingBench};

void modelGeometry()
{
    for (const auto kind:Kinds) for (int facing=0;facing<4;++facing) for (unsigned variant=0;variant<3;++variant) {
        const glm::vec3 position{17.5f,38,-12.5f};
        const auto mesh=buildHubAsset({kind,position,facing,variant});
        check(!mesh.indices.empty()&&mesh.indices.size()%3==0,"every Heartwood variant is a complete 3D mesh");
        bool finite=true,inBounds=true,paint=true,unit=true,indices=true,winding=true;
        for (const auto& vertex:mesh.vertices) {
            for (int axis=0;axis<3;++axis) {
                finite&=std::isfinite(vertex.position[axis])&&std::isfinite(vertex.normal[axis]);
                paint&=vertex.color[axis]>=0&&vertex.color[axis]<=1;
                inBounds&=std::abs(vertex.position[axis]-position[axis])<=HubAssetMargin;
            }
            unit&=std::abs(glm::length(vertex.normal)-1.f)<.0001f;
        }
        for (std::size_t i=0;i+2<mesh.indices.size();i+=3) {
            const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
            if (a>=mesh.vertices.size()||b>=mesh.vertices.size()||c>=mesh.vertices.size()) {indices=false;continue;}
            const auto normal=glm::cross(mesh.vertices[b].position-mesh.vertices[a].position,
                                         mesh.vertices[c].position-mesh.vertices[a].position);
            if (glm::length(normal)<.000001f) {winding=false;continue;}
            winding&=glm::dot(glm::normalize(normal),mesh.vertices[a].normal)>.999f;
        }
        check(finite&&unit&&paint,"furniture and crystals have valid painted flat normals");
        check(inBounds,"every prop stays within the renderer's chunk-culling margin");
        check(indices&&winding,"all triangles have visible, nondegenerate faces");
    }
    const auto seed=buildHubAsset({HubAssetKind::HeartSeed,{}});
    bool outward=true;
    for (const auto& vertex:seed.vertices) {
        if (vertex.material!=float(Block::Lantern)||std::abs(vertex.normal.y)>.999f) continue;
        outward&=glm::dot(glm::vec2(vertex.position.x,vertex.position.z),
                         glm::vec2(vertex.normal.x,vertex.normal.z))>0;
    }
    check(outward,"the amber seed's emissive facets face the room, not the inside of the crystal");
}

void catalogAndLighting()
{
    const auto catalog=sectionAssetCatalog();
    check(catalog.size()==HubAssetCount,"the section catalog retains every stable asset ID");
    for (const auto kind:Kinds) {
        const auto found=std::find_if(catalog.begin(),catalog.end(),[&](const auto& info){return info.kind==kind;});
        check(found!=catalog.end()&&!found->name.empty(),"new architectural props have usable creative-menu labels");
    }
    const glm::vec3 anchor{.5f,20,.5f};
    const std::array fixtures{HubAsset{HubAssetKind::HeartSeed,anchor},
        HubAsset{HubAssetKind::ChainLantern,anchor,0,2},HubAsset{HubAssetKind::SapCluster,anchor,1}};
    const auto dressing=buildAssetDressing(fixtures);
    check(dressing.lights.size()==3,"each luminous prop contributes exactly one real light source");
    if (dressing.lights.size()==3) {
        check(dressing.lights[0].position.y>anchor.y+2,"seed illumination comes from its amber core");
        check(std::abs(dressing.lights[1].position.y-(anchor.y-heartwoodLanternDrop(2)))<.0001f,
              "a longer lantern chain moves the light with the glass chamber");
        check(dressing.lights[2].position.x>anchor.x+.5f&&dressing.lights[2].position.z==anchor.z,
              "rotated sap lights sit outside their supporting wall");
    }
}

void furnitureCollision()
{
    AssetCollision collision;
    const std::array furniture{HubAsset{HubAssetKind::ReadingDesk,{}}};
    collision.rebuild(furniture);
    const auto hit=collision.raycast({0,3,0},{0,-1,0},4);
    check(hit&&hit->point.y>1.18f,"the open book and desktop are real collision surfaces");
    check(collision.sweep({-.25f,0,2},{.25f,1.8f,2.5f},2,-4)>-2.f,
          "a player cannot pass through a reading desk");
}

// Find the curving arrival avenue using real movement, including its stairs
// and every final prop. Each search state is a copy of an already-reachable
// player; no expansion teleports into an otherwise inaccessible landing.
bool walkFromVillage(Player& player,const World& world)
{
    const glm::vec3 destination{8.5f,9,39.5f};
    if (!player.setFeetPosition(world,VillageSpawn)||!player.grounded()) return false;
    struct Node { Player player; std::size_t parent; glm::vec3 step; };
    std::vector<Node> nodes{{player,0,{}}};
    using Key=std::tuple<int,int,int>;
    const auto key=[](glm::vec3 p) { return Key{int(std::round(p.x*2)),int(std::round(p.y*2)),int(std::round(p.z*2))}; };
    std::map<Key,std::size_t> visited{{key(player.feetPosition()),0}};
    using Candidate=std::pair<float,std::size_t>;
    std::priority_queue<Candidate,std::vector<Candidate>,std::greater<Candidate>> pending;
    pending.push({0,0});
    constexpr glm::vec3 steps[]{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1}};
    while (!pending.empty()) {
        const auto current=pending.top().second;
        pending.pop();
        const auto before=nodes[current].player.feetPosition();
        if (glm::distance(before,destination)<.01f) {
            std::vector<glm::vec3> route;
            for (auto n=current;n!=0;n=nodes[n].parent) route.push_back(nodes[n].step);
            for (auto it=route.rbegin();it!=route.rend();++it) {
                player.move(world,*it);
                player.move(world,{0,-1,0});
            }
            return player.grounded()&&glm::distance(player.feetPosition(),destination)<.01f;
        }
        for (const auto step:steps) {
            Player next=nodes[current].player;
            next.move(world,step);
            next.move(world,{0,-1,0});
            const auto after=next.feetPosition();
            if (!next.grounded()||std::abs(after.x-before.x-step.x)>.01f
                ||std::abs(after.z-before.z-step.z)>.01f||std::abs(after.y-before.y)>1.01f
                ||after.x<-24||after.x>25||after.z<39||after.z>82||after.y<8||after.y>26
                ||visited.contains(key(after))) continue;
            const auto index=nodes.size();
            nodes.push_back({next,current,step});
            visited.emplace(key(after),index);
            const auto delta=glm::abs(after-destination);
            pending.push({delta.x+delta.y+delta.z,index});
        }
    }
    return false;
}

void hallFurnishing()
{
    World world;
    check(planHeartwoodAssets(world).empty(),"empty and custom worlds do not receive floating village furniture");
    buildUndergroundScene(world);
    const auto planned=planHeartwoodAssets(world);
    const auto repeated=planHeartwoodAssets(world);
    check(planned==repeated,"the furnishing plan is deterministic");
    const auto count=[&](HubAssetKind kind) {
        return std::count_if(planned.begin(),planned.end(),[&](const auto& asset){return asset.kind==kind;});
    };
    check(count(HubAssetKind::HeartSeed)==1,"there is one centerpiece at the skill-journal station");
    check(count(HubAssetKind::LibraryBookcase)>=4,"the library has its own substantial collection of modeled shelves");
    check(count(HubAssetKind::ReliefMapTable)==1&&count(HubAssetKind::SurveyInstrument)==1,
          "the expedition loft has a relief map and a surveying instrument");
    check(count(HubAssetKind::NurseryBed)>=2&&count(HubAssetKind::PottingBench)==1,
          "the conservatory has planted nursery beds and a potting bench");
    for (const auto& asset:planned) {
        const auto inRoom=[&](HeartwoodRoomKind kind) {
            const auto rooms=heartwoodRooms();
            const auto found=std::find_if(rooms.begin(),rooms.end(),[&](const auto& room) { return room.kind==kind; });
            if (found==rooms.end()) return false;
            const auto& room=*found;
            return asset.position.x>=room.minimum.x&&asset.position.x<=room.maximum.x+1
                &&asset.position.y>=room.minimum.y&&asset.position.y<=room.maximum.y+1
                &&asset.position.z>=room.minimum.z&&asset.position.z<=room.maximum.z+1;
        };
        if (asset.kind==HubAssetKind::LibraryBookcase)
            check(inRoom(HeartwoodRoomKind::Library),"bookcases belong to the library rather than repeated room copies");
        if (asset.kind==HubAssetKind::ReliefMapTable||asset.kind==HubAssetKind::SurveyInstrument)
            check(inRoom(HeartwoodRoomKind::ExpeditionLoft),"expedition furniture stays in the expedition loft");
        if (asset.kind==HubAssetKind::NurseryBed||asset.kind==HubAssetKind::PottingBench)
            check(inRoom(HeartwoodRoomKind::Conservatory),"plant-growing furniture belongs to the conservatory");
    }
    check(count(HubAssetKind::ChainLantern)>=16,"supported pendant lighting spans multiple levels");
    check(count(HubAssetKind::SapCluster)>=5,"raised cyan sap connects the lower hall to its canopy");
    check(count(HubAssetKind::RootCorbel)>=4,"carved supports attach beneath the galleries");
    check(count(HubAssetKind::TimberFence)>=30,"fine railings join the gallery posts");
    for (const auto& asset:planned) {
        if (asset.kind==HubAssetKind::ChainLantern) {
            const auto cell=glm::ivec3(glm::floor(asset.position+glm::vec3(0,.05f,0)));
            check(isSolid(world.get(cell)),"every hanging lamp meets an actual overhead beam");
        }
    }
    AssetCollision collision;
    collision.rebuild(planned);
    const auto open=[&](glm::vec3 feet) {
        return !collision.overlaps(feet+glm::vec3(-.3f,.02f,-.3f),feet+glm::vec3(.3f,1.8f,.3f));
    };
    check(open(heartwoodLayout().approach),"the seed can be approached without crossing a prop");
    for (const auto& room:heartwoodRooms())
        check(open(room.entrance),"bookcases and chairs leave upstairs doorways open");

    // Exercise the actual movement sweeps against every final village model,
    // not merely the new furniture or a single overlap check at the landings.
    auto allCollision=std::make_shared<AssetCollision>();
    allCollision->rebuild(planHubAssets(world));
    Player player;
    player.setAssetCollision(allCollision);
    const auto walk=[&](glm::vec3 step,int count) {
        for (int i=0;i<count;++i) {
            const auto before=player.feetPosition();
            player.move(world,step);
            player.move(world,{0,-1,0});
            const auto after=player.feetPosition();
            if (!player.grounded()||std::abs(after.x-before.x-step.x)>.01f
                ||std::abs(after.z-before.z-step.z)>.01f||std::abs(after.y-before.y)>1.01f) {
                std::cerr<<"Furnished route blocked at "<<before.x<<','<<before.y<<','<<before.z<<'\n';
                return false;
            }
        }
        return true;
    };
    const auto walkTo=[&](glm::vec3 target) {
        const auto before=player.feetPosition();
        const auto delta=target-before;
        if ((std::abs(delta.x)>.01f)&&(std::abs(delta.z)>.01f)) return false;
        const glm::vec3 step{glm::sign(delta.x),0,glm::sign(delta.z)};
        const int count=int(std::round(std::max(std::abs(delta.x),std::abs(delta.z))));
        return walk(step,count)&&glm::distance(player.feetPosition(),target)<.01f;
    };
    // One uninterrupted tour catches the old failure: each individual stair
    // worked after teleporting, but rooms, walls and guards blocked its exits.
    bool tour=walkFromVillage(player,world);
    check(tour,"the arrival avenue and south bridge reach the hall with final scenery collision");
    const glm::vec3 tourStops[]{
        glm::vec3{8.5f,9,28.5f},{-12.5f,9,28.5f},{-12.5f,22,14.5f},
        {-17.5f,22,14.5f},{-17.5f,22,-3.5f},{-2.5f,22,-3.5f},
        {-2.5f,22,-7.5f},{-2.5f,22,-3.5f},{-2.5f,22,-.5f},
        {26.5f,22,-.5f},{26.5f,22,-5.5f},{26.5f,22,-.5f},
        {-17.5f,22,-.5f},{-17.5f,22,12.5f},{-12.5f,22,12.5f},
        {-12.5f,35,-3.5f},{23.5f,35,-3.5f},{23.5f,35,20.5f},
        {28.5f,35,20.5f},{28.5f,35,16.5f},{28.5f,35,20.5f},
        {23.5f,35,20.5f},{23.5f,35,-3.5f},{-12.5f,35,-3.5f},
        {-12.5f,22,12.5f},{-12.5f,22,14.5f},{-12.5f,9,28.5f},
        {8.5f,9,28.5f},{8.5f,9,39.5f}
    };
    for (const auto target:tourStops) {
        if (!tour) break;
        tour=walkTo(target);
        if (!tour) std::cerr<<"Continuous village tour could not reach "
                           <<target.x<<','<<target.y<<','<<target.z<<'\n';
    }
    check(tour,"a single grounded walk visits all three distinct rooms and returns via both flights");

    for (int lane=-15;lane<=-9;++lane) {
        const float x=float(lane)+.5f;
        check(player.setFeetPosition(world,{x,9,28.5f})&&walk({0,0,-1},14)
            &&std::abs(player.feetPosition().y-22)<.01f,
            "full asset collision leaves every main-stair lane climbable");
        check(walk({0,0,1},14)&&std::abs(player.feetPosition().y-9)<.01f,
            "main stairs remain descendable after adding rails and foliage");
        check(player.setFeetPosition(world,{x,22,12.5f})&&walk({0,0,-1},15)
            &&std::abs(player.feetPosition().y-35)<.01f,
            "the upper staircase stays clear with all final village models");
        check(walk({0,0,1},15)&&std::abs(player.feetPosition().y-22)<.01f,
            "players can descend the upper staircase past the new decorations");
    }
    for (const auto& room:heartwoodRooms()) {
        check(player.setFeetPosition(world,room.entrance)&&walk({0,0,-1},4)&&walk({0,0,1},4),
            "players can enter and leave each furnished room without jumping or squeezing");
    }

    // Deleting a lamp's beam cannot leave its chain suspended in an edited hub.
    const auto lamp=std::find_if(planned.begin(),planned.end(),[](const auto& asset){return asset.kind==HubAssetKind::ChainLantern;});
    if (lamp!=planned.end()) {
        world.set(glm::ivec3(glm::floor(lamp->position+glm::vec3(0,.05f,0))),Block::Air);
        const auto edited=planHeartwoodAssets(world);
        check(std::none_of(edited.begin(),edited.end(),[&](const auto& asset){
            return asset.kind==HubAssetKind::ChainLantern&&asset.position==lamp->position;
        }),"unsupported pendant fixtures disappear with their support");
    }
}
} // namespace

int main()
{
    modelGeometry();
    catalogAndLighting();
    furnitureCollision();
    hallFurnishing();
    return failures?1:0;
}
