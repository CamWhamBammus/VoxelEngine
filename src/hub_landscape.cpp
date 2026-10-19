#include "hub_assets.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/vec2.hpp>

namespace voxel {
namespace {

struct Bounds { glm::vec3 low,high; };

std::uint32_t hash(int x,int z,std::uint32_t seed)
{
    auto value=seed^(static_cast<std::uint32_t>(x)*0x9e3779b9u)
                   ^(static_cast<std::uint32_t>(z)*0x85ebca6bu);
    value=(value^(value>>16))*0x7feb352du;
    value=(value^(value>>15))*0x846ca68bu;
    return value^(value>>16);
}

glm::vec3 rotate(glm::vec3 p,int facing)
{
    switch (facing) {
    case 1: return {p.z,p.y,-p.x};
    case 2: return {-p.x,p.y,-p.z};
    case 3: return {-p.z,p.y,p.x};
    default: return p;
    }
}

Bounds placedBounds(const HubAsset& asset,Bounds local)
{
    const auto a=rotate(local.low,asset.facing),b=rotate(local.high,asset.facing);
    return {asset.position+glm::min(a,b),asset.position+glm::max(a,b)};
}

bool natural(Block block)
{
    return block==Block::Grass || block==Block::Dirt || block==Block::Stone
        || block==Block::Limestone || block==Block::Slate || block==Block::MossStone
        || block==Block::Calcite || block==Block::RootedDirt || block==Block::Moss || block==Block::GlowMoss;
}

// Small conservative boxes cover the whole model, not just its center. The
// attached plane is excluded from air checks and included in support checks.
template<class Predicate>
bool volume(const World& world,const HubAsset& asset,Bounds local,Predicate accepts)
{
    const auto box=placedBounds(asset,local);
    const auto low=glm::ivec3(glm::floor(box.low+glm::vec3(.0001f)));
    const auto high=glm::ivec3(glm::floor(box.high-glm::vec3(.0001f)));
    if (!World::contains(low) || !World::contains(high)) return false;
    for (int y=low.y;y<=high.y;++y) for (int z=low.z;z<=high.z;++z)
        for (int x=low.x;x<=high.x;++x) if (!accepts(world.get({x,y,z}))) return false;
    return true;
}

bool clear(const World& world,const HubAsset& asset,Bounds local)
{
    return volume(world,asset,local,[](Block block){return block==Block::Air;});
}

std::vector<glm::ivec2> columns(const World& world,int step,std::uint32_t salt)
{
    std::vector<glm::ivec2> result;
    for (int z=World::Min.z+5;z<World::MaxExclusive.z-5;z+=step)
        for (int x=World::Min.x+5;x<World::MaxExclusive.x-5;x+=step) result.push_back({x,z});
    // A capped scan should fill the whole hub, rather than its first corner.
    std::stable_sort(result.begin(),result.end(),[&](glm::ivec2 a,glm::ivec2 b) {
        return hash(a.x,a.y,world.seed()^salt)<hash(b.x,b.y,world.seed()^salt);
    });
    return result;
}

} // namespace

std::vector<HubAsset> planLandscapeAssets(const World& world)
{
    std::vector<HubAsset> assets;
    std::vector<Bounds> occupied;
    const auto add=[&](const HubAsset& asset,Bounds local) {
        const auto box=placedBounds(asset,local);
        if (!World::contains(glm::ivec3(glm::floor(box.low)))
            || !World::contains(glm::ivec3(glm::floor(box.high)))) return false;
        for (const auto& building : sceneBuildingPlan()) {
            const auto size=hubBuildingFootprint(building);
            const glm::vec3 low=glm::vec3(building.origin)+glm::vec3(-1.5f,1,-1.5f);
            const glm::vec3 high=glm::vec3(building.origin)
                +glm::vec3(size.x+1.5f,hubBuildingHeight(building)+5.f,size.y+1.5f);
            if (box.low.x<high.x && box.high.x>low.x && box.low.y<high.y && box.high.y>low.y
                && box.low.z<high.z && box.high.z>low.z) return false;
        }
        for (const auto& other : occupied) {
            if (box.low.x<other.high.x+.15f && box.high.x>other.low.x-.15f
                && box.low.y<other.high.y+.15f && box.high.y>other.low.y-.15f
                && box.low.z<other.high.z+.15f && box.high.z>other.low.z-.15f) return false;
        }
        assets.push_back(asset); occupied.push_back(box);
        return true;
    };
    const auto model=[&](HubAssetKind kind,glm::vec3 position,int facing) {
        const auto cell=glm::ivec3(glm::floor(position));
        return HubAsset{kind,position,facing,hash(cell.x,cell.z,world.seed()^std::uint32_t(cell.y*73))};
    };

    // Keep arches inside the tree's planted bed, facing the four approaches.
    // The bridges are wider than these models' openings. Reserving the whole
    // footprint plus shoulder room keeps both bridge lanes and the plaza loop
    // clear, rather than accepting a root foot on any solid paving block.
    constexpr Bounds archBounds{{-3.95f,0,-.8f},{3.95f,3.95f,.8f}};
    constexpr glm::ivec3 normals[4]{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
    for (int facing=0;facing<4;++facing) {
        const auto asset=model(HubAssetKind::RootArch,
            HeartTreeCenter+glm::vec3(normals[facing])*10.f,facing);
        if (clear(world,asset,archBounds)
            && volume(world,asset,{{-4.3f,-1,-1.15f},{4.3f,0,1.15f}},natural))
            add(asset,archBounds);
    }

    // Roots hang from the top of a genuine retaining face. Six clear blocks
    // below its lip leave walking headroom beneath the longest hanging stem.
    constexpr Bounds curtainBounds{{-1.6f,-3.8f,0},{1.6f,0,.55f}};
    constexpr Bounds ledgeBounds{{-2.8f,-1.1f,0},{2.8f,.35f,1.5f}};
    const auto walls=[&](bool roots,int limit) {
        int count=0;
        for (const auto column : columns(world,roots?5:7,roots?37:53)) {
            if (count>=limit) break;
            bool placed=false;
            for (int y=World::MaxExclusive.y-6;y>=14 && !placed;--y) {
                const glm::ivec3 anchor(column.x,y,column.y);
                if (!natural(world.get(anchor))) continue;
                if (roots && world.get(anchor+glm::ivec3(0,1,0))!=Block::Air) continue;
                for (int facing=0;facing<4;++facing) {
                    if (world.get(anchor+normals[facing])!=Block::Air) continue;
                    auto position=glm::vec3(anchor)+glm::vec3(.5f)+glm::vec3(normals[facing])*.5f;
                    if (roots) position.y=y+.98f;
                    const auto asset=model(roots?HubAssetKind::RootCurtain:HubAssetKind::RockLedge,position,facing);
                    const float width=roots?1.6f:2.8f;
                    if (!volume(world,asset,{{-width,roots?-3.8f:-1.1f,-.95f},
                        {width,roots?0.f:.35f,-.005f}},natural)) continue;
                    if (!clear(world,asset,{{-width,roots?-6.f:-3.6f,.005f},
                        {width,roots?0.f:.35f,roots?.55f:1.5f}})) continue;
                    if (add(asset,roots?curtainBounds:ledgeBounds)) {++count; placed=true; break;}
                }
            }
        }
    };
    walls(true,20);
    walls(false,28);

    constexpr Bounds stalactiteBounds{{-1.6f,-3.8f,-1.6f},{1.6f,0,1.6f}};
    int stalactites=0;
    for (const auto column : columns(world,11,71)) {
        if (stalactites>=28) break;
        for (int y=World::MaxExclusive.y-2;y>=20;--y) {
            const auto asset=model(HubAssetKind::StalactiteCluster,{column.x+.5f,float(y),column.y+.5f},0);
            if (!natural(world.get({column.x,y,column.y}))
                || world.get({column.x,y-1,column.y})!=Block::Air) continue;
            if (!volume(world,asset,{{-1.6f,0,-1.6f},{1.6f,1,1.6f}},natural)
                || !clear(world,asset,{{-1.6f,-4,-1.6f},{1.6f,0,1.6f}})) continue;
            if (add(asset,stalactiteBounds)) ++stalactites;
            break;
        }
    }

    constexpr Bounds boulderBounds{{-1.8f,0,-1.8f},{1.8f,2.4f,1.8f}};
    int boulders=0;
    for (const auto column : columns(world,9,89)) {
        if (boulders>=36) break;
        for (int y=World::MaxExclusive.y-5;y>=8;--y) {
            if (!natural(world.get({column.x,y,column.y}))) continue;
            const auto asset=model(HubAssetKind::Boulder,{column.x+.5f,y+1.f,column.y+.5f},0);
            if (!volume(world,asset,{{-1.8f,-1,-1.8f},{1.8f,0,1.8f}},natural)
                || !clear(world,asset,{{-1.8f,0,-1.8f},{1.8f,3,1.8f}})) continue;
            if (add(asset,boulderBounds)) ++boulders;
            break;
        }
    }
    return assets;
}

} // namespace voxel
