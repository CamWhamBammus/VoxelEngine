#include "voxel/edit_history.h"

#include "voxel/world.h"

namespace voxel {

EditHistory::EditHistory(std::size_t capacity) : capacity_(capacity)
{
}

bool EditHistory::apply(World& world, glm::ivec3 position, Block block)
{
    if (!isValidBlock(block)) {
        return false;
    }
    const Block before = world.get(position);
    if (!world.set(position, block)) {
        return false;
    }

    redo_.clear();
    if (capacity_ > 0) {
        undo_.push_back({position, before, block});
        if (undo_.size() > capacity_) {
            undo_.pop_front();
        }
    }
    return true;
}

bool EditHistory::undo(World& world)
{
    if (undo_.empty()) {
        return false;
    }
    const Edit edit = undo_.back();
    if (world.get(edit.position) != edit.after
        || !world.set(edit.position, edit.before)) {
        clear();
        return false;
    }
    redo_.push_back(edit);
    undo_.pop_back();
    return true;
}

bool EditHistory::redo(World& world)
{
    if (redo_.empty()) {
        return false;
    }
    const Edit edit = redo_.back();
    if (world.get(edit.position) != edit.before
        || !world.set(edit.position, edit.after)) {
        clear();
        return false;
    }
    undo_.push_back(edit);
    redo_.pop_back();
    return true;
}

void EditHistory::clear()
{
    undo_.clear();
    redo_.clear();
}

} // namespace voxel
