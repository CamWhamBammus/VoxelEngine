#include "spawner_chains.h"
#include "sections.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unistd.h>

namespace {
using namespace voxel;
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
template<class Operation> bool fails(Operation operation)
{
    try { operation(); return false; } catch (const std::exception&) { return true; }
}
std::string read(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void integer(std::string& data, std::size_t offset, std::uint32_t value)
{
    for (int i=0;i<4;++i) data.at(offset+i)=static_cast<char>(value>>(i*8));
}
void writeChecked(const std::filesystem::path& path, std::string data)
{
    std::uint32_t checksum=2166136261u;
    for (std::size_t i=8;i<data.size()-4;++i)
        checksum=(checksum^static_cast<unsigned char>(data[i]))*16777619u;
    integer(data,data.size()-4,checksum);
    std::ofstream file(path,std::ios::binary);file.write(data.data(),data.size());
}
}

int main()
{
    using namespace voxel;
    char temporary[]="/tmp/voxel-chain-storage-XXXXXX";
    if (!mkdtemp(temporary)) return 1;
    const std::filesystem::path root(temporary);
    int result=0;
    try {
        std::vector<HubAsset> assets{
            {HubAssetKind::TrialSpawner,{-6.5f,9,0}},
            {HubAssetKind::TrialSpawner,{.5f,9,0}},
            {HubAssetKind::TrialSpawner,{6.5f,9,0}}};
        require(validateSpawnerChains(assets).empty(),"legacy spawners remain independent");
        require(linkSpawners(assets,0,1).empty() && linkSpawners(assets,1,2).empty(),
                "sequential links assign stable unique identities");
        require(spawnerById(assets,assets[0].spawner.next)==1
                && spawnerById(assets,assets[1].spawner.next)==2,"stable links resolve to their endpoints");
        require(!spawnerById(assets,0) && !spawnerById(assets,999),"missing or zero identities do not select a spawner");
        const auto linked=assets;
        for (const auto edge : {std::pair{0u,0u}, {2u,0u}, {0u,2u}, {99u,0u}}) {
            require(!linkSpawners(assets,edge.first,edge.second).empty() && assets==linked,
                    "self links, loops, shared followers and invalid indices fail atomically");
        }
        require(linkSpawners(assets,0,std::nullopt).empty() && !assets[0].spawner.next
                && assets[1].spawner.next==linked[1].spawner.next,"unlink only changes the selected outgoing edge");
        assets=linked;
        std::swap(assets[0],assets[2]);
        require(validateSpawnerChains(assets).empty() && spawnerById(assets,linked[0].spawner.id)==2,
                "reordering unrelated placements cannot change the linked identity");
        assets=linked;
        assets.erase(assets.begin()+1);pruneSpawnerLinks(assets);
        require(!assets[0].spawner.next && validateSpawnerChains(assets).empty(),
                "removing a middle fight disconnects its predecessor instead of silently skipping ahead");
        auto copied=linked;remapCopiedSpawners(copied,linked);
        std::vector<HubAsset> both=linked;both.insert(both.end(),copied.begin(),copied.end());
        require(validateSpawnerChains(both).empty() && copied[0].spawner.next==copied[1].spawner.id
                && copied[0].spawner.id!=linked[0].spawner.id,"copied chains keep their own endpoints");
        std::vector<HubAsset> partial{linked[0]};remapCopiedSpawners(partial,linked);
        require(partial[0].spawner.next==0,"partial copies never activate a source outside the copy");
        auto maximum=linked;maximum[0].spawner.id=std::numeric_limits<std::uint32_t>::max();
        std::vector<HubAsset> extra{{HubAssetKind::TrialSpawner,{0,9,4}}};
        maximum.insert(maximum.end(),extra.begin(),extra.end());
        require(linkSpawners(maximum,2,3).empty() && maximum[3].spawner.id>0,
                "identity allocation tolerates imported maximum IDs without wrapping");

        World world;buildBlankSection(world);SectionMetadata meta;meta.lighting=SectionLighting::Dark;
        assets=linked;
        assets[1].chestItems={{ItemId::Jade,3}};
        const auto path=root/"chain.vxs";
        saveSection(world,meta,assets,path);
        const auto loaded=loadSection(path);
        require(loaded.assets==assets && loaded.world.sameContents(world) && loaded.meta.lighting==meta.lighting,
                "section storage preserves links, counts, loot, terrain and darkness");
        const auto original=read(path);
        for (int fault=0;fault<5;++fault) {
            auto invalid=assets;
            if (fault==0) invalid[1].spawner.id=invalid[0].spawner.id;
            if (fault==1) invalid[0].spawner.next=999;
            if (fault==2) invalid[2].spawner.next=invalid[0].spawner.id;
            if (fault==3) invalid[0].spawner.id=0;
            if (fault==4) invalid[0].kind=HubAssetKind::LanternCap;
            require(fails([&]{saveSection(world,meta,invalid,path,true);}) && read(path)==original,
                    "invalid graphs cannot overwrite an existing section");
        }
        const std::size_t first=60+meta.name.size();
        const auto malformed=root/"malformed.vxs";
        for (const auto target : {999u,linked[0].spawner.id}) {
            auto corrupt=original;integer(corrupt,first+52,target);writeChecked(malformed,corrupt);
            require(fails([&]{loadSection(malformed);}),"valid checksums do not excuse dangling or self-referential file links");
        }
        // Strip the two v6 words from each authored spawner to make genuine
        // earlier layouts, preserving each version's terrain and lighting footer.
        for (const auto version : {4u,5u}) {
            auto old=original;std::size_t cursor=first;
            for (const auto& asset:assets) {
                cursor+=48+asset.chestItems.size()*8;
                old.erase(cursor,8);
            }
            if (version==4) old.erase(old.size()-8,4);
            integer(old,8,version);const auto oldPath=root/("legacy-"+std::to_string(version)+".vxs");
            writeChecked(oldPath,old);
            auto expected=assets;for(auto& asset:expected){asset.spawner.id=0;asset.spawner.next=0;}
            const auto restored=loadSection(oldPath);
            require(restored.assets==expected && restored.meta.lighting==(version==4?SectionLighting::Ambient:SectionLighting::Dark),
                    "v4/v5 spawners import independently with their original counts, rewards and lighting");
            require(read(path)==original,"reading legacy files never rewrites the new section");
        }
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';result=1; }
    std::filesystem::remove_all(root);return result;
}
