#include "inventory.h"

#include <algorithm>
#include <stdexcept>

namespace voxel {

int Inventory::count(ItemId item) const
{
    if (!validItem(item)) return 0;
    int total = 0;
    for (const auto& slot : slots)
        if (slot.item == item) total += slot.count;
    return total;
}

int Inventory::occupiedSlots() const
{
    return static_cast<int>(std::count_if(slots.begin(), slots.end(),
        [](const ItemStack& slot) { return slot.count > 0; }));
}

bool Inventory::valid() const
{
    return std::all_of(slots.begin(), slots.end(), [](const ItemStack& slot) {
        return validItem(slot.item) && slot.count >= 0 && slot.count <= StackLimit
            && (slot.count != 0 || slot.item == ItemId::Coal);
    });
}

int Inventory::add(ItemId item, int amount)
{
    if (!validItem(item) || amount < 0) throw std::invalid_argument("Invalid item or amount");
    if (!valid()) throw std::logic_error("Invalid inventory");
    for (auto& slot : slots) {
        if (amount == 0) break;
        if (slot.count == 0 || slot.item != item) continue;
        const int taken = std::min(amount, StackLimit - slot.count);
        slot.count += taken;
        amount -= taken;
    }
    for (auto& slot : slots) {
        if (amount == 0) break;
        if (slot.count != 0) continue;
        const int taken = std::min(amount, StackLimit);
        slot = {item, taken};
        amount -= taken;
    }
    return amount;
}

bool Inventory::remove(ItemId item, int amount)
{
    if (!validItem(item) || amount < 0 || !valid() || count(item) < amount) return false;
    for (auto& slot : slots) {
        if (amount == 0) break;
        if (slot.item != item) continue;
        const int taken = std::min(amount, slot.count);
        slot.count -= taken;
        amount -= taken;
        if (slot.count == 0) slot = {};
    }
    return true;
}

bool Inventory::move(int from, int to)
{
    if (from < 0 || from >= SlotCount || to < 0 || to >= SlotCount || from == to || !valid())
        return false;
    auto& source = slots[from];
    auto& target = slots[to];
    if (source.count == 0) return false;
    if (target.count > 0 && source.item == target.item) {
        const int taken = std::min(source.count, StackLimit - target.count);
        if (taken == 0) return false;
        source.count -= taken;
        target.count += taken;
        if (source.count == 0) source = {};
    } else {
        std::swap(source, target);
    }
    return true;
}

void Inventory::sort()
{
    if (!valid()) throw std::logic_error("Invalid inventory");
    std::array<int, ItemCount> totals{};
    for (const auto& slot : slots)
        totals[static_cast<std::size_t>(slot.item)] += slot.count;
    slots = {};
    for (int i = 0; i < ItemCount; ++i) add(static_cast<ItemId>(i), totals[i]);
}

} // namespace voxel
