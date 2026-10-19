#include "hub_lush.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace {
using namespace voxel;
int failures=0;
void check(bool value,const std::string& message)
{
    if (!value) { std::cerr<<"FAIL: "<<message<<'\n'; ++failures; }
}
struct Bounds { glm::vec3 low,high; };
Bounds bounds(const HubAsset& asset)
{
    Bounds result{glm::vec3(std::numeric_limits<float>::max()),glm::vec3(std::numeric_limits<float>::lowest())};
    for (const auto& vertex : buildHubAsset(asset).vertices) {
        result.low=glm::min(result.low,vertex.position);
        result.high=glm::max(result.high,vertex.position);
    }
    return result;
}
bool overlap(Bounds a,Bounds b)
{
    return a.low.x<b.high.x && a.high.x>b.low.x && a.low.y<b.high.y && a.high.y>b.low.y
        && a.low.z<b.high.z && a.high.z>b.low.z;
}
bool smallPlant(HubAssetKind kind)
{
    return kind==HubAssetKind::Fern || kind==HubAssetKind::Flowers || kind==HubAssetKind::Mushrooms
        || kind==HubAssetKind::Reeds || kind==HubAssetKind::Shrub || kind==HubAssetKind::BroadleafClump
        || kind==HubAssetKind::FernBed || kind==HubAssetKind::MossGarden;
}
bool ground(HubAssetKind kind)
{
    switch (kind) {
    case HubAssetKind::HangingRoots: case HubAssetKind::BlossomVines: case HubAssetKind::ShelfFungi:
    case HubAssetKind::CaveLotus: case HubAssetKind::WaterGrass: case HubAssetKind::RootFan:
    case HubAssetKind::HangingPlanter: return false;
    default: return true;
    }
}
glm::ivec3 cell(glm::vec3 p) { return glm::ivec3(glm::floor(p)); }
bool samePlacement(const std::vector<HubAsset>& plan,const HubAsset& asset)
{
    return std::find(plan.begin(),plan.end(),asset)!=plan.end();
}
} // namespace

int main()
{
    World world(1337);
    buildUndergroundScene(world);
    const auto full=planHubAssets(world);
    std::vector<HubAsset> older;
    for (const auto& asset : full) if (asset.kind<HubAssetKind::TreeFern) older.push_back(asset);
    const auto before=world;
    const auto lush=planLushAssets(world,older);
    check(lush.size()>=70 && lush.size()<=240,"the real village has a substantial bounded new garden layer");
    check(lush==planLushAssets(world,older),"lush placement repeats deterministically");
    check(world.sameContents(before),"planning cosmetic layers leaves game voxels unchanged");
    int groundCount=0,waterCount=0,hangingCount=0,wallCount=0,centerCount=0;
    std::array<bool,16> present{};
    std::array<int,3> centralTall{};
    std::vector<Bounds> remainingSmallPlants;
    for (const auto& asset : older) if (smallPlant(asset.kind)) remainingSmallPlants.push_back(bounds(asset));
    World edited=world;
    std::vector<HubAsset> removed;
    bool groundRemoved=false,waterRemoved=false,hangingRemoved=false;
    for (const auto& asset : lush) {
        check(samePlacement(full,asset),"the new kit is actually integrated into the hub");
        const auto kind=static_cast<int>(asset.kind)-static_cast<int>(HubAssetKind::TreeFern);
        check(kind>=0 && kind<int(present.size()),"lush plans use registered lush models");
        if (kind>=0 && kind<int(present.size())) present[kind]=true;
        const auto box=bounds(asset);
        check(World::contains(cell(box.low)) && World::contains(cell(box.high)),"full lush meshes remain inside the world");
        const bool central=glm::length(glm::vec2(asset.position.x-8,asset.position.z-8))<66.f;
        centerCount+=central;
        const int tall=asset.kind==HubAssetKind::TreeFern ? 0 : asset.kind==HubAssetKind::LushSapling ? 1
            : asset.kind==HubAssetKind::GiantMushroom ? 2 : -1;
        if (tall>=0) {
            if (central) ++centralTall[tall];
            for (const auto small : remainingSmallPlants)
                check(!overlap(box,small),"tall lush plants replace overlapping old foliage rather than stacking through it");
        }
        for (const auto& building : sceneBuildingPlan()) {
            const auto base=glm::vec3(building.origin);
            const auto size=hubBuildingFootprint(building);
            check(!overlap(box,{base+glm::vec3(-1.5f,0,-1.5f),
                    base+glm::vec3(size.x+1.5f,hubBuildingHeight(building)+4.f,size.y+1.5f)}),
                  "lush meshes leave village rooms, trim, and entrances unobstructed");
        }
        // Every vertex's surrounding walk-height volume must avoid the route
        // surface, including bridge decks. This catches cosmetic route blockers.
        const auto low=cell(box.low+glm::vec3(.001f,-2.1f,.001f));
        const auto high=cell(box.high-glm::vec3(.001f));
        bool clear=true;
        for (int y=std::max(1,low.y);y<=high.y;++y) for (int z=low.z;z<=high.z;++z)
            for (int x=low.x;x<=high.x;++x) {
                const auto block=world.get({x,y,z});
                clear &= !((block==Block::Path || block==Block::Planks)
                    && world.get({x,y+1,z})==Block::Air);
            }
        check(clear,"actual lush model bounds do not cover paths or bridge headroom");
        if (ground(asset.kind)) {
            ++groundCount;
            bool support=true;
            for (int z=low.z;z<=high.z;++z) for (int x=low.x;x<=high.x;++x) {
                const auto block=world.get({x,int(asset.position.y)-1,z});
                support &= isSolid(block) && block!=Block::Path && block!=Block::Planks;
            }
            check(support,"wide garden plants and logs have ground beneath their full footprint");
            if (!groundRemoved) {
                const auto anchor=cell(asset.position-glm::vec3(0,.5f,0));
                edited.set(anchor,Block::Air); removed.push_back(asset); groundRemoved=true;
            }
        } else if (asset.kind==HubAssetKind::CaveLotus || asset.kind==HubAssetKind::WaterGrass) {
            ++waterCount;
            bool support=true;
            for (int z=low.z;z<=high.z;++z) for (int x=low.x;x<=high.x;++x)
                support &= world.get({x,int(asset.position.y)-1,z})==Block::Water;
            check(support,"lotus and water grass sit in real uninterrupted surface water");
            if (!waterRemoved) {
                edited.set(cell(asset.position-glm::vec3(0,.5f,0)),Block::Air);
                removed.push_back(asset); waterRemoved=true;
            }
        } else if (asset.kind==HubAssetKind::HangingRoots || asset.kind==HubAssetKind::HangingPlanter) {
            ++hangingCount;
            check(isSolid(world.get(cell(asset.position))),"hanging roots and baskets meet a real ceiling or branch");
            if (!hangingRemoved) {
                edited.set(cell(asset.position),Block::Air);
                removed.push_back(asset); hangingRemoved=true;
            }
        } else {
            ++wallCount;
            constexpr glm::vec3 normals[]{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
            check(isSolid(world.get(cell(asset.position-normals[asset.facing]*.25f))),
                  "vertical fungi, vines, and roots are attached to a solid face");
        }
    }
    check(groundCount>=30 && waterCount>=8 && hangingCount>=5 && wallCount>=5,
          "hub dressing includes visible ground, water, overhead and wall layers");
    check(centerCount>=40,"new foliage is visible around the central tree and waterfront");
    check(std::all_of(centralTall.begin(),centralTall.end(),[](int count) { return count>=2; }),
          "tree ferns, saplings and giant mushrooms each have several silhouettes near the central village");
    check(std::count(present.begin(),present.end(),true)>=12,"the hub showcases a varied portion of the lush kit");
    const auto changed=planLushAssets(edited,older);
    for (const auto& asset : removed)
        check(!samePlacement(changed,asset),"removing a ground, water or ceiling support removes its model");

    // Surface accents may change material but must never add collision, drain
    // a lake, erase a path, or replace the grass beds used by existing gardens.
    World terrain(73);
    for (std::size_t i=0;i<World::ChunkCount;++i) {
        const auto origin=terrain.origin(i);
        Chunk generated;
        generateTerrain(generated,origin,terrain.seed());
        for (int y=0;y<Chunk::Size;++y) for (int z=0;z<Chunk::Size;++z) for (int x=0;x<Chunk::Size;++x)
            if (generated.get(x,y,z)!=Block::Air)
                terrain.set(origin+glm::ivec3(x,y,z),generated.get(x,y,z));
    }
    // Gentle generated hills expose only their grass tops. Add the rock/soil
    // ledges, a paved crossing and a water pool that a cave surface pass targets.
    for (int z=18;z<=36;++z) for (int x=-8;x<=24;++x) for (int y=1;y<=15;++y)
        terrain.set({x,y,z},y==15 ? (z==27 ? Block::Path : Block::Grass)
                                  : y>=13 ? Block::Dirt : Block::Stone);
    for (int z=14;z<18;++z) for (int x=-6;x<=0;++x) for (int y=8;y<=10;++y)
        terrain.set({x,y,z},Block::Water);
    const auto original=terrain;
    dressLushTerrain(terrain);
    int repainted=0;
    for (std::size_t i=0;i<World::ChunkCount;++i) {
        const auto& a=original.chunk(i); const auto& b=terrain.chunk(i);
        for (int y=0;y<Chunk::Size;++y) for (int z=0;z<Chunk::Size;++z) for (int x=0;x<Chunk::Size;++x) {
            const auto old=a.get(x,y,z), now=b.get(x,y,z);
            repainted+=old!=now;
            if (old==now) continue;
            check(isSolid(old)==isSolid(now) && isRenderable(old)==isRenderable(now),
                  "surface repainting preserves every collision and air boundary");
            check(old!=Block::Grass && old!=Block::Path && old!=Block::Water,
                  "terrain accents preserve grass beds, authored paving and water");
        }
    }
    check(repainted>20,"surface accents actually repaint natural faces");
    const auto painted=terrain;
    dressLushTerrain(terrain);
    check(terrain.sameContents(painted),"surface accent painting is idempotent");

    World alternate(73);
    buildUndergroundScene(alternate);
    check(planLushAssets(alternate,{})!=planLushAssets(world,{}),"scene seeds vary the garden placement");
    for (std::size_t i=0;i<World::ChunkCount;++i) {
        const auto origin=terrain.origin(i);
        for (int y=0;y<Chunk::Size;++y) for (int z=0;z<Chunk::Size;++z) for (int x=0;x<Chunk::Size;++x)
            terrain.set(origin+glm::ivec3(x,y,z),Block::Air);
    }
    check(planLushAssets(terrain,{}).empty(),"empty worlds never acquire floating lush scenery");
    return failures?1:0;
}
