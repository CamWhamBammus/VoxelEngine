#include "heartwood_assets.h"
#include "heartwood.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace voxel {
namespace {

bool hallPresent(const World& world)
{
    return world.get({8,8,11})==Block::Path
        && world.get({8,48,9})==Block::Timber
        && world.get({8,38,-11})==Block::RootWood;
}

struct Bounds { glm::vec3 low,high; };
Bounds modelBounds(const HubAsset& asset)
{
    Bounds bounds{glm::vec3(std::numeric_limits<float>::max()),
                  glm::vec3(std::numeric_limits<float>::lowest())};
    for (const auto& vertex:buildHubAsset(asset).vertices) {
        bounds.low=glm::min(bounds.low,vertex.position);
        bounds.high=glm::max(bounds.high,vertex.position);
    }
    return bounds;
}

bool grounded(const World& world,const HubAsset& asset)
{
    const auto bounds=modelBounds(asset);
    const int y=int(std::floor(asset.position.y-.01f));
    for (int z=int(std::floor(bounds.low.z+.001f));z<=int(std::floor(bounds.high.z-.001f));++z)
        for (int x=int(std::floor(bounds.low.x+.001f));x<=int(std::floor(bounds.high.x-.001f));++x)
            if (!isSolid(world.get({x,y,z}))) return false;
    return true;
}

bool clearModel(const World& world,const HubAsset& asset,bool wall=false)
{
    constexpr glm::vec3 directions[]{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
    const auto normal=directions[asset.facing&3];
    const auto bounds=modelBounds(asset);
    const auto center=(bounds.low+bounds.high)*.5f;
    for (const auto& vertex:buildHubAsset(asset).vertices) {
        // A thin backing is deliberately embedded in its supporting wall.
        if (wall&&glm::dot(vertex.position-asset.position,normal)<.08f) continue;
        const auto inside=vertex.position-glm::sign(vertex.position-center)*.001f;
        const auto cell=glm::ivec3(glm::floor(inside));
        if (!World::contains(cell)||isSolid(world.get(cell))) return false;
    }
    return true;
}

} // namespace

std::vector<HubAsset> planHeartwoodAssets(const World& world)
{
    std::vector<HubAsset> assets;
    if (!hallPresent(world)) return assets;
    std::uint32_t seed=21;
    const auto floor=[&](HubAssetKind kind,glm::vec3 position,int facing=0) {
        HubAsset asset{kind,position,facing,seed++};
        if (grounded(world,asset)&&clearModel(world,asset)) assets.push_back(asset);
    };
    const auto wall=[&](HubAssetKind kind,glm::vec3 position,int facing=0) {
        constexpr glm::vec3 directions[]{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
        HubAsset asset{kind,position,facing,seed++};
        const auto support=glm::ivec3(glm::floor(position-directions[facing&3]*.12f));
        if (isSolid(world.get(support))&&clearModel(world,asset,true)) assets.push_back(asset);
    };
    const auto ceiling=[&](glm::vec3 position) {
        HubAsset asset{HubAssetKind::ChainLantern,position,0,seed++};
        if (!isSolid(world.get(glm::ivec3(glm::floor(position+glm::vec3(0,.05f,0)))))) return;
        // The top plate embeds into the beam; only the hanging part needs air.
        for (const auto& vertex:buildHubAsset(asset).vertices) {
            if (vertex.position.y>=position.y-.07f) continue;
            if (isSolid(world.get(glm::ivec3(glm::floor(vertex.position))))) return;
        }
        assets.push_back(asset);
    };

    floor(HubAssetKind::HeartSeed,heartwoodLayout().seed);

    // Shelves line room backs; the middle and doorway stay open for walking.
    for (const auto& room:heartwoodRooms()) {
        const float y=float(room.minimum.y);
        for (float x=float(room.minimum.x)+1.5f;x<=float(room.maximum.x)-.8f;x+=2.65f)
            floor(HubAssetKind::LibraryBookcase,{x,y,float(room.minimum.z)+.65f});
        for (float z=float(room.minimum.z)+4.0f;z<float(room.maximum.z)-1.5f;z+=2.65f)
            floor(HubAssetKind::LibraryBookcase,{float(room.minimum.x)+.65f,y,z},1);

        const float middleX=float(room.minimum.x+room.maximum.x)*.5f;
        const float middleZ=float(room.minimum.z+room.maximum.z)*.5f;
        floor(HubAssetKind::ReadingDesk,{middleX+.8f,y,middleZ-.2f});
        floor(HubAssetKind::ReadingSeat,{middleX+.8f,y,middleZ+1.3f},2);
        floor(HubAssetKind::ReadingDesk,{float(room.maximum.x)-1.9f,y,float(room.minimum.z)+3.5f},3);
        floor(HubAssetKind::ReadingSeat,{float(room.maximum.x)-3.4f,y,float(room.minimum.z)+3.5f},1);
        ceiling({middleX+.5f,float(room.maximum.y)+1.0f,middleZ+.5f});
    }

    // Matching hanging lamps on structural beam intersections frame the atrium.
    for (const float x:{-11.5f,8.5f,28.5f})
        for (const float z:{-12.5f,9.5f,29.5f}) ceiling({x,48,z});
    for (const float y:{21.f,34.f}) {
        for (const float x:{-5.5f,21.5f})
            for (const float z:{3.5f,15.5f,25.5f}) ceiling({x,y,z});
        for (const float x:{1.5f,13.5f}) ceiling({x,y,26.5f});
    }

    // Fine sap fragments sit proud of the native glowing seam and connect the
    // seed's warm focus to the high cyan light running through the living tree.
    for (int y=11;y<51;y+=4) {
        const float x=(y/4)%2 ? 5.5f : 12.5f;
        wall(HubAssetKind::SapCluster,{x,float(y),-7.0f});
    }

    // Each carved knee meets a full-height tree column beneath the gallery.
    // The projecting branch reaches its deck without a freestanding root end.
    for (const float y:{21.f,34.f}) {
        wall(HubAssetKind::RootCorbel,{-15.0f,y,28.5f},1);
        wall(HubAssetKind::RootCorbel,{32.0f,y,28.5f},3);
        wall(HubAssetKind::RootCorbel,{3.5f,y,-15.0f});
        wall(HubAssetKind::RootCorbel,{18.5f,y,-15.0f});
    }

    // Continuous waist-high rails fill the gaps between the voxel newel posts.
    // The first gallery's west stair landing remains a broad open threshold.
    for (const int top:{HeartwoodGalleryFloor,HeartwoodUpperFloor}) {
        for (int z=3;z<=25;++z) {
            if (z==3||z==9||z==15||z==21||z==25) continue;
            for (const int x:{-6,21})
                floor(HubAssetKind::TimberFence,{float(x)+.5f,float(top+1),float(z)+.5f},1);
        }
        for (int x=-5;x<=19;++x) {
            if (x==-5||x==1||x==7||x==13||x==19) continue;
            floor(HubAssetKind::TimberFence,{float(x)+.5f,float(top+1),26.5f});
        }
    }
    return assets;
}

void blendHeartwoodAssets(const World& world,std::vector<HubAsset>& assets)
{
    if (!hallPresent(world)) return;
    std::erase_if(assets,[&](const HubAsset& asset) {
        // Native block lanterns still need their caps/brackets in the new hall.
        if (asset.kind==HubAssetKind::LanternCap||asset.kind==HubAssetKind::WallBracket) return false;
        const auto p=asset.position;
        if (p.x<HeartwoodMinimum.x-6||p.x>HeartwoodMaximum.x+6
            ||p.z<HeartwoodMinimum.z-6||p.z>HeartwoodMaximum.z+6||p.y>62) return false;
        const auto bounds=modelBounds(asset);
        return bounds.high.x>HeartwoodMinimum.x&&bounds.low.x<HeartwoodMaximum.x+1
            &&bounds.high.z>HeartwoodMinimum.z&&bounds.low.z<HeartwoodMaximum.z+1
            &&bounds.low.y<60;
    });
    auto planned=planHeartwoodAssets(world);
    assets.insert(assets.end(),planned.begin(),planned.end());
}

} // namespace voxel
