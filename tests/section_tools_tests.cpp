#include "section_tools.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <glm/common.hpp>

namespace {
using namespace voxel;
int failures = 0;
void check(bool condition, const std::string& description)
{
    if (!condition) { std::cerr << description << '\n'; ++failures; }
}
void apply(World& world, const SectionEditPlan& plan)
{
    check(plan.error.empty(), "only a valid plan is applied");
    for (const auto& edit : plan.blocks) {
        check(world.get(edit.position) == edit.before, "plan stores the actual prior block for undo");
        world.set(edit.position, edit.after);
    }
}
bool contains(const SectionEditPlan& plan, glm::ivec3 position, Block material)
{
    return std::any_of(plan.blocks.begin(), plan.blocks.end(), [&](const SectionBlockEdit& edit) {
        return edit.position == position && edit.after == material;
    });
}
void rejected(const SectionEditPlan& plan, const std::vector<HubAsset>& original, const char* description)
{
    check(!plan.error.empty() && plan.blocks.empty() && plan.assets == original, description);
}
constexpr glm::vec3 camera{20.5f, 40.5f, 20.5f};
} // namespace

int main()
{
    using namespace voxel;
    const auto box = normalizedSectionBox({2, 12, 2}, {0, 10, 0});
    check(box.min == glm::ivec3(0, 10, 0) && box.max == glm::ivec3(2, 12, 2),
          "opposite corner order produces the same inclusive selection");
    check(validSectionBox(box) && !validSectionBox({box.max, box.min}),
          "un-normalized reversed corners are rejected");
    check(validSectionBox({SectionInteriorMin, SectionInteriorMax - glm::ivec3(1)}),
          "the entire editable interior is a valid selection");
    check(!validSectionBox({SectionInteriorMin - glm::ivec3(0, 1, 0), SectionInteriorMax - glm::ivec3(1)})
          && !validSectionBox({SectionInteriorMin, SectionInteriorMax}),
          "both protected shell boundaries are excluded");

    World world;
    world.set({1, 11, 1}, Block::CopperOre);
    const std::vector<HubAsset> plants{
        {HubAssetKind::Fern, {.5f, 11.f, .5f}, 0, 13},
        {HubAssetKind::Flowers, {3.f, 11.f, .5f}, 0, 27}
    };
    const auto shell = planSectionFill(world, plants, box, Block::MossStone, SectionFillMode::Hollow,
                                      {1.5f, 11.5f, 1.5f});
    check(shell.error.empty() && shell.blocks.size() == 26 && shell.assets == plants,
          "shell builds only the boundary and leaves models and an interior camera alone");
    check(!contains(shell, {1, 11, 1}, Block::MossStone) && world.get({0, 10, 0}) == Block::Air,
          "planning preserves interior ore and does not mutate the world");
    apply(world, shell);
    check(world.get({1, 11, 1}) == Block::CopperOre, "applying shell preserves existing interior content");
    const auto solid = planSectionFill(world, plants, box, Block::MossStone, SectionFillMode::Solid, camera);
    check(solid.error.empty() && solid.blocks.size() == 1 && contains(solid, {1, 11, 1}, Block::MossStone),
          "solid fill changes the remaining interior and skips matching blocks");
    apply(world, solid);
    check(planSectionFill(world, plants, box, Block::MossStone, SectionFillMode::Solid, camera).blocks.empty(),
          "matching fill is a no-op");
    const auto erased = planSectionFill(world, plants, box, Block::Air, SectionFillMode::Erase, camera);
    check(erased.error.empty() && erased.blocks.size() == 27 && erased.assets.size() == 1
          && erased.assets.front() == plants.back(),
          "erase clears blocks and model anchors while preserving the adjacent unselected model");
    apply(world, erased);
    const auto flat = planSectionFill(world, {}, {{0, 10, 0}, {3, 10, 2}}, Block::Path, SectionFillMode::Hollow, camera);
    check(flat.error.empty() && flat.blocks.size() == 12, "a one-block-high shell builds a complete floor");
    rejected(planSectionFill(world, plants, box, Block::Stone, SectionFillMode::Solid, {2.5f, 12.5f, 2.5f}), plants,
             "camera overlap rejects all edits even after earlier cells were planned");
    rejected(planSectionFill(world, plants, box, Block::Stone, SectionFillMode::Hollow, {.5f, 10.5f, .5f}), plants,
             "a camera on the shell cannot be enclosed by a boundary block");
    check(planSectionFill(world, plants, box, Block::Water, SectionFillMode::Solid, {.5f, 10.5f, .5f}).error.empty(),
          "water fill may overlap the camera because water is not solid");
    rejected(planSectionFill(world, plants, box, Block::Bedrock, SectionFillMode::Solid, camera), plants,
             "protected bedrock cannot be used as a fill material");
    rejected(planSectionFill(world, plants, {SectionInteriorMin, SectionInteriorMax}, Block::Stone, SectionFillMode::Solid, camera), plants,
             "a selection touching the protected shell fails without clipping or partial writes");

    // A rectangular clipboard distinguishes the clockwise transform from its inverse.
    World source;
    const SectionBox sourceBox{{0, 15, 0}, {1, 16, 2}};
    source.set({0, 15, 0}, Block::RootWood);
    source.set({1, 15, 2}, Block::GlowMoss);
    source.set({1, 16, 1}, Block::Water);
    const std::vector<HubAsset> sourceAssets{
        {HubAssetKind::WallBracket, {.5f, 16.f, 1.5f}, 1, 73},
        {HubAssetKind::Fern, {10.5f, 16.f, 1.5f}, 0, 8}
    };
    const auto copied = copySectionBox(source, sourceAssets, sourceBox);
    check(!copied.empty() && copied.size == glm::ivec3(2, 2, 3) && copied.blocks.size() == 12,
          "copy stores the inclusive rectangular volume including air");
    check(copied.blocks[0] == Block::RootWood && copied.blocks[9] == Block::GlowMoss
          && copied.blocks[7] == Block::Water, "clipboard indexing is x then y then z");
    check(copied.assets.size() == 1 && copied.assets.front().position == glm::vec3(.5f, 1.f, 1.5f)
          && copied.assets.front().variation == 73, "copy includes selected models with relative origins and variants");
    check(rotatedSectionSize(copied, 1) == glm::ivec3(3, 2, 2)
          && rotatedSectionSize(copied, -1) == glm::ivec3(3, 2, 2)
          && rotatedSectionSize(copied, 4) == copied.size, "rotation swaps horizontal dimensions and wraps turns");
    check(copySectionBox(source, sourceAssets, {sourceBox.max, sourceBox.min}).empty(),
          "invalid copy yields an empty clipboard");

    World destination;
    const glm::ivec3 origin{-8, 20, -8};
    destination.set(origin + glm::ivec3(1, 0, 1), Block::Brick);
    const std::vector<HubAsset> existing{
        {HubAssetKind::Fern, glm::vec3(origin) + glm::vec3(.5f, 0, .5f), 0, 4},
        {HubAssetKind::Flowers, {12.5f, 20.f, 12.5f}, 0, 5}
    };
    const auto paste = planSectionPaste(destination, existing, copied, origin, 1, false, camera);
    check(paste.error.empty() && paste.blocks.size() == 3 && paste.assets.size() == 3,
          "default paste merges nonair blocks and copied models into existing scenery");
    check(contains(paste, origin + glm::ivec3(0, 0, 1), Block::RootWood)
          && contains(paste, origin + glm::ivec3(2, 0, 0), Block::GlowMoss)
          && contains(paste, origin + glm::ivec3(1, 1, 0), Block::Water),
          "quarter turn matches +Z to +X for a nonsquare volume and retains water");
    check(paste.assets.back().position == glm::vec3(origin) + glm::vec3(1.5f, 1.f, 1.5f)
          && paste.assets.back().facing == 2 && paste.assets.back().variation == 73,
          "model position, facing and variation follow the same quarter turn");
    check(destination.get(origin + glm::ivec3(0, 0, 1)) == Block::Air
          && destination.get(origin + glm::ivec3(1, 0, 1)) == Block::Brick,
          "paste planning leaves source and destination unchanged");
    const auto replace = planSectionPaste(destination, existing, copied, origin, 1, true, camera);
    check(replace.error.empty() && replace.blocks.size() == 4
          && contains(replace, origin + glm::ivec3(1, 0, 1), Block::Air)
          && replace.assets.size() == 2 && replace.assets.front() == existing.back(),
          "include-air paste clears destination gaps and replaces anchored models only inside the volume");
    apply(destination, paste);
    const auto repeated = planSectionPaste(destination, paste.assets, copied, origin, 1, false, camera);
    check(repeated.error.empty() && repeated.blocks.empty() && repeated.assets == paste.assets,
          "repeating a paste creates neither duplicate blocks nor duplicate models");
    auto changedVariant = copied;
    ++changedVariant.assets.front().variation;
    const auto duplicate = planSectionPaste(destination, paste.assets, changedVariant, origin, 1, false, camera);
    check(duplicate.error.empty() && duplicate.assets == paste.assets,
          "varying a copied model does not stack a duplicate at the same anchor and facing");

    const std::array<glm::ivec3, 4> rotatedFirst{{{0, 0, 0}, {0, 0, 1}, {1, 0, 2}, {2, 0, 0}}};
    for (int rotation = 0; rotation < 4; ++rotation) {
        World empty;
        const auto rotated = planSectionPaste(empty, {}, copied, origin, rotation, false, camera);
        check(rotated.error.empty() && contains(rotated, origin + rotatedFirst[rotation], Block::RootWood),
              "all four block rotations preserve the selected corner");
        if (rotated.assets.empty()) { check(false, "rotated model exists"); continue; }
        const auto referenceMesh = buildHubAsset(copied.assets.front());
        const auto pastedMesh = buildHubAsset(rotated.assets.front());
        check(referenceMesh.vertices.size() == pastedMesh.vertices.size(), "rotation keeps the same model mesh");
        for (std::size_t i = 0; i < referenceMesh.vertices.size() && i < pastedMesh.vertices.size(); ++i) {
            auto point = referenceMesh.vertices[i].position;
            if (rotation == 1) point = {point.z, point.y, copied.size.x - point.x};
            else if (rotation == 2) point = {copied.size.x - point.x, point.y, copied.size.z - point.z};
            else if (rotation == 3) point = {copied.size.z - point.z, point.y, point.x};
            const auto delta = glm::abs(pastedMesh.vertices[i].position - (glm::vec3(origin) + point));
            check(delta.x < .0001f && delta.y < .0001f && delta.z < .0001f,
                  "model geometry follows exactly the block clipboard rotation");
        }
    }

    World stairSource;
    stairSource.set({0,15,0}, stairBlock(Block::Stone,2));
    stairSource.set({1,15,2}, stairBlock(Block::Planks,3));
    const auto stairCopy = copySectionBox(stairSource, {}, {{0,15,0},{1,15,2}});
    check(stairCopy.blocks.front() == stairBlock(Block::Stone,2)
          && stairCopy.blocks.back() == stairBlock(Block::Planks,3),
          "copying stair blocks preserves each material and authored facing");
    const std::array<glm::ivec3,4> rotatedLast{{{1,0,2},{2,0,0},{0,0,0},{0,0,1}}};
    for (int rotation = 0; rotation < 4; ++rotation) {
        World stairDestination;
        const auto stairs = planSectionPaste(stairDestination, {}, stairCopy, origin, rotation, false, camera);
        check(stairs.error.empty() && stairs.blocks.size() == 2
              && contains(stairs, origin + rotatedFirst[rotation], stairBlock(Block::Stone,2+rotation))
              && contains(stairs, origin + rotatedLast[rotation], stairBlock(Block::Planks,3+rotation)),
              "every clipboard quarter-turn rotates stair facing together with its position and material");
    }
    check(stairSource.get({0,15,0}) == stairBlock(Block::Stone,2)
          && stairCopy.blocks.front() == stairBlock(Block::Stone,2),
          "rotated stair pastes leave the original section and clipboard orientation unchanged");

    World empty;
    rejected(planSectionPaste(empty, existing, copied, origin, 1, true,
                             glm::vec3(origin) + glm::vec3(2.5f, .5f, .5f)), existing,
             "paste camera collision retains every original model and returns no partial blocks");
    rejected(planSectionPaste(empty, existing, copied, {31, 20, 0}, 1, true, camera), existing,
             "paste is rejected if any corner leaves the editable room");
    rejected(planSectionPaste(empty, existing, copied, {std::numeric_limits<int>::max(), 20, 0}, 1, true, camera), existing,
             "hostile destination coordinates fail without integer overflow");
    auto malformed = copied;
    malformed.blocks.pop_back();
    rejected(planSectionPaste(empty, existing, malformed, origin, 0, true, camera), existing,
             "a truncated clipboard cannot partially erase its destination");
    malformed = copied; malformed.blocks[0] = static_cast<Block>(255);
    rejected(planSectionPaste(empty, existing, malformed, origin, 0, true, camera), existing,
             "unknown block IDs are rejected atomically");
    malformed = copied; malformed.assets.front().position.x = -.5f;
    rejected(planSectionPaste(empty, existing, malformed, origin, 0, true, camera), existing,
             "copied model anchors must lie inside their original selection");
    rejected(planSectionPaste(empty, existing, copied, origin, 0, false,
                             {std::numeric_limits<float>::quiet_NaN(), 20, 0}), existing,
             "invalid camera coordinates cannot bypass overlap validation");

    SectionClipboard single;
    single.size = {1, 1, 1}; single.blocks = {Block::Water};
    const auto edge = planSectionPaste(empty, {}, single, SectionInteriorMax - glm::ivec3(1), 1, false, camera);
    check(edge.error.empty() && edge.blocks.size() == 1, "a one-cell paste may touch the inner face of the protected room shell");
    single.blocks = {Block::Air};
    const auto airOnly = planSectionPaste(destination, existing, single, origin + glm::ivec3(1, 0, 1), 0, false, camera);
    check(airOnly.error.empty() && airOnly.blocks.empty() && airOnly.assets == existing,
          "an all-air selection is a no-op with merge paste");
    check(contains(planSectionPaste(destination, existing, single, origin + glm::ivec3(1, 0, 1), 0, true, camera),
                   origin + glm::ivec3(1, 0, 1), Block::Air),
          "an all-air clipboard can deliberately clear a destination when include-air is enabled");
    single.blocks.clear(); single.assets = {{HubAssetKind::Fern, {.5f, 0, .5f}, 0, 8}};
    const auto modelsOnly = planSectionPaste(empty, {}, single, origin, 0, false, camera);
    check(!single.empty() && modelsOnly.error.empty() && modelsOnly.blocks.empty() && modelsOnly.assets.size() == 1,
          "asset-only clipboards can be pasted without any terrain blocks");
    rejected(planSectionPaste(empty, existing, single, {31, 20, 31}, 0, true, camera), existing,
             "actual mesh bounds protect the shell even when the model anchor fits inside it");

    SectionClipboard wallModel;
    wallModel.size = {1, 4, 1};
    wallModel.blocks.resize(4, Block::Air);
    wallModel.assets = {{HubAssetKind::BlossomVines, {0, 1, .5f}, 1, 0}};
    check(planSectionPaste(empty, {}, wallModel, {0, 20, 30}, 1, false, camera).error.empty(),
          "a rotated wall model can face back into the room from an interior anchor");
    rejected(planSectionPaste(empty, existing, wallModel, {0, 20, 31}, 1, false, camera), existing,
             "rotation cannot move a model anchor onto the exclusive room edge and break section saving");

    const std::vector<HubAsset> full(SectionMaxAssets, existing.back());
    rejected(planSectionPaste(empty, full, single, origin, 0, false, camera), full,
             "asset limit rejection is atomic");
    const std::vector<HubAsset> replacedFull(SectionMaxAssets, {HubAssetKind::Flowers, glm::vec3(origin) + glm::vec3(.5f, 0, .5f), 0, 5});
    const auto freed = planSectionPaste(empty, replacedFull, single, origin, 0, true, camera);
    check(freed.error.empty() && freed.assets.size() == 1,
          "include-air paste applies the model limit after removing replaced destination models");

    // A large floor stencil overlaps an irregular hole without replacing any
    // surrounding material, even when the normal include-air toggle is on.
    SectionClipboard floor;
    floor.size = {5, 1, 4};
    floor.blocks.resize(20, Block::Path);
    floor.assets = {{HubAssetKind::Fern, {.5f, 0, .5f}, 0, 9}};
    World brokenFloor;
    const glm::ivec3 floorOrigin{-12, 18, -10};
    const std::array<glm::ivec3, 4> holes{{{1, 0, 1}, {2, 0, 1}, {2, 0, 2}, {3, 0, 2}}};
    const std::array<Block, 6> materials{Block::Brick, Block::Water, Block::Leaves,
                                      Block::CopperOre, Block::Bedrock, Block::Planks};
    for (int z = 0; z < floor.size.z; ++z)
        for (int x = 0; x < floor.size.x; ++x) {
            const glm::ivec3 local{x, 0, z};
            if (std::find(holes.begin(), holes.end(), local) != holes.end()) continue;
            brokenFloor.set(floorOrigin + local, materials[(x + z * floor.size.x) % materials.size()]);
        }
    const std::vector<HubAsset> floorModels{
        {HubAssetKind::Flowers, glm::vec3(floorOrigin) + glm::vec3(.5f, 0, .5f), 0, 10},
        existing.back()
    };
    const auto gaps = planSectionPaste(brokenFloor, floorModels, floor, floorOrigin, 0, true,
                                       glm::vec3(floorOrigin) + glm::vec3(.5f), true);
    check(gaps.error.empty() && gaps.blocks.size() == holes.size() && gaps.assets == floorModels,
          "fill gaps changes only the smaller hole, preserves all models and ignores include-air");
    for (const auto hole : holes)
        check(contains(gaps, floorOrigin + hole, Block::Path), "fill gaps plans every irregular hole cell");
    check(std::all_of(gaps.blocks.begin(), gaps.blocks.end(), [&](const SectionBlockEdit& edit) {
        return edit.before == Block::Air && edit.after == Block::Path
            && std::find(holes.begin(), holes.end(), edit.position - floorOrigin) != holes.end();
    }), "fill gaps never schedules a write to water, foliage, ore, bedrock or other existing blocks");
    rejected(planSectionPaste(brokenFloor, floorModels, floor, floorOrigin, 0, false,
                              glm::vec3(floorOrigin + holes.back()) + glm::vec3(.5f), true), floorModels,
             "a camera in a real gap rejects the whole repair after earlier holes were planned");
    apply(brokenFloor, gaps);
    for (int z = 0; z < floor.size.z; ++z)
        for (int x = 0; x < floor.size.x; ++x) {
            const glm::ivec3 local{x, 0, z};
            const bool hole = std::find(holes.begin(), holes.end(), local) != holes.end();
            const auto expected = hole ? Block::Path : materials[(x + z * floor.size.x) % materials.size()];
            check(brokenFloor.get(floorOrigin + local) == expected,
                  "applied repair merges cleanly with all surrounding materials");
        }
    const auto filled = planSectionPaste(brokenFloor, floorModels, floor, floorOrigin, 0, true, camera, true);
    check(filled.error.empty() && filled.blocks.empty() && filled.assets == floorModels,
          "a fully overlapped fill-gaps stamp is a no-op");

    for (int rotation = 0; rotation < 4; ++rotation) {
        World rotatedFloor;
        rotatedFloor.set(origin + rotatedFirst[rotation], Block::Clay);
        const auto gapsRotated = planSectionPaste(rotatedFloor, existing, copied, origin,
                                                  rotation, true, camera, true);
        check(gapsRotated.error.empty() && gapsRotated.blocks.size() == 2 && gapsRotated.assets == existing
              && !contains(gapsRotated, origin + rotatedFirst[rotation], Block::RootWood),
              "all four fill-gaps rotations preserve the existing corner and ignore copied models");
        const auto reference = planSectionPaste(empty, {}, copied, origin, rotation, false, camera);
        for (const auto& edit : reference.blocks) {
            if (edit.position == origin + rotatedFirst[rotation]) continue;
            check(contains(gapsRotated, edit.position, edit.after),
                  "fill-gaps rotation places each remaining material at the regular rotated location");
        }
    }
    single.assets.clear();
    single.blocks = {Block::Air};
    const auto gapsAir = planSectionPaste(brokenFloor, floorModels, single, floorOrigin, 0, true, camera, true);
    check(gapsAir.error.empty() && gapsAir.blocks.empty() && gapsAir.assets == floorModels,
          "an all-air fill-gaps clipboard cannot erase terrain or models");
    single.blocks.clear();
    single.assets = {{HubAssetKind::Fern, {.5f, 0, .5f}, 0, 8}};
    const auto gapsModels = planSectionPaste(empty, full, single, {31, 20, 31}, 0, true, camera, true);
    check(gapsModels.error.empty() && gapsModels.blocks.empty() && gapsModels.assets == full,
          "fill gaps ignores copied model geometry and asset limits while retaining existing models");
    rejected(planSectionPaste(empty, floorModels, floor, {30, 20, 0}, 0, false, camera, true), floorModels,
             "fill gaps still rejects a stamp extending beyond the protected shell atomically");
    const std::vector<HubAsset> rewardAssets{
        {HubAssetKind::CrystalChest, {1, 15, 1}, 0, 0,
            {{ItemId::Ruby, 7}, {ItemId::Key, 2}, {ItemId::CopperOre, 99}}},
        {HubAssetKind::KeyPickup, {2.5f, 15, 1}, 0, 0}
    };
    auto rewards = copySectionBox(empty, rewardAssets, {{0, 15, 0}, {3, 17, 3}});
    check(rewards.assets.size() == 2 && rewards.assets[0].chestItems == rewardAssets[0].chestItems,
          "copy retains authored rewards and separate key pickups");
    for (int rotation = 0; rotation < 4; ++rotation) {
        auto rewardPaste = planSectionPaste(empty, {}, rewards, {-10, 20, -10}, rotation, false, camera);
        check(rewardPaste.error.empty() && rewardPaste.assets.size() == 2
              && rewardPaste.assets[0].chestItems == rewardAssets[0].chestItems,
              "all paste rotations retain exact item IDs, quantities, and chest appearance");
        if (rewardPaste.assets.empty()) continue;
        rewardPaste.assets[0].chestItems[0].count = 1;
        check(rewards.assets[0].chestItems[0].count == 7 && rewardAssets[0].chestItems[0].count == 7,
              "pasted chests own independent reward lists");
    }
    rewards.assets[0].chestItems[0].count = ChestStackLimit + 1;
    rejected(planSectionPaste(empty, existing, rewards, {-10, 20, -10}, 0, true, camera), existing,
             "invalid copied chest counts fail before replacing any destination content");
    rewards.assets[0].chestItems.clear();
    rewards.assets[1].chestItems = {{ItemId::Key, 1}};
    rejected(planSectionPaste(empty, existing, rewards, {-10, 20, -10}, 0, true, camera), existing,
             "pickup assets cannot carry hidden chest reward lists");
    rewards.assets[1] = {HubAssetKind::ItemPickup, {2.5f, 0, 1}, 0, 0, {}, {ItemId::Amethyst, 37}};
    for (int rotation = 0; rotation < 4; ++rotation) {
        auto pickupPaste = planSectionPaste(empty, {}, rewards, {-10, 20, -10}, rotation, false, camera);
        check(pickupPaste.error.empty() && pickupPaste.assets.size() == 2
              && pickupPaste.assets[1].pickupItem == ItemStack{ItemId::Amethyst, 37},
              "rotated copies preserve exact ground pickup item and quantity");
        if (pickupPaste.assets.size() != 2) continue;
        pickupPaste.assets[1].pickupItem.count = 1;
        check(rewards.assets[1].pickupItem.count == 37, "pasted pickups have independent quantities");
    }
    rewards.assets[1].pickupItem.count = 100;
    rejected(planSectionPaste(empty, existing, rewards, {-10, 20, -10}, 0, true, camera), existing,
             "invalid pickup quantities cannot enter a section through the clipboard");
    if (failures == 0) std::cout << "Section selection, bulk edits and clipboard tests passed\n";
    return failures ? 1 : 0;
}
