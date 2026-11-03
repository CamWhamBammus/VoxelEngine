#include "world.h"

#include <array>
#include <iostream>
#include <set>
#include <string>

namespace {
int failures=0;
void check(bool value,const std::string& message)
{
    if (!value) {++failures; std::cerr<<"FAIL: "<<message<<'\n';}
}
std::size_t chunkAt(glm::ivec3 cell)
{
    const auto local=(cell-voxel::World::Min)/voxel::Chunk::Size;
    return std::size_t(local.x+voxel::World::Width*(local.z+voxel::World::Width*local.y));
}
}

int main()
{
    using namespace voxel;
    Chunk empty, otherEmpty;
    check(empty.revision()==0 && otherEmpty.revision()==0 && empty==otherEmpty,
          "initial all-air chunks safely share the zero token");
    check(empty.set(0,0,0,Block::Air) && empty.revision()==0,
          "a valid unchanged Chunk write keeps both its true return value and token");
    check(empty.set(0,0,0,Block::Stone) && empty.revision()!=0,
          "the first real voxel edit allocates a token");
    const Chunk snapshot=empty;
    const auto before=empty.revision();
    check(empty.set(0,0,0,Block::Stone) && empty.revision()==before,
          "unchanged writes never invalidate a cached mesh");
    check(!empty.set(-1,0,0,Block::Stone) && !empty.set(Chunk::Size,0,0,Block::Stone)
          && !empty.set(0,-1,0,Block::Stone) && !empty.set(0,0,Chunk::Size,Block::Stone)
          && !empty.set(0,0,0,static_cast<Block>(255)) && empty.revision()==before && empty==snapshot,
          "out-of-bounds or invalid writes do not change token or contents");

    Chunk left=snapshot,right=snapshot;
    check(left.revision()==snapshot.revision() && right.revision()==snapshot.revision(),
          "copying retains the exact content token");
    left.set(1,2,3,Block::IronOre);
    right.set(1,2,3,Block::GoldOre);
    check(left.revision()!=right.revision() && left!=right,
          "divergent clones with matching edit counts cannot share a token");
    right=left;
    check(right==left && right.revision()==left.revision(), "assignment copies content and its token together");
    right.set(1,2,3,Block::Air);
    right.set(1,2,3,Block::IronOre);
    check(right==left && right.revision()!=left.revision(),
          "undoing an edit restores content equality without erasing change history");
    Chunk independentlyBuilt;
    independentlyBuilt.set(0,0,0,Block::Stone);
    check(independentlyBuilt==snapshot && independentlyBuilt.revision()!=snapshot.revision(),
          "content equality excludes revision identity");

    std::set<std::uint64_t> tokens;
    std::array<Chunk,12> chunks;
    for (std::size_t i=0;i<chunks.size();++i) {
        chunks[i]=snapshot;
        for (int edit=0;edit<64;++edit) {
            chunks[i].set(1,1,1,edit%2?Block::CopperOre:Block::IronOre);
            tokens.insert(chunks[i].revision());
        }
    }
    check(tokens.size()==chunks.size()*64, "real edits across separate chunks receive distinct identities");
    Chunk generated;
    generateTerrain(generated,{0,0,0},42);
    const auto generatedRevision=generated.revision();
    generateTerrain(generated,{0,0,0},42);
    check(generated.revision()==generatedRevision,
          "regenerating identical terrain does not invalidate unchanged chunks");

    World original(42);
    const World saved=original;
    bool copiesRetain=true;
    for (std::size_t i=0;i<World::ChunkCount;++i)
        copiesRetain &= original.chunk(i).revision()==saved.chunk(i).revision();
    check(copiesRetain && original.sameContents(saved), "world snapshots retain all per-chunk tokens");
    const glm::ivec3 cell{0,40,0};
    const auto index=chunkAt(cell);
    const auto token=original.chunk(index).revision();
    const auto material=original.get(cell);
    check(!original.set(cell,material) && original.chunk(index).revision()==token,
          "World keeps its false return value for unchanged writes");
    check(!original.set(World::MaxExclusive,Block::Stone)
          && !original.set(cell,static_cast<Block>(255)) && original.chunk(index).revision()==token,
          "invalid World edits preserve the token");
    original.takeDirtyChunks();
    check(original.chunk(index).revision()==token,
          "consuming dirty flags cannot hide an edit from independent renderer snapshots");
    World alternate=original;
    original.set(cell,Block::JadeOre);
    alternate.set(cell,Block::RubyOre);
    check(original.chunk(index).revision()!=alternate.chunk(index).revision(),
          "world clones with one different edit each cannot alias a renderer cache");
    original.set(cell,material);
    check(original.sameContents(saved) && original.chunk(index).revision()!=saved.chunk(index).revision(),
          "saved-world content comparisons ignore revision history");
    alternate=saved;
    bool assignedTokens=true;
    for (std::size_t i=0;i<World::ChunkCount;++i)
        assignedTokens &= alternate.chunk(i).revision()==saved.chunk(i).revision();
    check(assignedTokens && alternate.sameContents(saved),
          "assigning a saved world restores matching tokens with its blocks");
    return failures?1:0;
}
