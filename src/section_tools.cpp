#include "section_tools.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>

namespace voxel {
namespace {
int turns(int value) { return (value % 4 + 4) % 4; }

bool finite(glm::vec3 point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool inside(glm::vec3 point, glm::vec3 low, glm::vec3 high)
{
    return finite(point) && point.x >= low.x && point.y >= low.y && point.z >= low.z
        && point.x < high.x && point.y < high.y && point.z < high.z;
}

bool anchoredIn(const HubAsset& asset, const SectionBox& box)
{
    return inside(asset.position, glm::vec3(box.min), glm::vec3(box.max) + glm::vec3(1));
}

bool validSize(glm::ivec3 size)
{
    const auto room = SectionInteriorMax - SectionInteriorMin;
    return size.x > 0 && size.y > 0 && size.z > 0
        && size.x <= room.x && size.y <= room.y && size.z <= room.z;
}

std::size_t volume(glm::ivec3 size)
{
    return static_cast<std::size_t>(size.x) * size.y * size.z;
}

std::size_t index(glm::ivec3 point, glm::ivec3 size)
{
    return static_cast<std::size_t>(point.x) + size.x * (point.y + size.y * point.z);
}

bool validAsset(const HubAsset& asset)
{
    return static_cast<std::size_t>(asset.kind) < HubAssetCount
        && finite(asset.position) && asset.facing >= 0 && asset.facing < 4
        && validChestItems(asset);
}

bool samePlacement(const HubAsset& a, const HubAsset& b)
{
    return a.kind == b.kind && a.position == b.position && a.facing == b.facing;
}

bool cameraOverlap(glm::ivec3 position, Block after, glm::vec3 camera)
{
    return isSolid(after) && std::floor(static_cast<double>(camera.x)) == position.x
        && std::floor(static_cast<double>(camera.y)) == position.y
        && std::floor(static_cast<double>(camera.z)) == position.z;
}

SectionEditPlan failed(std::span<const HubAsset> assets, std::string error)
{
    return {{}, {assets.begin(), assets.end()}, std::move(error)};
}

glm::ivec3 rotateCell(glm::ivec3 p, glm::ivec3 size, int rotation)
{
    switch (rotation) {
    case 1: return {p.z, p.y, size.x - 1 - p.x};
    case 2: return {size.x - 1 - p.x, p.y, size.z - 1 - p.z};
    case 3: return {size.z - 1 - p.z, p.y, p.x};
    default: return p;
    }
}

glm::vec3 rotateOrigin(glm::vec3 p, glm::ivec3 size, int rotation)
{
    switch (rotation) {
    case 1: return {p.z, p.y, size.x - p.x};
    case 2: return {size.x - p.x, p.y, size.z - p.z};
    case 3: return {size.z - p.z, p.y, p.x};
    default: return p;
    }
}

bool meshInsideRoom(const HubAsset& asset)
{
    const auto mesh = buildHubAsset(asset);
    if (mesh.vertices.empty()) return false;
    // Match the single-model tool's small surface-attachment tolerance.
    const auto low = glm::vec3(SectionInteriorMin) - glm::vec3(.02f);
    const auto high = glm::vec3(SectionInteriorMax) + glm::vec3(.02f);
    return std::all_of(mesh.vertices.begin(), mesh.vertices.end(), [&](const MeshVertex& vertex) {
        const auto p = vertex.position;
        return finite(p) && p.x >= low.x && p.y >= low.y && p.z >= low.z
            && p.x <= high.x && p.y <= high.y && p.z <= high.z;
    });
}

bool validClipboard(const SectionClipboard& clipboard)
{
    if (!validSize(clipboard.size) || clipboard.empty() || clipboard.assets.size() > SectionMaxAssets)
        return false;
    if (!clipboard.blocks.empty() && clipboard.blocks.size() != volume(clipboard.size)) return false;
    if (!std::all_of(clipboard.blocks.begin(), clipboard.blocks.end(), isValidBlock)) return false;
    return std::all_of(clipboard.assets.begin(), clipboard.assets.end(), [&](const HubAsset& asset) {
        return validAsset(asset) && inside(asset.position, glm::vec3(0), glm::vec3(clipboard.size));
    });
}
} // namespace

SectionBox normalizedSectionBox(glm::ivec3 a, glm::ivec3 b)
{
    return {glm::min(a, b), glm::max(a, b)};
}

bool validSectionBox(const SectionBox& box)
{
    return sectionInterior(box.min) && sectionInterior(box.max)
        && box.min.x <= box.max.x && box.min.y <= box.max.y && box.min.z <= box.max.z;
}

bool SectionClipboard::empty() const
{
    return size.x <= 0 || size.y <= 0 || size.z <= 0 || (blocks.empty() && assets.empty());
}

SectionClipboard copySectionBox(const World& world, std::span<const HubAsset> assets,
                                const SectionBox& box)
{
    if (!validSectionBox(box)) return {};
    SectionClipboard result;
    result.size = box.max - box.min + glm::ivec3(1);
    result.blocks.reserve(volume(result.size));
    for (int z = box.min.z; z <= box.max.z; ++z)
        for (int y = box.min.y; y <= box.max.y; ++y)
            for (int x = box.min.x; x <= box.max.x; ++x)
                result.blocks.push_back(world.get({x, y, z}));
    for (auto asset : assets) {
        if (!anchoredIn(asset, box)) continue;
        if (!validAsset(asset) || result.assets.size() == SectionMaxAssets) return {};
        asset.position -= glm::vec3(box.min);
        result.assets.push_back(asset);
    }
    return result;
}

glm::ivec3 rotatedSectionSize(const SectionClipboard& clipboard, int quarterTurns)
{
    if (turns(quarterTurns) % 2) return {clipboard.size.z, clipboard.size.y, clipboard.size.x};
    return clipboard.size;
}

SectionEditPlan planSectionFill(const World& world, std::span<const HubAsset> assets,
                               const SectionBox& box, Block material, SectionFillMode mode,
                               glm::vec3 cameraPosition)
{
    if (!validSectionBox(box)) return failed(assets, "KEEP THE SELECTION INSIDE THE ROOM");
    if (!finite(cameraPosition)) return failed(assets, "INVALID CAMERA POSITION");
    if (mode != SectionFillMode::Solid && mode != SectionFillMode::Hollow && mode != SectionFillMode::Erase)
        return failed(assets, "INVALID BUILD TOOL");
    if (mode != SectionFillMode::Erase && (!isRenderable(material) || material == Block::Bedrock))
        return failed(assets, "SELECT A BUILDING BLOCK FIRST");
    SectionEditPlan plan{{}, {assets.begin(), assets.end()}, {}};
    const Block after = mode == SectionFillMode::Erase ? Block::Air : material;
    for (int z = box.min.z; z <= box.max.z; ++z)
        for (int y = box.min.y; y <= box.max.y; ++y)
            for (int x = box.min.x; x <= box.max.x; ++x) {
                if (mode == SectionFillMode::Hollow && x != box.min.x && x != box.max.x
                    && y != box.min.y && y != box.max.y && z != box.min.z && z != box.max.z) continue;
                const glm::ivec3 position{x, y, z};
                const auto before = world.get(position);
                if (before == after) continue;
                if (cameraOverlap(position, after, cameraPosition))
                    return failed(assets, "MOVE OUT OF THE BLOCKS YOU ARE BUILDING");
                plan.blocks.push_back({position, before, after});
            }
    if (mode == SectionFillMode::Erase)
        std::erase_if(plan.assets, [&](const HubAsset& asset) { return anchoredIn(asset, box); });
    return plan;
}

SectionEditPlan planSectionPaste(const World& world, std::span<const HubAsset> assets,
                                const SectionClipboard& clipboard, glm::ivec3 origin,
                                int quarterTurns, bool includeAir, glm::vec3 cameraPosition,
                                bool emptyOnly)
{
    if (!validClipboard(clipboard)) return failed(assets, "COPY A VALID SELECTION FIRST");
    if (!finite(cameraPosition)) return failed(assets, "INVALID CAMERA POSITION");
    // Check the origin before addition: even malformed int coordinates cannot overflow.
    if (!sectionInterior(origin)) return failed(assets, "THE PASTE MUST FIT INSIDE THE ROOM");
    const int rotation = turns(quarterTurns);
    const auto size = rotatedSectionSize(clipboard, rotation);
    const SectionBox destination{origin, origin + size - glm::ivec3(1)};
    if (!validSectionBox(destination)) return failed(assets, "THE PASTE MUST FIT INSIDE THE ROOM");
    SectionEditPlan plan{{}, {assets.begin(), assets.end()}, {}};
    if (includeAir && !emptyOnly)
        std::erase_if(plan.assets, [&](const HubAsset& asset) { return anchoredIn(asset, destination); });
    for (int z = 0; z < clipboard.size.z; ++z)
        for (int y = 0; y < clipboard.size.y; ++y)
            for (int x = 0; x < clipboard.size.x; ++x) {
                const glm::ivec3 local{x, y, z};
                const Block after = clipboard.blocks.empty() ? Block::Air : clipboard.blocks[index(local, clipboard.size)];
                if ((!includeAir || emptyOnly) && after == Block::Air) continue;
                const auto position = origin + rotateCell(local, clipboard.size, rotation);
                const auto before = world.get(position);
                if (emptyOnly && before != Block::Air) continue;
                if (before == after) continue;
                if (cameraOverlap(position, after, cameraPosition))
                    return failed(assets, "MOVE OUT OF THE BLOCKS YOU ARE PASTING");
                plan.blocks.push_back({position, before, after});
            }
    if (emptyOnly) return plan;
    for (auto asset : clipboard.assets) {
        asset.position = glm::vec3(origin) + rotateOrigin(asset.position, clipboard.size, rotation);
        asset.facing = (asset.facing + rotation) % 4;
        // Rotation can move an origin on the low selection edge to its high
        // edge. Even an inward-facing model needs a saveable interior anchor.
        if (!inside(asset.position, glm::vec3(SectionInteriorMin), glm::vec3(SectionInteriorMax)))
            return failed(assets, "A COPIED MODEL ANCHOR WOULD LEAVE THE ROOM");
        if (!meshInsideRoom(asset)) return failed(assets, "A COPIED MODEL WOULD EXTEND OUTSIDE THE ROOM");
        if (std::any_of(plan.assets.begin(), plan.assets.end(), [&](const HubAsset& present) {
            return samePlacement(asset, present);
        })) continue;
        if (plan.assets.size() >= SectionMaxAssets) return failed(assets, "PASTE WOULD EXCEED THE ASSET LIMIT");
        plan.assets.push_back(asset);
    }
    return plan;
}

} // namespace voxel
