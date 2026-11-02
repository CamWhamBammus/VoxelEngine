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
        auto inside=vertex.position-glm::sign(vertex.position-center)*.001f;
        // Slender stems have a few millimeters of thickness below their soil
        // anchor. Keep their base seated without hiding deeper intersections.
        if (!wall&&vertex.position.y>=asset.position.y-.005f)
            inside.y=std::max(inside.y,asset.position.y+.001f);
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
    const auto ceiling=[&](glm::vec3 position,HubAssetKind kind=HubAssetKind::ChainLantern) {
        // A gallery includes a two-block fascia below its walking surface.
        // Find its real underside rather than embedding a chain in that fascia.
        auto support=glm::ivec3(glm::floor(position+glm::vec3(0,.05f,0)));
        if (!isSolid(world.get(support))) return;
        const int lowest=support.y-6;
        while (support.y>lowest&&isSolid(world.get(support-glm::ivec3(0,1,0)))) --support.y;
        if (isSolid(world.get(support-glm::ivec3(0,1,0)))) return;
        position.y=float(support.y);
        HubAsset asset{kind,position,0,seed++};
        // The top plate embeds into the beam; only the hanging part needs air.
        for (const auto& vertex:buildHubAsset(asset).vertices) {
            if (vertex.position.y>=position.y-.07f) continue;
            if (isSolid(world.get(glm::ivec3(glm::floor(vertex.position))))) return;
        }
        assets.push_back(asset);
    };

    floor(HubAssetKind::HeartSeed,heartwoodLayout().seed);

    // Each room has a different silhouette and use. Furnish the back and sides
    // first, keeping the doorway and a generous central walk unobstructed.
    for (const auto& room:heartwoodRooms()) {
        const float y=float(room.minimum.y);
        const float left=float(room.minimum.x),right=float(room.maximum.x);
        const float back=float(room.minimum.z),front=float(room.maximum.z);
        const float middleX=(left+right)*.5f,middleZ=(back+front)*.5f;
        switch (room.kind) {
        case HeartwoodRoomKind::Library:
            for (float x=left+1.5f;x<=right-1.0f;x+=2.65f)
                floor(HubAssetKind::LibraryBookcase,{x,y,back+.65f});
            floor(HubAssetKind::LibraryBookcase,{left+.65f,y,back+4.5f},1);
            floor(HubAssetKind::ReadingDesk,{right-1.5f,y,back+5.8f});
            floor(HubAssetKind::ReadingSeat,{right-1.5f,y,back+7.3f},2);
            ceiling({middleX,float(room.maximum.y)+1,middleZ+1.f});
            break;
        case HeartwoodRoomKind::ExpeditionLoft:
            floor(HubAssetKind::ReliefMapTable,{middleX,y,middleZ-.2f});
            floor(HubAssetKind::SurveyInstrument,{right-1.5f,y,front-2.f},3);
            wall(HubAssetKind::ToolRack,{middleX,y+.3f,back+.02f});
            floor(HubAssetKind::CrateStack,{left+1.5f,y,back+2.2f});
            floor(HubAssetKind::Barrel,{left+2.1f,y,back+4.f});
            ceiling({middleX,float(room.maximum.y)+1,middleZ+.4f});
            break;
        case HeartwoodRoomKind::Conservatory:
            floor(HubAssetKind::PottingBench,{middleX,y,back+1.8f});
            floor(HubAssetKind::NurseryBed,{right-2.f,y,back+5.f});
            floor(HubAssetKind::NurseryBed,{right-2.f,y,front-4.f});
            ceiling({middleX,float(room.maximum.y)+1,middleZ});
            ceiling({right-1.5f,float(room.maximum.y)+1,back+5.f},HubAssetKind::HangingFern);
            ceiling({left+1.f,float(room.maximum.y)+1,front-2.f},HubAssetKind::HangingPlanter);
            break;
        }
    }

    // Matching hanging lamps on structural beam intersections frame the atrium.
    for (const float x:{-11.5f,8.5f,28.5f})
        for (const float z:{-12.5f,9.5f,29.5f}) ceiling({x,48,z});
    for (const float y:{21.f,34.f}) {
        for (const float x:{-5.5f,21.5f})
            for (const float z:{3.5f,15.5f,25.5f}) ceiling({x,y,z});
        for (const float x:{-5.5f,21.5f}) ceiling({x,y,31.5f});
    }

    // Fine sap fragments sit proud of the native glowing seam and connect the
    // seed's warm focus to the high cyan light running through the living tree.
    for (int y=11;y<51;y+=4) {
        const float x=(y/4)%2 ? 5.5f : 12.5f;
        wall(HubAssetKind::SapCluster,{x,float(y),-7.0f});
    }

    // Each carved knee meets a full-height tree column beneath the gallery.
    // The projecting branch reaches its deck without a freestanding root end.
    for (const float y:{20.f,33.f}) {
        wall(HubAssetKind::RootCorbel,{-15.0f,y,28.5f},1);
        wall(HubAssetKind::RootCorbel,{32.0f,y,28.5f},3);
        wall(HubAssetKind::RootCorbel,{3.5f,y,-15.0f});
        wall(HubAssetKind::RootCorbel,{18.5f,y,-15.0f});
    }

    constexpr std::array<glm::vec3,4> gardenCenters{{
        {-3.5f,10,-.5f},{21.5f,10,-.5f},{-3.5f,10,21.5f},{23.5f,10,22.5f}
    }};
    for (std::size_t i=0;i<gardenCenters.size();++i) {
        floor(i%2 ? HubAssetKind::MossGarden : HubAssetKind::FernBed,gardenCenters[i]);
        if (i%2!=0) floor(HubAssetKind::BroadleafClump,gardenCenters[i]);
        floor(HubAssetKind::CrystalFlower,gardenCenters[i]+glm::vec3(.7f,0,.5f));
        // Small mushrooms grow below the ferns, opposite the crystal flowers.
        // Keep their complete footprint inside the raised moss planting bed.
        if (i%2==0)
            floor(HubAssetKind::Mushrooms,gardenCenters[i]+glm::vec3(-.65f,0,-.55f));
    }
    // The enlarged island replaces the original reed pockets. Replant its
    // outer east banks beside the water, clear of the approach and tall plants.
    floor(HubAssetKind::Reeds,{47.5f,9,19.5f});
    floor(HubAssetKind::Reeds,{47.5f,9,1.5f});

    // The garden layer follows rail edges and beam undersides. No broad leaves
    // are scattered across the stairs, seed approach, or library doors.
    for (const float y:{21.f,34.f}) {
        for (const float z:{6.5f,18.5f}) {
            ceiling({-5.5f,y,z},HubAssetKind::HangingFern);
            ceiling({21.5f,y,z},HubAssetKind::HangingFern);
            wall(HubAssetKind::MossDrape,{-5.f,y+.08f,z-1.6f},1);
            wall(HubAssetKind::MossDrape,{21.f,y+.08f,z-1.6f},3);
        }
        wall(HubAssetKind::BlossomVines,{-5.f,y+.10f,31.5f},1);
        wall(HubAssetKind::BlossomVines,{21.f,y+.10f,31.5f},3);
    }
    for (const float x:{-11.5f,28.5f})
        for (const float z:{-.5f,21.5f}) ceiling({x,48,z},HubAssetKind::Vines);

    // Continuous waist-high rails fill the gaps between the voxel newel posts.
    // The first gallery's west stair landing remains a broad open threshold.
    for (const int top:{HeartwoodGalleryFloor,HeartwoodUpperFloor}) {
        for (int z=3;z<=33;++z) {
            if (z==3||z==9||z==15||z==21||z==25) continue;
            for (const int x:{-6,21})
                floor(HubAssetKind::TimberFence,{float(x)+.5f,float(top+1),float(z)+.5f},1);
        }
        // The open front of the U-shaped gallery needs guards across its
        // exposed ends as well as along the inner edge of the atrium.
        for (int x=-16;x<=32;++x)
            if (x<=-7 || x>=22)
                floor(HubAssetKind::TimberFence,{float(x)+.5f,float(top+1),33.5f});
    }
    // Stair guards follow each half-step flight along its outside shoulder.
    for (const auto flight:std::array<glm::ivec2,2>{{{HeartwoodFloor,27},{HeartwoodGalleryFloor,11}}})
        for (int step=0;step<13;++step) {
            if (step%4==0) continue; // Existing full-height newel posts.
            const int top=flight.x+1+step,z=flight.y-step;
            for (const int x:{-16,-8})
                floor(HubAssetKind::TimberFence,{float(x)+.5f,float(top+1),float(z)+.5f},1);
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
