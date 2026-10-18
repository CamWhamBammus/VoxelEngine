#include "editing.h"
#include "world.h"

#include <iostream>
#include <limits>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
} // namespace

int main()
{
    using voxel::Block;
    using voxel::EditHistory;
    using voxel::World;
    const glm::ivec3 left{-1, 14, 1};
    const glm::ivec3 right{0, 14, 1};
    World world;
    world.set(left, Block::Air);
    world.set(right, Block::Air);
    world.takeDirtyChunks();
    EditHistory history;

    check(!history.undo(world) && !history.redo(world), "empty history cannot undo or redo");
    check(history.apply(world, left, Block::Grass), "first edit succeeds");
    check(history.undoCount() == 1 && history.redoCount() == 0, "successful edit is recorded");
    check(world.takeDirtyChunks() == std::vector<std::size_t>({39, 40}),
        "history edits preserve seam dirty tracking");
    check(history.apply(world, right, Block::Stone), "second edit succeeds");
    world.takeDirtyChunks();
    check(history.undo(world) && world.get(right) == Block::Air,
        "undo restores the second voxel");
    check(history.undoCount() == 1 && history.redoCount() == 1, "undo updates stack counts");
    check(world.takeDirtyChunks() == std::vector<std::size_t>({39, 40}),
        "undo dirties both chunks at a seam");

    check(!history.apply(world, left, Block::Grass), "no-op edit returns false");
    for (const Block invalid : {static_cast<Block>(voxel::BlockCount), static_cast<Block>(255)}) {
        check(!history.apply(world, right, invalid), "invalid material is rejected");
    }
    for (const glm::ivec3 outside : {glm::ivec3(-65, 0, 0), glm::ivec3(80, 0, 0),
             glm::ivec3(0, -1, 0), glm::ivec3(0, 64, 0), glm::ivec3(0, 0, -65),
             glm::ivec3(0, 0, 80), glm::ivec3(std::numeric_limits<int>::min(), 0, 0),
             glm::ivec3(0, 0, std::numeric_limits<int>::max())}) {
        check(!history.apply(world, outside, Block::Stone), "out-of-world edit is rejected");
    }
    check(history.undoCount() == 1 && history.redoCount() == 1,
        "failed edits preserve undo and redo stacks");
    check(world.get(right) == Block::Air && world.takeDirtyChunks().empty(),
        "failed edits preserve world state and dirty tracking");

    check(history.undo(world) && world.get(left) == Block::Air,
        "repeated undo restores the original first voxel");
    check(!history.undo(world) && history.redoCount() == 2,
        "exhausted undo preserves redo history");
    check(history.redo(world) && world.get(left) == Block::Grass,
        "redo replays edits in original order");
    check(history.redo(world) && world.get(right) == Block::Stone,
        "repeated redo restores the second edit");
    check(!history.redo(world) && history.undoCount() == 2,
        "exhausted redo preserves undo history");
    check(history.undo(world), "undo before branching succeeds");
    check(history.apply(world, right, Block::Sand) && history.redoCount() == 0,
        "successful new edit clears the redo branch");
    check(!history.redo(world) && world.get(right) == Block::Sand,
        "discarded branch cannot be replayed");
    history.clear();
    check(history.undoCount() == 0 && history.redoCount() == 0
        && world.get(left) == Block::Grass && world.get(right) == Block::Sand,
        "clear removes history without changing the world");

    EditHistory bounded(2);
    world.set(left, Block::Air);
    bounded.apply(world, left, Block::Grass);
    bounded.apply(world, left, Block::Dirt);
    bounded.apply(world, left, Block::Stone);
    check(bounded.undoCount() == 2, "history respects its capacity");
    check(bounded.undo(world) && world.get(left) == Block::Dirt, "newest retained edit can undo");
    check(bounded.undo(world) && world.get(left) == Block::Grass, "oldest retained edit can undo");
    check(!bounded.undo(world) && world.get(left) == Block::Grass,
        "evicted edit cannot undo");
    check(bounded.redo(world) && bounded.redo(world) && world.get(left) == Block::Stone,
        "bounded history supports complete redo");
    check(bounded.undoCount() == 2 && bounded.redoCount() == 0,
        "redo preserves the capacity limit");

    EditHistory disabled(0);
    check(disabled.apply(world, left, Block::Sand) && world.get(left) == Block::Sand,
        "zero capacity still applies edits");
    check(disabled.undoCount() == 0 && disabled.redoCount() == 0
        && !disabled.undo(world) && !disabled.redo(world), "zero capacity stores no history");

    for (const glm::ivec3 edge : {voxel::World::Min, voxel::World::MaxExclusive - glm::ivec3(1)}) {
        const Block before = world.get(edge);
        const Block after = before == Block::Air ? Block::Stone : Block::Air;
        check(history.apply(world, edge, after) && world.get(edge) == after,
            "finite world edge can be edited");
        check(history.undo(world) && world.get(edge) == before,
            "finite world edge restores its previous material");
        history.clear();
    }

    world.set(left, Block::Air);
    world.set(right, Block::Air);
    history.apply(world, left, Block::Grass);
    history.apply(world, right, Block::Stone);
    world.set(right, Block::Sand);
    world.takeDirtyChunks();
    check(!history.undo(world) && world.get(right) == Block::Sand,
        "undo does not overwrite an external edit");
    check(history.undoCount() == 0 && history.redoCount() == 0,
        "undo conflict clears all history");
    check(world.get(left) == Block::Grass && world.takeDirtyChunks().empty(),
        "undo conflict changes no voxels or dirty flags");

    world.set(left, Block::Air);
    history.apply(world, left, Block::Grass);
    history.undo(world);
    world.set(left, Block::Dirt);
    world.takeDirtyChunks();
    check(!history.redo(world) && world.get(left) == Block::Dirt,
        "redo does not overwrite an external edit");
    check(history.undoCount() == 0 && history.redoCount() == 0
        && world.takeDirtyChunks().empty(), "redo conflict clears history without dirtying chunks");

    if (failures == 0) {
        std::cout << "Edit history tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
