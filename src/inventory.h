#pragma once

#include "items.h"
#include <array>

namespace voxel {

struct Inventory {
    static constexpr int SlotCount = 20;
    static constexpr int StackLimit = 99;
    std::array<ItemStack, SlotCount> slots{};

    int count(ItemId item) const;
    int occupiedSlots() const;
    bool valid() const;
    // Fill existing stacks first. Return anything that did not fit.
    int add(ItemId item, int amount);
    // Removing more than is held changes nothing.
    bool remove(ItemId item, int amount);
    // Merge matching stacks, or move/swap different stacks.
    bool move(int from, int to);
    void sort();
    bool operator==(const Inventory&) const = default;
};

} // namespace voxel
