#include "asset_collision.h"
#include "heartwood.h"
#include "heartwood_assets.h"
#include "scene.h"
#include "sections.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
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
    HubAssetKind::SapCluster,HubAssetKind::RootCorbel};

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
    check(count(HubAssetKind::LibraryBookcase)>=12,"both library floors and the study contain modeled shelves");
    check(count(HubAssetKind::ReadingDesk)==6&&count(HubAssetKind::ReadingSeat)==6,
          "all three upstairs rooms have reading places");
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
