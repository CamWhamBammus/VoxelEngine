#include "section_editor.h"
#include "game.h"
#include "input.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace voxel {
namespace {
constexpr float BuildReach = 24.0f;
int wrap(int index, int count) { return count > 0 ? (index % count + count) % count : 0; }
void pointCamera(Camera& camera, glm::vec3 direction)
{
    if (glm::length(direction) < .001f) return;
    direction = glm::normalize(direction);
    const auto facing = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z, direction.x) - std::atan2(facing.z, facing.x)),
                  glm::degrees(std::asin(direction.y) - std::asin(facing.y)));
}
std::optional<float> assetHit(glm::vec3 minimum, glm::vec3 maximum, glm::vec3 origin, glm::vec3 direction)
{
    float near = 0, far = BuildReach;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < .00001f) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) return std::nullopt;
        } else {
            float a = (minimum[axis] - origin[axis]) / direction[axis];
            float b = (maximum[axis] - origin[axis]) / direction[axis];
            if (a > b) std::swap(a, b);
            near = std::max(near, a);
            far = std::min(far, b);
            if (near > far) return std::nullopt;
        }
    }
    return near;
}
SectionBox brushBox(const RayHit& hit, int size, bool place)
{
    const int radius = size / 2;
    auto low = hit.position + (place ? hit.normal : glm::ivec3(0)) - glm::ivec3(radius);
    for (int axis = 0; axis < 3; ++axis) if (hit.normal[axis]) {
        low[axis] += radius;
        if ((place && hit.normal[axis] < 0) || (!place && hit.normal[axis] > 0)) low[axis] -= size - 1;
    }
    return {glm::max(low, SectionInteriorMin), glm::min(low + glm::ivec3(size - 1), SectionInteriorMax - 1)};
}
}

SectionEditor::SectionEditor(std::filesystem::path libraryRoot) : libraryRoot_(std::move(libraryRoot))
{
    for (const Block block : PlaceableBlocks)
        palette_[0].push_back({std::string(blockName(block)), block, std::nullopt, SectionMarker::None});
    for (const auto& asset : sectionAssetCatalog())
        if (!isItemPickup(asset.kind))
            palette_[1].push_back({std::string(asset.name), std::nullopt, asset.kind, SectionMarker::None});
    palette_[2] = {{"Start", {}, {}, SectionMarker::Start},
                   {"Checkpoint", {}, {}, SectionMarker::Checkpoint},
                   {"End", {}, {}, SectionMarker::End}};
    for (const auto& item : itemCatalog())
        palette_[3].push_back({std::string(item.name), {}, HubAssetKind::ItemPickup, SectionMarker::None, item.id});
    const std::array blocks{Block::Stone, Block::Grass, Block::Timber, Block::Planks,
        Block::Water, Block::Leaves, Block::Lantern, Block::Crystal, Block::Path};
    for (std::size_t i = 0; i < hotbar_.size(); ++i)
        hotbar_[i] = {std::string(blockName(blocks[i])), blocks[i], {}, SectionMarker::None};
}

bool SectionEditor::menuOpen() const
{
    return screen_ != SectionEditorScreen::Build && screen_ != SectionEditorScreen::Test;
}
std::shared_ptr<const AssetCollision> SectionEditor::assetCollision() const
{
    if (!assetCollision_) assetCollision_ = std::make_shared<AssetCollision>();
    if (collisionRevision_ != assetRevision_) {
        assetCollision_->rebuild(assets_);
        collisionRevision_ = assetRevision_;
    }
    return assetCollision_;
}
void SectionEditor::enter(Game& game)
{
    closeChestContents();
    screen_ = SectionEditorScreen::Worlds;
    selection_ = static_cast<int>(selectedWorld_);
    game.target.reset();
    message_ = "CHOOSE THE WORLD THIS SECTION WILL BELONG TO";
}
bool SectionEditor::leave(Game& game)
{
    if (hasSection_ && dirty_ && !saveDraft(game)) return false;
    closeChestContents();
    game.endSectionLoot();
    preview_.reset(); targetAsset_.reset();
    resetBuildTools();
    hasSection_ = dirty_ = testComplete_ = checkpointReached_ = false;
    assets_.clear(); ++revision_; ++assetRevision_;
    meta_.tested = false;
    return true;
}
void SectionEditor::refreshLibrary()
{
    entries_ = listSections(libraryRoot_, selectedWorld_);
    drafts_ = listSectionDrafts(libraryRoot_);
    std::erase_if(drafts_, [&](const SectionEntry& entry) { return entry.meta.world != selectedWorld_; });
    selection_ = 0;
}
void SectionEditor::newSection(Game& game)
{
    if (hasSection_ && dirty_ && !saveDraft(game)) return;
    buildBlankSection(game.world);
    meta_ = {};
    meta_.world = selectedWorld_;
    assets_.clear(); ++assetRevision_;
    draftPath_.clear();
    acceptedPath_.clear();
    undo_.clear(); redo_.clear();
    resetBuildTools();
    hasSection_ = dirty_ = true;
    testComplete_ = checkpointReached_ = false;
    testElapsed_ = autosaveElapsed_ = 0;
    testFalls_ = 0;
    ++revision_;
    screen_ = SectionEditorScreen::Build;
    game.camera.reset();
    game.camera.setPosition(meta_.spawn + glm::vec3(0, 5, 1));
    pointCamera(game.camera, {0, -.25f, -1});
    message_ = "NEW 64 X 48 X 64 ROOM | E BLOCKS / ASSETS / ITEMS | F8 WALK TEST";
    saveDraft(game);
}
void SectionEditor::openSection(Game& game, const SectionEntry& entry, bool draft)
{
    if (hasSection_ && dirty_ && !saveDraft(game)) return;
    auto document = loadSection(entry.path); // A failed load leaves the current room intact.
    game.world = std::move(document.world);
    meta_ = std::move(document.meta);
    assets_ = std::move(document.assets); ++assetRevision_;
    const auto collision = assetCollision();
    const bool markersNeedRepair = !sectionMarkerSafe(game.world, meta_.spawn, collision.get())
        || !sectionMarkerSafe(game.world, meta_.finish, collision.get())
        || (meta_.checkpoint && !sectionMarkerSafe(game.world, *meta_.checkpoint, collision.get()));
    draftPath_ = draft ? entry.path : std::filesystem::path{};
    acceptedPath_ = draft ? std::filesystem::path{} : entry.path;
    // Reopened accepted sections must be walked again before publishing another revision.
    meta_.tested = false;
    hasSection_ = true; dirty_ = !draft;
    testComplete_ = checkpointReached_ = false;
    testElapsed_ = autosaveElapsed_ = 0; testFalls_ = 0;
    undo_.clear(); redo_.clear(); ++revision_;
    resetBuildTools();
    screen_ = SectionEditorScreen::Build;
    game.camera.setPosition(meta_.spawn + glm::vec3(0, 4, 1));
    pointCamera(game.camera, meta_.finish - meta_.spawn + glm::vec3(0, -8, 0));
    message_ = markersNeedRepair ? "OPENED " + meta_.name + " | MOVE BLOCKED / UNSUPPORTED MARKERS BEFORE F8 TEST"
                                : "OPENED " + meta_.name + " | F8 TEST | F10 ACCEPT A NEW COPY";
}
void SectionEditor::selectLibrary(Game& game)
{
    if (selection_ == 0) newSection(game);
    else if (selection_ <= static_cast<int>(drafts_.size())) openSection(game, drafts_[selection_ - 1], true);
    else {
        const int index = selection_ - 1 - static_cast<int>(drafts_.size());
        if (index >= 0 && index < static_cast<int>(entries_.size())) openSection(game, entries_[index], false);
    }
}
bool SectionEditor::saveDraft(Game& game)
{
    if (!hasSection_) return true;
    try {
        auto draftMetadata = meta_;
        if (draftMetadata.name.find_first_not_of(' ') == std::string::npos)
            draftMetadata.name = "Untitled section";
        if (draftPath_.empty()) draftPath_ = saveSectionDraft(game.world, draftMetadata, assets_, libraryRoot_);
        else saveSection(game.world, draftMetadata, assets_, draftPath_, true);
        dirty_ = false; autosaveElapsed_ = 0;
        message_ = "DRAFT SAVED | " + meta_.name;
        return true;
    } catch (const std::exception& error) {
        message_ = std::string("DRAFT NOT SAVED: ") + error.what();
        return false;
    }
}
void SectionEditor::invalidateTest()
{
    meta_.tested = false; testComplete_ = false; checkpointReached_ = false;
    testElapsed_ = 0; testFalls_ = 0;
}
void SectionEditor::changed(Edit edit)
{
    if (edit.beforeAssets != assets_) ++assetRevision_;
    invalidateTest();
    edit.afterMeta = meta_; edit.afterAssets = assets_;
    undo_.push_back(std::move(edit));
    if (undo_.size() > 256) undo_.pop_front();
    // Large fills are one action, but retaining hundreds of full-room fills
    // should not make the editor's history consume unbounded memory.
    std::size_t cells = 0;
    for (const auto& action : undo_) cells += action.blocks.size();
    while (cells > 1000000 && undo_.size() > 1) {
        cells -= undo_.front().blocks.size();
        undo_.pop_front();
    }
    redo_.clear(); dirty_ = true; ++revision_;
}
void SectionEditor::undo(Game& game, bool redo)
{
    auto& source = redo ? redo_ : undo_;
    auto& destination = redo ? undo_ : redo_;
    if (source.empty()) { message_ = redo ? "NOTHING TO REDO" : "NOTHING TO UNDO"; return; }
    auto edit = std::move(source.back()); source.pop_back();
    if (edit.position) game.world.set(*edit.position, redo ? edit.after : edit.before);
    for (const auto& block : edit.blocks) game.world.set(block.position, redo ? block.after : block.before);
    meta_ = redo ? edit.afterMeta : edit.beforeMeta;
    const auto& restoredAssets = redo ? edit.afterAssets : edit.beforeAssets;
    if (assets_ != restoredAssets) ++assetRevision_;
    assets_ = restoredAssets;
    destination.push_back(std::move(edit));
    invalidateTest(); dirty_ = true; ++revision_;
    message_ = redo ? "REDID EDIT | TEST AGAIN BEFORE ACCEPTING" : "UNDID EDIT | TEST AGAIN BEFORE ACCEPTING";
}
void SectionEditor::update(Game& game, const InputActions& actions, float deltaSeconds)
{
    try {
        if (!actions.active && !actions.closeMenu) return;
        if (screen_ == SectionEditorScreen::ChestContents) {
            updateChestContents(game, actions);
            game.status = message_;
            return;
        }
        if (actions.save && hasSection_) saveDraft(game);
        if (actions.devLibrary || (actions.devClick && *actions.devClick == SectionUiLibrary)) {
            if (hasSection_ && dirty_ && !saveDraft(game)) return;
            if (screen_ == SectionEditorScreen::Test) stopTest(game);
            refreshLibrary(); screen_ = SectionEditorScreen::Library;
            preview_.reset(); game.target.reset(); return;
        }
        if (actions.devTest || (actions.devClick && *actions.devClick == SectionUiTest)) {
            if (screen_ == SectionEditorScreen::Test) stopTest(game);
            else if (hasSection_ && (screen_ == SectionEditorScreen::Build || screen_ == SectionEditorScreen::Palette
                                    || screen_ == SectionEditorScreen::Tools || screen_ == SectionEditorScreen::PasteSettings)) startTest(game);
            return;
        }
        if (actions.devAccept || (actions.devClick && *actions.devClick == SectionUiAccept)) {
            if (hasSection_ && (screen_ == SectionEditorScreen::Build || screen_ == SectionEditorScreen::Test
                                || screen_ == SectionEditorScreen::Palette || screen_ == SectionEditorScreen::Tools
                                || screen_ == SectionEditorScreen::PasteSettings)) openAccept(game);
            return;
        }
        if (menuOpen()) updateMenu(game, actions);
        else if (screen_ == SectionEditorScreen::Test) updateTest(game, actions, deltaSeconds);
        else updateBuild(game, actions);
        if (hasSection_ && dirty_ && std::isfinite(deltaSeconds) && deltaSeconds > 0) {
            autosaveElapsed_ += std::min(deltaSeconds, .1f);
            if (autosaveElapsed_ >= 20) saveDraft(game);
        }
    } catch (const std::exception& error) { message_ = std::string("EDITOR: ") + error.what(); }
    game.status = message_;
}

void SectionEditor::updateMenu(Game& game, const InputActions& actions)
{
    if (screen_ == SectionEditorScreen::Tools) { updateToolsMenu(game, actions); return; }
    if (screen_ == SectionEditorScreen::PasteSettings) { updatePasteSettings(game, actions); return; }
    const int click = actions.devClick.value_or(-1);
    const bool confirm = actions.upgrade || click == SectionUiConfirm;
    const bool back = actions.closeMenu || click == SectionUiBack;
    if (click == SectionUiSave) { saveDraft(game); return; }
    if (screen_ == SectionEditorScreen::Worlds) {
        const int count = static_cast<int>(sectionWorldCatalog().size());
        if (back) return; // B / F7 leaves dev mode through Game.
        if (actions.devMenuStep.x || actions.devMenuStep.y)
            selection_ = wrap(selection_ + actions.devMenuStep.x + actions.devMenuStep.y, count);
        if (click >= SectionUiWorldBase && click < SectionUiWorldBase + count) selection_ = click - SectionUiWorldBase;
        if (confirm || (click >= SectionUiWorldBase && click < SectionUiWorldBase + count)) {
            selectedWorld_ = sectionWorldCatalog()[selection_].id;
            refreshLibrary(); screen_ = SectionEditorScreen::Library;
            message_ = "NEW ROOM OR OPEN A SAVED SECTION";
        }
        return;
    }
    if (screen_ == SectionEditorScreen::Library) {
        if (back) { screen_ = SectionEditorScreen::Worlds; selection_ = static_cast<int>(selectedWorld_); return; }
        const int count = 1 + static_cast<int>(drafts_.size() + entries_.size());
        const int step = actions.devMenuStep.x + actions.devMenuStep.y;
        if (step) selection_ = wrap(selection_ + step, count);
        if (click == SectionUiLibraryPrevious || click == SectionUiLibraryNext) {
            const int page = std::max(0, selection_ - 1) / 6;
            const int delta = click == SectionUiLibraryNext ? 1 : -1;
            selection_ = std::clamp(1 + (page + delta) * 6, count > 1 ? 1 : 0, count - 1);
        }
        bool open = confirm;
        if (click == SectionUiLibraryNew) { selection_ = 0; open = true; }
        else if (click >= SectionUiLibraryDraftBase && click < SectionUiLibraryDraftBase + static_cast<int>(drafts_.size())) {
            selection_ = 1 + click - SectionUiLibraryDraftBase; open = true;
        } else if (click >= SectionUiLibraryEntryBase && click < SectionUiLibraryEntryBase + static_cast<int>(entries_.size())) {
            selection_ = 1 + static_cast<int>(drafts_.size()) + click - SectionUiLibraryEntryBase; open = true;
        }
        if (open) selectLibrary(game);
        return;
    }
    if (screen_ == SectionEditorScreen::Palette) {
        if (back || actions.devPalette) { screen_ = SectionEditorScreen::Build; return; }
        int tab = paletteTab_;
        if (actions.devTabStep) {
            constexpr std::array order{0, 1, 3, 2};
            const int index = static_cast<int>(std::find(order.begin(), order.end(), tab) - order.begin());
            tab = order[wrap(index + actions.devTabStep, static_cast<int>(order.size()))];
        }
        if (click >= SectionUiPaletteTabBase && click < SectionUiPaletteTabBase + 4) tab = click - SectionUiPaletteTabBase;
        if (tab != paletteTab_) { paletteTab_ = tab; paletteSelection_ = 0; }
        if (paletteTab_ == 3 && (click == SectionUiPickupMinus || click == SectionUiPickupPlus)) {
            const int direction = click == SectionUiPickupPlus ? 1 : -1;
            pickupQuantity_ = std::clamp(pickupQuantity_ + direction * (actions.sprint ? 10 : 1), 1, ChestStackLimit);
            message_ = "PICKUP STACK: " + std::to_string(pickupQuantity_) + " | SHIFT +/- CHANGES TEN";
        }
        const int count = static_cast<int>(paletteItems().size());
        const int step = actions.devMenuStep.x + actions.devMenuStep.y * 6;
        if (step) paletteSelection_ = wrap(paletteSelection_ + step, count);
        if (click == SectionUiPalettePrevious || click == SectionUiPaletteNext) {
            const int delta = click == SectionUiPaletteNext ? 1 : -1;
            const int page = std::clamp(paletteSelection_ / 24 + delta, 0, (count - 1) / 24);
            paletteSelection_ = page * 24;
        }
        if (actions.devSlot && *actions.devSlot >= 0 && *actions.devSlot < 9) selectedSlot_ = *actions.devSlot;
        if (click >= SectionUiHotbarBase && click < SectionUiHotbarBase + 9) selectedSlot_ = click - SectionUiHotbarBase;
        const bool itemClick = click >= SectionUiPaletteItemBase && click < SectionUiPaletteItemBase + count;
        if (itemClick) paletteSelection_ = click - SectionUiPaletteItemBase;
        if (confirm || itemClick) {
            hotbar_[selectedSlot_] = paletteItems()[paletteSelection_];
            message_ = "SLOT " + std::to_string(selectedSlot_ + 1) + ": " + hotbar_[selectedSlot_].name;
        }
        return;
    }
    if (screen_ == SectionEditorScreen::Accept) {
        if (back) { screen_ = SectionEditorScreen::Build; return; }
        if (actions.devSelectAll) nameSelected_ = true;
        if (actions.devBackspace && !meta_.name.empty()) {
            if (nameSelected_) meta_.name.clear();
            else meta_.name.pop_back();
            nameSelected_ = false; dirty_ = true;
        }
        for (const unsigned char letter : actions.devText) {
            if (letter < 32 || letter >= 127) continue;
            if (nameSelected_) { meta_.name.clear(); nameSelected_ = false; }
            if (meta_.name.size() < 48) { meta_.name += static_cast<char>(letter); dirty_ = true; }
        }
        const int count = static_cast<int>(sectionCategoryCatalog().size());
        int category = static_cast<int>(meta_.category);
        if (actions.devMenuStep.x || actions.devMenuStep.y) category = wrap(category + actions.devMenuStep.x + actions.devMenuStep.y, count);
        if (click >= SectionUiCategoryBase && click < SectionUiCategoryBase + count) category = click - SectionUiCategoryBase;
        if (category != static_cast<int>(meta_.category)) { meta_.category = sectionCategoryCatalog()[category].id; dirty_ = true; }
        if (confirm) accept(game);
    }
}

void SectionEditor::refreshTarget(Game& game)
{
    game.target = raycast(game.camera.position(), game.camera.forward(), BuildReach,
                         [&](glm::ivec3 cell) { return game.world.get(cell); });
    targetAsset_.reset();
    targetSurface_ = assetCollision()->raycast(game.camera.position(), game.camera.forward(), BuildReach);
    if (targetSurface_ && game.target && targetSurface_->distance > game.target->distance + .01f)
        targetSurface_.reset();
    float nearest = game.target ? game.target->distance + .01f : BuildReach;
    if (boundsRevision_ != revision_) {
        assetBounds_.clear();
        for (const auto& asset : assets_) {
            AssetBounds bounds{glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
            for (const auto& vertex : buildHubAsset(asset).vertices) {
                bounds.minimum = glm::min(bounds.minimum, vertex.position);
                bounds.maximum = glm::max(bounds.maximum, vertex.position);
            }
            assetBounds_.push_back(bounds);
        }
        boundsRevision_ = revision_;
    }
    for (std::size_t i = 0; i < assets_.size(); ++i) {
        const auto hit = assetHit(assetBounds_[i].minimum, assetBounds_[i].maximum, game.camera.position(), game.camera.forward());
        if (hit && *hit < nearest) { targetAsset_ = i; nearest = *hit; }
    }
    preview_.reset();
    const auto& item = hotbar_[selectedSlot_];
    const bool supportedPickup = !item.pickup || (game.target && game.target->normal.y == 1
        && !targetAsset_ && !targetSurface_ && isSolid(game.world.get(game.target->position)));
    if (item.asset && game.target && game.target->normal != glm::ivec3(0) && supportedPickup) {
        const auto cell = game.target->position + game.target->normal;
        if (sectionInterior(cell)) {
            const auto normal = game.target->normal;
            const int wallFacing = normal.x > 0 ? 1 : normal.x < 0 ? 3 : normal.z < 0 ? 2 : 0;
            const int rotation = (facing_ + (normal.y == 0 ? wallFacing : 0)) % 4;
            HubAsset candidate{*item.asset, glm::vec3(0), rotation,
                               isChestAsset(*item.asset) || item.pickup ? 0 : variation_};
            if (item.pickup) candidate.pickupItem = {*item.pickup, pickupQuantity_};
            glm::vec3 minimum(std::numeric_limits<float>::max()), maximum(std::numeric_limits<float>::lowest());
            const auto mesh = buildHubAsset(candidate);
            for (const auto& vertex : mesh.vertices) {
                minimum = glm::min(minimum, vertex.position);
                maximum = glm::max(maximum, vertex.position);
            }
            if (!mesh.vertices.empty()) {
                candidate.position = glm::vec3(cell) + glm::vec3(.5f, 0, .5f);
                const auto support = game.target->position;
                // Attach the model's actual bounds to the selected surface:
                // floor assets stand on it, hanging assets meet the underside,
                // and wall attachments face away from the wall.
                if (normal.y > 0 && game.world.get(support) == Block::Water
                    && (candidate.kind == HubAssetKind::CaveLotus || candidate.kind == HubAssetKind::WaterGrass))
                    candidate.position.y = support.y + 1.0f;
                else if (normal.y > 0) candidate.position.y = support.y + 1.0f - minimum.y;
                else if (normal.y < 0) candidate.position.y = support.y - maximum.y;
                else {
                    candidate.position.y = support.y + .5f;
                    if (normal.x) candidate.position.x = support.x + (normal.x > 0 ? 1.0f - minimum.x : -maximum.x);
                    if (normal.z) candidate.position.z = support.z + (normal.z > 0 ? 1.0f - minimum.z : -maximum.z);
                }
                candidate.position = glm::clamp(candidate.position, glm::vec3(SectionInteriorMin) + glm::vec3(.001f),
                                                 glm::vec3(SectionInteriorMax) - glm::vec3(.001f));
                const auto low = candidate.position + minimum;
                const auto high = candidate.position + maximum;
                const auto roomLow = glm::vec3(SectionInteriorMin) - glm::vec3(.02f);
                const auto roomHigh = glm::vec3(SectionInteriorMax) + glm::vec3(.02f);
                if (glm::all(glm::greaterThanEqual(low, roomLow)) && glm::all(glm::lessThanEqual(high, roomHigh)))
                    preview_ = candidate;
            }
        }
    }
    if (item.block) game.selectedBlock = *item.block;
}
void SectionEditor::updateBuild(Game& game, const InputActions& actions)
{
    // Camera flight is applied by Input before this update; retain the protected room.
    game.camera.setPosition(glm::clamp(game.camera.position(), glm::vec3(SectionInteriorMin) + glm::vec3(.2f),
                                       glm::vec3(SectionInteriorMax) - glm::vec3(.2f)));
    if (actions.devPalette) {
        refreshTarget(game);
        if (targetAsset_ && isChestAsset(assets_[*targetAsset_].kind)) openChestContents(game, *targetAsset_);
        else { screen_ = SectionEditorScreen::Palette; preview_.reset(); game.target.reset(); }
        return;
    }
    if (actions.devSlot && *actions.devSlot >= 0 && *actions.devSlot < 9) selectedSlot_ = *actions.devSlot;
    else if (actions.materialStep || actions.toolStep) selectedSlot_ = wrap(selectedSlot_ + actions.materialStep % 9 + actions.toolStep % 9, 9);
    if (actions.devRotate) {
        if (buildMode_ == SectionBuildMode::Paste) pasteRotation_ = (pasteRotation_ + 1) % 4;
        else facing_ = (facing_ + 1) % 4;
    }
    if (actions.devVariation) {
        if (hotbar_[selectedSlot_].pickup)
            message_ = "PICKUP STACKS ARE SET IN THE ITEMS MENU | E TO COLLECT IN PLAY";
        else if (hotbar_[selectedSlot_].asset && isChestAsset(*hotbar_[selectedSlot_].asset))
            message_ = "CHEST STYLES ARE IN THE ASSET MENU | AIM AT A PLACED CHEST AND PRESS E TO SET ITEMS";
        else ++variation_;
    }
    if (actions.undo || actions.redo) undo(game, actions.redo && !actions.undo);
    refreshTarget(game);
    if (updateBuildTools(game, actions)) return;
    if (!actions.remove && !actions.place && !actions.pick) return;
    Edit edit;
    edit.beforeMeta = meta_; edit.beforeAssets = assets_;
    const auto& item = hotbar_[selectedSlot_];
    if (item.marker != SectionMarker::None && (actions.place || actions.remove)) {
        if (actions.remove) {
            if (item.marker == SectionMarker::Checkpoint && meta_.checkpoint) {
                meta_.checkpoint.reset(); changed(std::move(edit)); message_ = "CHECKPOINT REMOVED";
            } else message_ = "START AND END ARE REQUIRED | RIGHT CLICK A TOP SURFACE TO MOVE A MARKER";
            return;
        }
        const auto collision = assetCollision();
        std::optional<glm::vec3> feet;
        if (targetSurface_ && targetSurface_->normal.y > .2f) {
            // Settle the whole body, not just the ray point: a wide player stands
            // at the uphill contact of a sloped rock or mushroom cap.
            const auto above = targetSurface_->point + glm::vec3(0, Player::Width + .02f, 0);
            constexpr float half = Player::Width * .5f;
            const float drop = collision->sweep(above + glm::vec3(-half, 0, -half),
                                                above + glm::vec3(half, Player::Height, half),
                                                1, -2 * (Player::Width + .02f));
            feet = above + glm::vec3(0, drop, 0);
        } else if (!targetSurface_ && game.target && game.target->normal.y == 1) {
            feet = glm::vec3(game.target->position + glm::ivec3(0, 1, 0)) + glm::vec3(.5f, 0, .5f);
        }
        if (!feet || !sectionMarkerSafe(game.world, *feet, collision.get())) {
            message_ = "MARKERS NEED A CLEAR, SUPPORTED TOP SURFACE WITH HEADROOM"; return;
        }
        if (item.marker == SectionMarker::Start) meta_.spawn = *feet;
        else if (item.marker == SectionMarker::End) meta_.finish = *feet;
        else meta_.checkpoint = *feet;
        changed(std::move(edit)); message_ = "MOVED " + item.name + " | ONE OF EACH PER SECTION";
        refreshTarget(game); return;
    }
    if (targetAsset_ && (actions.remove || actions.pick)) {
        const auto index = *targetAsset_;
        if (actions.pick) {
            if (isItemPickup(assets_[index].kind)) {
                const auto stack = itemPickupStack(assets_[index]);
                const auto item = std::find_if(palette_[3].begin(), palette_[3].end(),
                    [&](const SectionPaletteItem& candidate) { return candidate.pickup == stack.item; });
                if (item != palette_[3].end()) {
                    hotbar_[selectedSlot_] = *item;
                    pickupQuantity_ = std::clamp(stack.count, 1, ChestStackLimit);
                }
            } else {
                const auto item = std::find_if(palette_[1].begin(), palette_[1].end(),
                    [&](const SectionPaletteItem& candidate) { return candidate.asset == assets_[index].kind; });
                if (item != palette_[1].end()) hotbar_[selectedSlot_] = *item;
            }
            facing_ = assets_[index].facing; variation_ = assets_[index].variation;
            message_ = "PICKED " + hotbar_[selectedSlot_].name;
        } else {
            SectionEditPlan plan{{}, assets_, {}};
            plan.assets.erase(plan.assets.begin() + static_cast<std::ptrdiff_t>(index));
            validateAssetEdit(game.world, plan);
            if (!plan.error.empty()) { message_ = plan.error; return; }
            assets_ = std::move(plan.assets);
            changed(std::move(edit)); message_ = "REMOVED ASSET";
        }
        refreshTarget(game); return;
    }
    if (!game.target) { message_ = "AIM AT A BLOCK WITHIN 24 BLOCKS"; return; }
    const auto hit = *game.target;
    if (actions.pick) {
        const auto block = game.world.get(hit.position);
        if (block != Block::Bedrock) hotbar_[selectedSlot_] = {std::string(blockName(block)), block, {}, SectionMarker::None};
        message_ = "PICKED " + hotbar_[selectedSlot_].name; return;
    }
    if (actions.remove) {
        if (!sectionInterior(hit.position)) { message_ = "THE ROOM SHELL IS PROTECTED"; return; }
        edit.position = hit.position; edit.before = game.world.get(hit.position); edit.after = Block::Air;
        if (game.world.set(hit.position, Block::Air)) { changed(std::move(edit)); message_ = "REMOVED BLOCK"; }
    } else if (item.block) {
        const auto placement = checkPlacement(game.world, hit, *item.block, game.camera.position());
        if (placement.failure != PlacementFailure::None || !sectionInterior(placement.position)) {
            message_ = "PLACE INSIDE THE ROOM, IN AN EMPTY CELL AWAY FROM YOUR CAMERA"; return;
        }
        edit.position = placement.position; edit.before = game.world.get(placement.position); edit.after = *item.block;
        if (game.world.set(placement.position, *item.block)) { changed(std::move(edit)); message_ = "PLACED " + item.name; }
    } else if (item.asset) {
        if (!preview_) {
            message_ = item.pickup ? "ITEMS NEED THE TOP OF A SOLID FLOOR INSIDE THE ROOM"
                                   : "AIM AT A BLOCK FACE INSIDE THE ROOM";
            return;
        }
        if (assets_.size() >= SectionMaxAssets) { message_ = "ASSET LIMIT REACHED"; return; }
        const auto duplicate = std::find_if(assets_.begin(), assets_.end(), [&](const HubAsset& asset) {
            return asset.kind == preview_->kind && asset.position == preview_->position && asset.facing == preview_->facing;
        });
        if (duplicate != assets_.end()) { message_ = "THIS ASSET IS ALREADY HERE"; return; }
        SectionEditPlan plan{{}, assets_, {}};
        plan.assets.push_back(*preview_);
        validateAssetEdit(game.world, plan);
        if (!plan.error.empty()) { message_ = plan.error; return; }
        assets_ = std::move(plan.assets); changed(std::move(edit));
        message_ = item.pickup ? "PLACED " + std::to_string(pickupQuantity_) + " " + item.name + " | E TO COLLECT IN PLAY"
            : isChestAsset(*item.asset) ? "PLACED EMPTY " + item.name + " | AIM AT IT AND PRESS E TO SET ITEMS"
                                       : "PLACED " + item.name + " | F8 TEST SOLID SURFACES";
    }
    refreshTarget(game);
}

HubAssetKind SectionEditor::editingChestKind() const
{
    return editingChest_ && *editingChest_ < assets_.size() ? assets_[*editingChest_].kind : HubAssetKind::LootChest;
}

void SectionEditor::selectChestSlot(int slot)
{
    chestSelection_ = std::clamp(slot, 0, ChestSlotCount - 1);
    chestQuantityText_ = std::to_string(std::max(1, chestSlots_[chestSelection_].count));
    chestQuantitySelected_ = true;
}

void SectionEditor::openChestContents(Game& game, std::size_t index)
{
    if (index >= assets_.size() || !isChestAsset(assets_[index].kind)) return;
    editingChest_ = index;
    chestSlots_ = {};
    const auto& items = assets_[index].chestItems;
    std::copy_n(items.begin(), std::min(items.size(), chestSlots_.size()), chestSlots_.begin());
    selectChestSlot(0);
    screen_ = SectionEditorScreen::ChestContents;
    preview_.reset(); game.target.reset();
    message_ = "SELECT A CHEST SLOT, CHOOSE AN ITEM, THEN SET ITS QUANTITY | SAVE APPLIES / ESC CANCELS";
}

void SectionEditor::closeChestContents()
{
    editingChest_.reset();
    chestSlots_ = {};
    selectChestSlot(0);
    if (screen_ == SectionEditorScreen::ChestContents) screen_ = SectionEditorScreen::Build;
}

void SectionEditor::updateChestContents(Game& game, const InputActions& actions)
{
    const int click = actions.devClick.value_or(-1);
    if (actions.closeMenu || click == SectionUiChestCancel || click == SectionUiBack) {
        closeChestContents();
        message_ = "CHEST EDIT CANCELLED | ORIGINAL CONTENTS KEPT";
        refreshTarget(game);
        return;
    }
    if (!editingChest_ || *editingChest_ >= assets_.size() || !isChestAsset(assets_[*editingChest_].kind)) {
        closeChestContents();
        message_ = "CHEST NO LONGER EXISTS";
        return;
    }
    if (actions.save || actions.devTest || actions.devAccept || actions.devLibrary
        || click == SectionUiSave || click == SectionUiTest || click == SectionUiAccept || click == SectionUiLibrary
        || actions.undo || actions.redo) {
        message_ = "SAVE OR CANCEL THE CHEST EDIT FIRST";
        return;
    }
    if (click >= SectionUiChestSlotBase && click < SectionUiChestSlotBase + ChestSlotCount)
        selectChestSlot(click - SectionUiChestSlotBase);
    else if (actions.devMenuStep.x || actions.devMenuStep.y)
        selectChestSlot(wrap(chestSelection_ + actions.devMenuStep.x + actions.devMenuStep.y * 5, ChestSlotCount));
    if (click == SectionUiChestQuantity || actions.devSelectAll) chestQuantitySelected_ = true;

    auto& selected = chestSlots_[chestSelection_];
    if (click >= SectionUiChestItemBase && click < SectionUiChestItemBase + ItemCount) {
        selected = {static_cast<ItemId>(click - SectionUiChestItemBase), std::max(1, selected.count)};
        selectChestSlot(chestSelection_);
        message_ = "ITEM SET | TYPE A QUANTITY FROM 1 TO 99 / SHIFT +/- CHANGES TEN";
    }
    if (click == SectionUiChestRemove) {
        selected = {};
        selectChestSlot(chestSelection_);
        message_ = "SLOT CLEARED | SAVE TO APPLY";
    } else if (click == SectionUiChestClear) {
        chestSlots_ = {};
        selectChestSlot(0);
        message_ = "ALL SLOTS CLEARED | SAVE TO APPLY OR CANCEL TO KEEP THE ORIGINAL";
    }
    if (actions.devBackspace && selected.count > 0) {
        if (chestQuantitySelected_) chestQuantityText_.clear();
        else if (!chestQuantityText_.empty()) chestQuantityText_.pop_back();
        chestQuantitySelected_ = false;
    }
    for (const char character : actions.devText) if (selected.count > 0 && character >= '0' && character <= '9') {
        if (chestQuantitySelected_) chestQuantityText_.clear();
        chestQuantitySelected_ = false;
        if (chestQuantityText_.size() < 2) chestQuantityText_ += character;
    }
    const auto quantity = [&] {
        int count = 0;
        for (const char character : chestQuantityText_) count = count * 10 + character - '0';
        return count;
    };
    if ((click == SectionUiChestMinus || click == SectionUiChestPlus) && selected.count > 0) {
        const int step = (actions.sprint ? 10 : 1) * (click == SectionUiChestPlus ? 1 : -1);
        selected.count = std::clamp(quantity() + step, 1, ChestStackLimit);
        selectChestSlot(chestSelection_);
    }
    const bool validQuantity = quantity() >= 1 && quantity() <= ChestStackLimit;
    if (selected.count > 0 && validQuantity) selected.count = quantity();
    if (selected.count > 0 && !validQuantity) message_ = "QUANTITY MUST BE FROM 1 TO 99";
    if (actions.upgrade || click == SectionUiChestApply || click == SectionUiConfirm) {
        if (selected.count > 0 && !validQuantity) return;
        std::vector<ItemStack> items;
        for (const auto& stack : chestSlots_) if (stack.count > 0) items.push_back(stack);
        auto& chest = assets_[*editingChest_];
        if (items != chest.chestItems) {
            Edit edit;
            edit.beforeMeta = meta_; edit.beforeAssets = assets_;
            chest.chestItems = std::move(items);
            changed(std::move(edit));
            message_ = "CHEST CONTENTS APPLIED | CTRL/CMD+Z UNDO | F8 TEST LOOT";
        } else message_ = "CHEST CONTENTS UNCHANGED";
        closeChestContents();
        refreshTarget(game);
    }
}

std::optional<SectionBox> SectionEditor::buildSelection() const
{
    if (!cornerA_ || !cornerB_) return std::nullopt;
    return normalizedSectionBox(*cornerA_, *cornerB_);
}

void SectionEditor::resetBuildTools()
{
    buildMode_ = SectionBuildMode::Single;
    brushErasing_ = false;
    cornerA_.reset(); cornerB_.reset(); toolPreview_.reset(); previewOrigin_.reset();
    previewPlan_ = {}; previewRevision_ = ~std::uint64_t(0);
    toolPreviewValid_ = false;
    pasteFillGaps_ = false; pasteFloorHeight_.reset(); pinnedPasteOrigin_.reset();
    ++pastePreviewRevision_;
    // Keep the clipboard for reusing structures in another section this session.
}

void SectionEditor::applyPlan(Game& game, SectionEditPlan plan, std::string_view label)
{
    validateAssetEdit(game.world, plan);
    if (!plan.error.empty()) { message_ = plan.error; return; }
    if (plan.blocks.empty() && plan.assets == assets_) { message_ = "NO CHANGES | " + std::string(label); return; }
    Edit edit;
    edit.beforeMeta = meta_; edit.beforeAssets = assets_;
    edit.blocks = std::move(plan.blocks);
    for (const auto& cell : edit.blocks) game.world.set(cell.position, cell.after);
    assets_ = std::move(plan.assets);
    const auto count = edit.blocks.size();
    changed(std::move(edit));
    message_ = std::string(label) + " | " + std::to_string(count) + " BLOCKS | CTRL/CMD+Z UNDO";
}

void SectionEditor::validateAssetEdit(const World& world, SectionEditPlan& plan) const
{
    if (!plan.error.empty() || plan.assets == assets_) return;
    const auto before = assetCollision();
    AssetCollision after;
    after.rebuild(plan.assets);
    // Pasted blocks and models are one edit. Check their combined destination,
    // so replacing a supporting model with a solid block remains possible.
    std::optional<World> destination;
    if (!plan.blocks.empty()) {
        destination = world;
        for (const auto& block : plan.blocks) destination->set(block.position, block.after);
    }
    const World& result = destination ? *destination : world;
    const auto preserved = [&](glm::vec3 marker) {
        return !sectionMarkerSafe(world, marker, before.get()) || sectionMarkerSafe(result, marker, &after);
    };
    if (!preserved(meta_.spawn) || !preserved(meta_.finish) || (meta_.checkpoint && !preserved(*meta_.checkpoint)))
        plan.error = "MOVE START / CHECKPOINT / END BEFORE CHANGING THEIR SUPPORT OR HEADROOM";
}

void SectionEditor::copySelection(Game& game)
{
    const auto box = buildSelection();
    if (!box) { message_ = "SELECT TWO CORNERS FIRST | G TOOLS / J AND K AT YOUR POSITION"; return; }
    clipboard_ = copySectionBox(game.world, assets_, *box);
    ++clipboardRevision_;
    previewRevision_ = ~std::uint64_t(0);
    pasteRotation_ = 0;
    pasteFloorHeight_.reset(); pinnedPasteOrigin_.reset();
    message_ = "COPIED " + std::to_string(clipboard_.size.x) + " X " + std::to_string(clipboard_.size.y)
        + " X " + std::to_string(clipboard_.size.z) + " + " + std::to_string(clipboard_.assets.size())
        + " ASSETS | CTRL/CMD+V TO PREVIEW";
}

void SectionEditor::beginPaste()
{
    if (clipboard_.empty()) { message_ = "COPY A SELECTION FIRST | G TOOLS"; return; }
    buildMode_ = SectionBuildMode::Paste;
    screen_ = SectionEditorScreen::Build;
    preview_.reset();
    previewRevision_ = ~std::uint64_t(0);
    pasteFloorHeight_.reset(); pinnedPasteOrigin_.reset();
    toolPreview_.reset(); previewPlan_ = {}; ++pastePreviewRevision_;
    message_ = pasteFillGaps_ ? "FILL GAPS | LEFT CLICK A FLOOR TOP TO SET ITS HEIGHT"
                            : "PASTE | RIGHT CLICK STAMP / ARROWS + PGUP-DOWN NUDGE / P PIN";
}

void SectionEditor::fillSelection(Game& game, SectionFillMode mode)
{
    const auto box = buildSelection();
    if (!box) { message_ = "SELECT TWO CORNERS FIRST | LEFT / RIGHT IN SELECT MODE, OR J / K"; return; }
    const auto block = hotbar_[selectedSlot_].block;
    if (mode != SectionFillMode::Erase && !block) { message_ = "SELECT A BLOCK IN YOUR HOTBAR FOR FILL OR SHELL"; return; }
    applyPlan(game, planSectionFill(game.world, assets_, *box, block.value_or(Block::Air), mode,
                                   game.camera.position()),
              mode == SectionFillMode::Erase ? "CLEARED SELECTION" : mode == SectionFillMode::Hollow ? "BUILT SHELL" : "FILLED SELECTION");
}

void SectionEditor::updateToolsMenu(Game& game, const InputActions& actions)
{
    if (actions.undo || actions.redo) { undo(game, actions.redo && !actions.undo); return; }
    if (actions.devSlot && *actions.devSlot >= 0 && *actions.devSlot < 9) selectedSlot_ = *actions.devSlot;
    const int click = actions.devClick.value_or(-1);
    if (actions.closeMenu || actions.devTools || click == SectionUiBack) {
        screen_ = SectionEditorScreen::Build;
        refreshTarget(game); refreshToolPreview(game); return;
    }
    if (actions.devPalette) { screen_ = SectionEditorScreen::Palette; return; }
    if (click >= SectionUiModeBase && click < SectionUiModeBase + 3) {
        buildMode_ = static_cast<SectionBuildMode>(click - SectionUiModeBase);
        brushErasing_ = false;
        screen_ = SectionEditorScreen::Build;
        message_ = buildMode_ == SectionBuildMode::Select ? "LEFT CLICK CORNER A / RIGHT CLICK CORNER B | J / K SET AT YOUR POSITION"
            : buildMode_ == SectionBuildMode::Brush ? "CUBE BRUSH | RIGHT ADD / LEFT CLEAR | G CHOOSE SIZE"
            : "SINGLE BLOCKS AND ASSETS | G BUILD TOOLS";
    } else if (click >= SectionUiBrushSizeBase && click < SectionUiBrushSizeBase + 4) {
        constexpr int sizes[]{1,3,5,9};
        brushSize_ = sizes[click - SectionUiBrushSizeBase];
        brushErasing_ = false;
        buildMode_ = SectionBuildMode::Brush;
        message_ = "BRUSH SIZE " + std::to_string(brushSize_) + " | G RETURN TO BUILD";
    } else if (click == SectionUiFill) fillSelection(game, SectionFillMode::Solid);
    else if (click == SectionUiShell) fillSelection(game, SectionFillMode::Hollow);
    else if (click == SectionUiErase) fillSelection(game, SectionFillMode::Erase);
    else if (click == SectionUiCopy) copySelection(game);
    else if (click == SectionUiPaste) beginPaste();
    else if (click == SectionUiPasteSettings) { screen_ = SectionEditorScreen::PasteSettings; }
    else if (click == SectionUiPasteAir) {
        if (!pasteFillGaps_) pasteIncludesAir_ = !pasteIncludesAir_;
        message_ = pasteIncludesAir_ ? "PASTE WITH AIR: REPLACES THE WHOLE BOX AND ITS DECORATIONS"
                                    : "PASTE SOLIDS ONLY: EMPTY CELLS KEEP EXISTING SCENERY";
    } else if (click == SectionUiClearSelection) {
        cornerA_.reset(); cornerB_.reset(); message_ = "SELECTION CLEARED";
    } else if (click == SectionUiHeightDown || click == SectionUiHeightUp) {
        if (auto box = buildSelection()) {
            box->max.y = std::clamp(box->max.y + (click == SectionUiHeightUp ? 1 : -1), box->min.y, SectionInteriorMax.y - 1);
            cornerA_ = box->min; cornerB_ = box->max;
            message_ = "SELECTION HEIGHT " + std::to_string(box->max.y - box->min.y + 1);
        } else message_ = "SELECT TWO CORNERS BEFORE CHANGING HEIGHT";
    }
    if (screen_ == SectionEditorScreen::Build) { refreshTarget(game); refreshToolPreview(game); }
}

void SectionEditor::updatePasteSettings(Game& game, const InputActions& actions)
{
    if (actions.undo || actions.redo) { undo(game, actions.redo && !actions.undo); return; }
    const int click = actions.devClick.value_or(-1);
    if (actions.closeMenu || click == SectionUiBack) { screen_ = SectionEditorScreen::Tools; return; }
    if (actions.devTools || click == SectionUiPasteResume) {
        if (buildMode_ == SectionBuildMode::Paste) screen_ = SectionEditorScreen::Build;
        else beginPaste();
    } else if (click == SectionUiPasteModeBase || click == SectionUiPasteModeBase + 1) {
        pasteFillGaps_ = click == SectionUiPasteModeBase + 1;
        if (pasteFillGaps_) pasteIncludesAir_ = false;
        beginPaste();
    } else if (click == SectionUiPasteAir && !pasteFillGaps_) {
        pasteIncludesAir_ = !pasteIncludesAir_;
        message_ = pasteIncludesAir_ ? "STANDARD PASTE REPLACES THE WHOLE BOX, INCLUDING AIR AND MODELS"
                                    : "STANDARD PASTE SKIPS COPIED AIR";
    }
    if (screen_ == SectionEditorScreen::Build) { refreshTarget(game); refreshToolPreview(game); }
}

void SectionEditor::refreshToolPreview(Game& game)
{
    toolPreview_.reset(); toolPreviewValid_ = false;
    if (buildMode_ != SectionBuildMode::Brush && buildMode_ != SectionBuildMode::Paste) return;
    preview_.reset(); targetAsset_.reset();
    if (buildMode_ == SectionBuildMode::Brush) {
        if (!game.target || game.target->normal == glm::ivec3(0)) return;
        toolPreview_ = brushBox(*game.target, brushSize_, !brushErasing_);
        const auto block = hotbar_[selectedSlot_].block;
        if (block && validSectionBox(*toolPreview_)) {
            const auto cameraCell = glm::ivec3(glm::floor(game.camera.position()));
            const bool inside = glm::all(glm::greaterThanEqual(cameraCell, toolPreview_->min))
                && glm::all(glm::lessThanEqual(cameraCell, toolPreview_->max));
            toolPreviewValid_ = brushErasing_ || !isSolid(*block) || !inside;
        }
        return;
    }
    const auto size = rotatedSectionSize(clipboard_, pasteRotation_);
    glm::ivec3 origin;
    if (pinnedPasteOrigin_) origin = *pinnedPasteOrigin_;
    else if (pasteFillGaps_) {
        if (!pasteFloorHeight_) return;
        // Follow a plane at the picked floor's top, not the next terrain hit.
        // A lower floor at the bottom of a hole cannot pull the stamp down.
        const auto direction = game.camera.forward();
        if (std::abs(direction.y) < .0001f) return;
        const float distance = (float(*pasteFloorHeight_) + 1.f - game.camera.position().y) / direction.y;
        if (!std::isfinite(distance) || distance < 0 || distance > 128.f) return;
        const auto point = game.camera.position() + direction * distance;
        origin = {int(std::floor(point.x)), *pasteFloorHeight_, int(std::floor(point.z))};
    } else {
        if (!game.target || game.target->normal == glm::ivec3(0)) return;
        origin = game.target->position + game.target->normal;
        for (int axis = 0; axis < 3; ++axis) if (game.target->normal[axis] < 0) origin[axis] -= size[axis] - 1;
    }
    toolPreview_ = SectionBox{origin, origin + size - 1};
    const auto cameraCell = glm::ivec3(glm::floor(game.camera.position()));
    if (!previewOrigin_ || *previewOrigin_ != origin || previewRevision_ != revision_
        || previewCameraCell_ != cameraCell || previewRotation_ != pasteRotation_ || previewAir_ != pasteIncludesAir_
        || previewFillGaps_ != pasteFillGaps_) {
        previewPlan_ = planSectionPaste(game.world, assets_, clipboard_, origin, pasteRotation_, pasteIncludesAir_, game.camera.position(), pasteFillGaps_);
        validateAssetEdit(game.world, previewPlan_);
        previewOrigin_ = origin; previewRevision_ = revision_; previewCameraCell_ = cameraCell;
        previewRotation_ = pasteRotation_; previewAir_ = pasteIncludesAir_;
        previewFillGaps_ = pasteFillGaps_; ++pastePreviewRevision_;
    }
    toolPreviewValid_ = previewPlan_.error.empty();
}

void SectionEditor::positionPaste(Game& game, const InputActions& actions)
{
    if (pasteFillGaps_ && (actions.removePressed || actions.devPasteLevel)) {
        if (!game.target || game.target->normal.y != 1 || !isSolid(game.world.get(game.target->position))) {
            message_ = "AIM AT A SOLID FLOOR TOP, THEN LEFT CLICK OR L TO PICK ITS HEIGHT";
        } else {
            auto cell = game.target->position;
            if (!sectionInterior(cell)) cell += game.target->normal;
            if (!sectionInterior(cell)) { message_ = "PICK A FLOOR INSIDE THE ROOM"; return; }
            pasteFloorHeight_ = cell.y; pinnedPasteOrigin_.reset();
            refreshToolPreview(game);
            message_ = "FLOOR HEIGHT " + std::to_string(cell.y) + " LOCKED | RIGHT CLICK FILLS EMPTY CELLS";
        }
    }
    if (actions.devPastePin) {
        if (pinnedPasteOrigin_) {
            pinnedPasteOrigin_.reset();
            message_ = pasteFillGaps_ ? "FOLLOWING AIM AT THE LOCKED FLOOR HEIGHT" : "PASTE FOLLOWS THE AIMED SURFACE";
        } else if (toolPreview_) {
            pinnedPasteOrigin_ = toolPreview_->min;
            message_ = "PASTE PINNED | ARROWS + PGUP-DOWN NUDGE / P FOLLOW AIM";
        } else message_ = pasteFillGaps_ ? "PICK A FLOOR HEIGHT BEFORE PINNING" : "AIM AT A SURFACE BEFORE PINNING";
    }
    if (actions.devPasteNudge != glm::ivec3(0)) {
        if (!pinnedPasteOrigin_ && toolPreview_) pinnedPasteOrigin_ = toolPreview_->min;
        if (!pinnedPasteOrigin_) {
            message_ = pasteFillGaps_ ? "PICK A FLOOR HEIGHT BEFORE NUDGING" : "AIM AT A SURFACE BEFORE NUDGING";
            return;
        }
        const auto direction = game.camera.forward();
        const glm::ivec3 forward = std::abs(direction.x) > std::abs(direction.z)
            ? glm::ivec3(direction.x > 0 ? 1 : -1, 0, 0) : glm::ivec3(0, 0, direction.z > 0 ? 1 : -1);
        const glm::ivec3 right(-forward.z, 0, forward.x);
        const auto steps = glm::clamp(actions.devPasteNudge, glm::ivec3(-1), glm::ivec3(1));
        *pinnedPasteOrigin_ = glm::clamp(*pinnedPasteOrigin_ + right * steps.x - forward * steps.z + glm::ivec3(0,steps.y,0),
                                       SectionInteriorMin - 128, SectionInteriorMax + 128);
        if (pasteFillGaps_) pasteFloorHeight_ = pinnedPasteOrigin_->y;
        message_ = "PASTE PINNED | ARROWS + PGUP-DOWN NUDGE / P FOLLOW AIM";
    }
}

bool SectionEditor::updateBuildTools(Game& game, const InputActions& actions)
{
    if (actions.devTools) {
        screen_ = SectionEditorScreen::Tools; preview_.reset(); game.target.reset(); return true;
    }
    if (actions.devCancel) {
        buildMode_ = SectionBuildMode::Single; toolPreview_.reset();
        pasteFloorHeight_.reset(); pinnedPasteOrigin_.reset();
        message_ = "SINGLE BUILD | SELECTION AND CLIPBOARD KEPT"; return true;
    }
    if (actions.devCornerA || actions.devCornerB) {
        const auto cell = glm::clamp(glm::ivec3(glm::floor(game.camera.position())), SectionInteriorMin, SectionInteriorMax - 1);
        if (actions.devCornerA) cornerA_ = cell;
        if (actions.devCornerB) cornerB_ = cell;
        buildMode_ = SectionBuildMode::Select;
        message_ = std::string("CORNER ") + (actions.devCornerA ? "A" : "B") + " SET AT YOUR POSITION | G FILL / COPY";
    }
    if (actions.devCopy) copySelection(game);
    if (actions.devPaste) beginPaste();
    if (buildMode_ == SectionBuildMode::Brush && (actions.remove || actions.place)) brushErasing_ = actions.remove;
    refreshToolPreview(game);
    if (buildMode_ == SectionBuildMode::Paste) {
        positionPaste(game, actions);
        refreshToolPreview(game);
    }
    if (actions.undo || actions.redo || actions.devCopy || actions.devPaste || actions.devCornerA || actions.devCornerB) return true;
    if (buildMode_ == SectionBuildMode::Paste && (actions.devPastePin || actions.devPasteLevel
        || actions.devPasteNudge != glm::ivec3(0) || (pasteFillGaps_ && actions.removePressed))) return true;
    if (buildMode_ == SectionBuildMode::Single) return false;
    preview_.reset(); targetAsset_.reset();
    if (buildMode_ == SectionBuildMode::Select) {
        if (actions.removePressed || actions.placePressed) {
            if (!game.target) { message_ = "AIM AT A BLOCK, OR USE J / K TO SELECT THE AIR AT YOUR POSITION"; return true; }
            auto cell = game.target->position;
            if (!sectionInterior(cell)) cell += game.target->normal;
            if (!sectionInterior(cell)) { message_ = "SELECT INSIDE THE ROOM"; return true; }
            if (actions.removePressed) cornerA_ = cell;
            else cornerB_ = cell;
            message_ = std::string("CORNER ") + (actions.removePressed ? "A" : "B") + " SET | G FILL / COPY / HEIGHT";
        }
        return true;
    }
    if (buildMode_ == SectionBuildMode::Paste) {
        if (actions.placePressed) {
            if (!toolPreview_) message_ = pasteFillGaps_ && !pasteFloorHeight_ ? "LEFT CLICK A FLOOR TOP TO SET THE HEIGHT FIRST"
                                                                                          : "AIM TOWARD THE PASTE SURFACE OR PIN A PREVIEW";
            else {
                applyPlan(game, std::move(previewPlan_), pasteFillGaps_ ? "FILLED GAPS" : "PASTED STRUCTURE");
                previewRevision_ = ~std::uint64_t(0);
                refreshTarget(game); refreshToolPreview(game);
            }
        }
        return true;
    }
    if (!actions.remove && !actions.place) return true;
    if (!game.target || game.target->normal == glm::ivec3(0)) { message_ = "AIM AT A BLOCK FACE FOR THE BRUSH"; return true; }
    const auto block = hotbar_[selectedSlot_].block;
    if (!block) { message_ = "SELECT A BLOCK FOR THE BRUSH | X FOR SINGLE ASSET PLACEMENT"; return true; }
    auto plan = planSectionFill(game.world, assets_, brushBox(*game.target, brushSize_, !actions.remove),
                               *block, actions.remove ? SectionFillMode::Erase : SectionFillMode::Solid, game.camera.position());
    // Painting builds around existing scenery. The explicit Fill command is
    // available when replacing the contents of a region is intended.
    if (!actions.remove) std::erase_if(plan.blocks, [](const SectionBlockEdit& cell) { return cell.before != Block::Air; });
    applyPlan(game, std::move(plan), actions.remove ? "CLEARED BRUSH" : "PAINTED BRUSH");
    refreshTarget(game); refreshToolPreview(game);
    return true;
}

void SectionEditor::faceMarker(Game& game, glm::vec3 position)
{
    game.player.setAssetCollision(game.sectionSceneCollision());
    if (!game.player.setFeetPosition(game.world, position) || !game.player.grounded())
        throw std::runtime_error("Marker has no supported space for the player");
    game.camera.setPosition(game.player.eyePosition());
    auto direction = meta_.finish - position;
    if (glm::length(direction) < .1f) direction = {0, 0, -1};
    direction.y = -.05f * std::max(1.0f, glm::length(direction));
    pointCamera(game.camera, direction);
}
void SectionEditor::startTest(Game& game)
{
    const auto collision = assetCollision();
    if (!sectionMarkerSafe(game.world, meta_.spawn, collision.get()) || !sectionMarkerSafe(game.world, meta_.finish, collision.get())
        || (meta_.checkpoint && !sectionMarkerSafe(game.world, *meta_.checkpoint, collision.get()))) {
        message_ = "FIX START / CHECKPOINT / END: EACH NEEDS SOLID SUPPORT AND CLEAR HEADROOM"; return;
    }
    if (glm::distance(meta_.spawn, meta_.finish) < 3) { message_ = "PLACE THE END AT LEAST THREE BLOCKS FROM THE START"; return; }
    if (dirty_ && !saveDraft(game)) return;
    buildPosition_ = game.camera.position(); buildDirection_ = game.camera.forward();
    checkpointReached_ = testComplete_ = false; testElapsed_ = 0; testFalls_ = 0;
    faceMarker(game, meta_.spawn);
    screen_ = SectionEditorScreen::Test; game.target.reset(); preview_.reset();
    game.beginSectionLoot(assets_, true);
    message_ = "WALK TO THE GOLD END | R RESET | F8 RETURN TO BUILD";
}
void SectionEditor::stopTest(Game& game)
{
    game.endSectionLoot();
    screen_ = SectionEditorScreen::Build;
    game.camera.setPosition(buildPosition_); pointCamera(game.camera, buildDirection_);
    message_ = testComplete_ ? "TEST PASSED | F10 CHOOSE A CATEGORY AND ACCEPT" : "BACK TO BUILD | COMPLETE A TEST BEFORE ACCEPTING";
}
void SectionEditor::resetTest(Game& game, bool restart)
{
    if (restart) { checkpointReached_ = false; testElapsed_ = 0; testFalls_ = 0; }
    testComplete_ = false;
    faceMarker(game, checkpointReached_ && meta_.checkpoint ? *meta_.checkpoint : meta_.spawn);
    message_ = "RETURNED TO " + std::string(checkpointReached_ ? "CHECKPOINT" : "START");
}
void SectionEditor::updateTest(Game& game, const InputActions& actions, float deltaSeconds)
{
    if (actions.respawn) { resetTest(game, actions.restartCrossing); return; }
    game.player.update(game.world, actions.movement, game.camera.forward(), actions.jump, actions.sprint, deltaSeconds, actions.descend);
    game.camera.setPosition(game.player.eyePosition());
    if (!testComplete_ && std::isfinite(deltaSeconds) && deltaSeconds > 0) testElapsed_ += std::min(deltaSeconds, .1f);
    const auto feet = game.player.feetPosition();
    // The room floor remains usable for descending routes and puzzles. R is
    // available anywhere; only leaving the protected room triggers a fall reset.
    if (feet.y < SectionInteriorMin.y - 2) { ++testFalls_; resetTest(game, false); return; }
    const auto reaches = [&](glm::vec3 marker) {
        const auto delta = feet - marker;
        return game.player.grounded() && std::abs(delta.y) < .6f && delta.x * delta.x + delta.z * delta.z < 1.2f * 1.2f;
    };
    if (meta_.checkpoint && reaches(*meta_.checkpoint)) {
        if (!checkpointReached_) message_ = "CHECKPOINT REACHED | R RETURNS HERE";
        checkpointReached_ = true;
    }
    if (!testComplete_ && reaches(meta_.finish)) {
        testComplete_ = meta_.tested = true; dirty_ = true;
        message_ = "TEST PASSED | F10 ACCEPT SECTION | F8 KEEP BUILDING";
    }
}
void SectionEditor::openAccept(Game& game)
{
    if (!meta_.tested) { message_ = "COMPLETE A WALK TEST AFTER YOUR LAST EDIT BEFORE ACCEPTING"; return; }
    if (screen_ == SectionEditorScreen::Test) stopTest(game);
    screen_ = SectionEditorScreen::Accept;
    nameSelected_ = true;
    message_ = "NAME THE SECTION, CHOOSE A CATEGORY, THEN ENTER TO ACCEPT";
}
void SectionEditor::accept(Game& game)
{
    if (meta_.name.find_first_not_of(' ') == std::string::npos) { message_ = "GIVE YOUR SECTION A NAME"; return; }
    // The durable accepted file is a new revision; an existing published section is never overwritten.
    acceptedPath_ = acceptSection(game.world, meta_, assets_, libraryRoot_);
    dirty_ = true;
    const bool saved = saveDraft(game);
    refreshLibrary(); screen_ = SectionEditorScreen::Library;
    if (saved) message_ = "ACCEPTED " + meta_.name + " | ADDED TO THE SELECTED WORLD AND CATEGORY";
}
} // namespace voxel
