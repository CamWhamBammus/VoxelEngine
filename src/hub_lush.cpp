#include "hub_lush.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace voxel {
namespace {
using Kind = HubAssetKind;
struct Bounds { glm::vec3 low, high; };
struct Occupied { Bounds bounds; bool smallPlant=false; };
constexpr glm::ivec3 Sides[4]{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};

std::uint32_t hash(int x, int z, std::uint32_t seed)
{
    auto value = seed ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u)
                      ^ (static_cast<std::uint32_t>(z) * 0x85ebca6bu);
    value = (value ^ (value >> 16)) * 0x7feb352du;
    value = (value ^ (value >> 15)) * 0x846ca68bu;
    return value ^ (value >> 16);
}

glm::vec3 turn(glm::vec3 p, int facing)
{
    switch (facing & 3) {
    case 1: return {p.z,p.y,-p.x};
    case 2: return {-p.x,p.y,-p.z};
    case 3: return {-p.z,p.y,p.x};
    default: return p;
    }
}

Bounds localBounds(Kind kind)
{
    switch (kind) {
    case Kind::TreeFern: return {{-1.9f,0,-1.9f},{1.9f,3.4f,1.9f}};
    case Kind::LushSapling: return {{-1.6f,0,-1.6f},{1.6f,3.8f,1.6f}};
    case Kind::HangingRoots: return {{-1.2f,-3.4f,-.8f},{1.2f,0,.8f}};
    case Kind::BlossomVines: return {{-.8f,-3.2f,0},{.8f,0,.6f}};
    case Kind::GlowMushroomColony: return {{-.95f,0,-.95f},{.95f,1.3f,.95f}};
    case Kind::GiantMushroom: return {{-1.7f,0,-1.7f},{1.7f,3.65f,1.7f}};
    case Kind::ShelfFungi: return {{-1.1f,-1.1f,0},{1.1f,1,1}};
    case Kind::CaveLotus: return {{-1,-.22f,-1},{1,.65f,1}};
    case Kind::WaterGrass: return {{-.65f,-.65f,-.65f},{.65f,1.65f,.65f}};
    case Kind::MossyLog: return {{-1.7f,0,-.7f},{1.7f,1.15f,.7f}};
    case Kind::RootFan: return {{-1.4f,-1.4f,0},{1.4f,1.4f,.55f}};
    case Kind::MossyStones: return {{-1.2f,0,-.9f},{1.2f,1,.9f}};
    case Kind::CrystalFlower: return {{-.8f,0,-.8f},{.8f,1.5f,.8f}};
    case Kind::SporeStalks: return {{-.8f,0,-.8f},{.8f,2.6f,.8f}};
    case Kind::CaveOrchid: return {{-.8f,0,-.8f},{.8f,1.2f,.8f}};
    case Kind::HangingPlanter: return {{-.8f,-2.8f,-.8f},{.8f,0,.8f}};
    default: return {{0,0,0},{0,0,0}};
    }
}

bool smallGroundPlant(Kind kind)
{
    return kind==Kind::Fern || kind==Kind::Flowers || kind==Kind::Mushrooms
        || kind==Kind::Reeds || kind==Kind::Shrub || kind==Kind::BroadleafClump
        || kind==Kind::FernBed || kind==Kind::MossGarden;
}

bool tallPlant(Kind kind)
{
    return kind==Kind::TreeFern || kind==Kind::LushSapling || kind==Kind::GiantMushroom;
}

Bounds meshBounds(const HubAsset& asset)
{
    Bounds bounds{glm::vec3(std::numeric_limits<float>::max()),
                  glm::vec3(std::numeric_limits<float>::lowest())};
    for (const auto& vertex : buildHubAsset(asset).vertices) {
        bounds.low=glm::min(bounds.low,vertex.position);
        bounds.high=glm::max(bounds.high,vertex.position);
    }
    return bounds;
}

Bounds placed(const HubAsset& asset, Bounds bounds)
{
    const auto a = turn(bounds.low, asset.facing), b = turn(bounds.high, asset.facing);
    return {asset.position + glm::min(a,b), asset.position + glm::max(a,b)};
}

bool overlaps(Bounds a, Bounds b, float margin = 0)
{
    return a.low.x < b.high.x + margin && a.high.x > b.low.x - margin
        && a.low.y < b.high.y + margin && a.high.y > b.low.y - margin
        && a.low.z < b.high.z + margin && a.high.z > b.low.z - margin;
}

bool clearOfBuildings(Bounds bounds)
{
    for (const auto& building : sceneBuildingPlan()) {
        const auto size = hubBuildingFootprint(building);
        const auto base = glm::vec3(building.origin);
        // Reserve the porch, window boxes and entrance approach as well as the room.
        if (overlaps(bounds, {base + glm::vec3(-2.5f,0,-2.5f),
                base + glm::vec3(size.x+2.5f,hubBuildingHeight(building)+5.f,size.y+2.5f)}))
            return false;
    }
    return true;
}

bool soil(Block block)
{
    return block == Block::Grass || block == Block::Dirt || block == Block::RootedDirt || block == Block::Moss;
}

bool natural(Block block)
{
    return soil(block) || block == Block::Stone || block == Block::Slate || block == Block::Limestone
        || block == Block::MossStone || block == Block::Calcite || block == Block::GlowMoss;
}

bool woodOrRock(Block block)
{
    return natural(block) || block == Block::Timber || block == Block::RootWood;
}

bool treeOrRock(Block block)
{
    return woodOrRock(block) || block == Block::Leaves || block == Block::GlowLeaves
        || block == Block::BlossomLeaves;
}

template<class Predicate>
bool volume(const World& world, Bounds bounds, Predicate accepts)
{
    const auto low = glm::ivec3(glm::floor(bounds.low + glm::vec3(.0001f)));
    const auto high = glm::ivec3(glm::floor(bounds.high - glm::vec3(.0001f)));
    if (!World::contains(low) || !World::contains(high)) return false;
    for (int y=low.y; y<=high.y; ++y) for (int z=low.z; z<=high.z; ++z)
        for (int x=low.x; x<=high.x; ++x) if (!accepts(world.get({x,y,z}))) return false;
    return true;
}

bool air(const World& world, Bounds bounds)
{
    return volume(world,bounds,[](Block block) { return block == Block::Air; });
}

// Even a noncolliding plant can hide a stair or clip through a walker. Reserve
// shoulder room on every path and bridge beside the full model silhouette.
bool leavesRoutesClear(const World& world, Bounds bounds)
{
    const auto low = glm::ivec3(glm::floor(bounds.low - glm::vec3(.4f,2.2f,.4f)));
    const auto high = glm::ivec3(glm::floor(bounds.high + glm::vec3(.4f,0,.4f)));
    for (int y=std::max(1,low.y); y<=high.y; ++y) for (int z=low.z; z<=high.z; ++z)
        for (int x=low.x; x<=high.x; ++x) {
            const auto block = stairMaterial(world.get({x,y,z}));
            if ((block == Block::Path || block == Block::Planks || block == Block::AgedPlanks)
                && world.get({x,y+1,z}) == Block::Air) return false;
        }
    return true;
}

std::vector<glm::ivec2> columns(const World& world, int spacing, std::uint32_t salt)
{
    std::vector<glm::ivec2> result;
    for (int z=-98; z<110; z+=spacing) for (int x=-98; x<110; x+=spacing)
        result.push_back({x,z});
    std::stable_sort(result.begin(),result.end(),[&](glm::ivec2 a, glm::ivec2 b) {
        // Fill the focal tree and waterfront first, then the outer neighborhoods.
        const auto band = [](glm::ivec2 p) {
            return glm::length(glm::vec2(p)-glm::vec2(8,8)) < 66.f ? 0 : 1;
        };
        if (band(a) != band(b)) return band(a) < band(b);
        return hash(a.x,a.y,world.seed()^salt) < hash(b.x,b.y,world.seed()^salt);
    });
    return result;
}

} // namespace

std::vector<HubAsset> planLushAssets(const World& world, std::span<const HubAsset> existing)
{
    std::vector<HubAsset> result;
    std::vector<Occupied> occupied;
    occupied.reserve(existing.size()+220);
    for (const auto& asset : existing) {
        if (isPathAsset(asset.kind)) continue; // route voxels handle these as one surface
        occupied.push_back({meshBounds(asset),smallGroundPlant(asset.kind)});
    }
    const auto model = [&](Kind kind, glm::vec3 position, int facing=0) {
        const auto cell = glm::ivec3(glm::floor(position));
        return HubAsset{kind,position,facing,hash(cell.x,cell.z,world.seed()^std::uint32_t(cell.y*97))};
    };
    const auto add = [&](const HubAsset& asset) {
        const auto bounds = placed(asset,localBounds(asset.kind));
        if (!clearOfBuildings(bounds) || !leavesRoutesClear(world,bounds)) return false;
        for (const auto& other : occupied) {
            if (tallPlant(asset.kind) && other.smallPlant) continue;
            if (overlaps(bounds,other.bounds,.18f)) return false;
        }
        if (tallPlant(asset.kind)) {
            std::erase_if(occupied,[&](const Occupied& other) {
                return other.smallPlant && overlaps(bounds,other.bounds,.18f);
            });
        }
        result.push_back(asset);
        occupied.push_back({bounds,false});
        return true;
    };

    const auto groundColumns = columns(world,3,83);
    // Taller silhouettes first; low orchids, stones and fungi form their understory.
    constexpr std::array tall{Kind::TreeFern,Kind::LushSapling,Kind::GiantMushroom};
    constexpr Kind understory[3][5]{
        {Kind::CaveOrchid,Kind::CrystalFlower,Kind::CaveOrchid,Kind::MossyStones,Kind::MossyLog},
        {Kind::GlowMushroomColony,Kind::SporeStalks,Kind::MossyLog,Kind::GlowMushroomColony,Kind::MossyStones},
        {Kind::CrystalFlower,Kind::CaveOrchid,Kind::MossyStones,Kind::SporeStalks,Kind::GlowMushroomColony}
    };
    std::array<int,3> centralTall{};
    for (int pass=0; pass<2; ++pass) {
        int count=0;
        const int limit=pass==0 ? 34 : 114;
        for (const auto column : groundColumns) {
            if (count>=limit) break;
            const auto seed=hash(column.x,column.y,world.seed());
            const auto patch=hash(int(std::floor(column.x/15.f)),int(std::floor(column.y/15.f)),world.seed());
            auto tallIndex=std::size_t(patch%tall.size());
            const bool central=glm::length(glm::vec2(column)-glm::vec2(8,8))<66.f;
            if (pass==0 && central) {
                const auto least=std::min_element(centralTall.begin(),centralTall.end());
                if (*least<2) tallIndex=std::size_t(least-centralTall.begin());
            }
            const Kind kind=pass==0 ? tall[tallIndex] : understory[patch%3][seed%5];
            for (int y=39; y>=8; --y) {
                if (!soil(world.get({column.x,y,column.y}))) continue;
                const auto asset=model(kind,{column.x+.5f,y+1.f,column.y+.5f},int(seed%4));
                const auto bounds=placed(asset,localBounds(kind));
                auto bed=bounds; bed.low.y=asset.position.y-1; bed.high.y=asset.position.y;
                if (!volume(world,bed,natural) || !air(world,bounds)) continue;
                if (add(asset)) {
                    ++count;
                    if (pass==0 && central) ++centralTall[tallIndex];
                }
                break;
            }
        }
    }

    // Flowers sit in the actual water plane. The full wet footprint rejects
    // waterfall lips, narrow channels, and shores where a floating stem would show.
    int waterPlants=0;
    for (const auto column : columns(world,3,107)) {
        if (waterPlants>=34) break;
        const auto seed=hash(column.x,column.y,world.seed()^13u);
        for (int y=48; y>=1; --y) {
            if (world.get({column.x,y,column.y})!=Block::Water) continue;
            const Kind kind=seed%3==0 ? Kind::WaterGrass : Kind::CaveLotus;
            const auto asset=model(kind,{column.x+.5f,y+1.f,column.y+.5f},int(seed%4));
            auto bounds=placed(asset,localBounds(kind));
            auto wet=bounds; wet.low.y=asset.position.y-1; wet.high.y=asset.position.y;
            auto dry=bounds; dry.low.y=asset.position.y;
            if (volume(world,wet,[](Block block) { return block==Block::Water; }) && air(world,dry)
                && add(asset)) ++waterPlants;
            break;
        }
    }

    int hanging=0;
    for (const auto column : columns(world,5,131)) {
        if (hanging>=24) break;
        const auto seed=hash(column.x,column.y,world.seed());
        for (int y=76; y>=16; --y) {
            if (!treeOrRock(world.get({column.x,y,column.y}))
                || world.get({column.x,y-1,column.y})!=Block::Air) continue;
            const Kind kind=seed%7==0 ? Kind::HangingPlanter : Kind::HangingRoots;
            const auto asset=model(kind,{column.x+.5f,float(y),column.y+.5f},int(seed%4));
            auto bounds=placed(asset,localBounds(kind));
            auto ceiling=bounds; ceiling.low.y=asset.position.y; ceiling.high.y=asset.position.y+1;
            auto clearance=bounds; clearance.low.y-=2.2f;
            if (volume(world,ceiling,treeOrRock) && air(world,clearance) && add(asset)) ++hanging;
            break;
        }
    }

    // Wall-attached root fans, orchids and shelf fungi break up the vertical
    // retaining faces and tree trunks without growing into rooms or footpaths.
    int walls=0;
    constexpr std::array wallKinds{Kind::ShelfFungi,Kind::RootFan,Kind::BlossomVines};
    for (const auto column : columns(world,4,157)) {
        if (walls>=28) break;
        const auto seed=hash(column.x,column.y,world.seed());
        bool found=false;
        for (int y=42; y>=12 && !found; --y) {
            const glm::ivec3 anchor(column.x,y,column.y);
            if (!woodOrRock(world.get(anchor))) continue;
            for (int side=0; side<4; ++side) {
                const int facing=(side+int(seed%4))%4;
                if (world.get(anchor+Sides[facing])!=Block::Air) continue;
                const Kind kind=wallKinds[(seed+std::uint32_t(y))%wallKinds.size()];
                const auto position=glm::vec3(anchor)+glm::vec3(.5f)+glm::vec3(Sides[facing])*.5f;
                const auto asset=model(kind,position,facing);
                auto local=localBounds(kind), support=local, clearance=local;
                support.low.z=-.9f; support.high.z=-.001f;
                clearance.low.z=.001f;
                if (kind==Kind::BlossomVines) clearance.low.y-=2.2f;
                if (!volume(world,placed(asset,support),woodOrRock)
                    || !air(world,placed(asset,clearance))) continue;
                if (add(asset)) { ++walls; found=true; break; }
            }
        }
    }
    return result;
}

void blendLushAssets(const World& world, std::vector<HubAsset>& assets)
{
    const auto lush=planLushAssets(world,assets);
    std::vector<Bounds> tall;
    for (const auto& asset : lush) if (tallPlant(asset.kind))
        tall.push_back(placed(asset,localBounds(asset.kind)));
    std::erase_if(assets,[&](const HubAsset& asset) {
        if (!smallGroundPlant(asset.kind)) return false;
        const auto bounds=meshBounds(asset);
        return std::any_of(tall.begin(),tall.end(),[&](Bounds feature) { return overlaps(bounds,feature,.18f); });
    });
    assets.insert(assets.end(),lush.begin(),lush.end());
}

void dressLushTerrain(World& world)
{
    // Surface repainting keeps every lake, stair, room, and collision boundary
    // exactly where it was. Coherent patches avoid salt-and-pepper block noise.
    for (int z=-98; z<110; ++z) for (int x=-98; x<110; ++x) {
        const auto patch=hash(int(std::floor(x/5.f)),int(std::floor(z/5.f)),world.seed());
        for (int y=3; y<82; ++y) {
            const glm::ivec3 cell(x,y,z);
            const auto block=world.get(cell);
            if (block!=Block::Stone && block!=Block::Slate && block!=Block::Limestone
                && block!=Block::Dirt && block!=Block::Leaves) continue;
            const auto p=glm::vec3(cell);
            // The north gateway and its deep arrival tunnel are architectural.
            if (x>=-17 && x<=43 && z<=-62) continue;
            if (block==Block::Leaves) {
                // Small neighboring trees gain blush-colored outer boughs;
                // retain the central tree's recognizable green/glowing crown.
                if (glm::length(glm::vec2(x-8,z-8))>49.f && patch%7==0
                    && world.get(cell+glm::ivec3(0,1,0))==Block::Air
                    && clearOfBuildings({p,p+glm::vec3(1)}))
                    world.set(cell,Block::BlossomLeaves);
                continue;
            }
            bool exposed=world.get(cell+glm::ivec3(0,1,0))==Block::Air;
            bool wet=false, street=false;
            for (const auto side : Sides) {
                const auto next=stairMaterial(world.get(cell+side));
                exposed |= next==Block::Air || next==Block::Water;
                wet |= next==Block::Water;
                street |= next==Block::Path || next==Block::Planks;
            }
            if (!exposed || street || !clearOfBuildings({p,p+glm::vec3(1)})) continue;
            if (block==Block::Dirt) {
                if (patch%3==0) world.set(cell,Block::RootedDirt);
                continue;
            }
            const auto layer=(patch+std::uint32_t(y/4))%13;
            if (wet || layer<3) world.set(cell,Block::MossStone);
            else if (layer==4 && y<45) world.set(cell,Block::Calcite);
            else if (layer==7 && y>12 && y<42 && patch%5==0) world.set(cell,Block::GlowMoss);
        }
    }
}

} // namespace voxel
