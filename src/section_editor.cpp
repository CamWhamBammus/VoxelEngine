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
}

SectionEditor::SectionEditor(std::filesystem::path libraryRoot) : libraryRoot_(std::move(libraryRoot))
{
    for (const Block block : PlaceableBlocks)
        palette_[0].push_back({std::string(blockName(block)), block, std::nullopt, SectionMarker::None});
    for (const auto& asset : sectionAssetCatalog())
        palette_[1].push_back({std::string(asset.name), std::nullopt, asset.kind, SectionMarker::None});
    palette_[2] = {{"Start", {}, {}, SectionMarker::Start},
                   {"Checkpoint", {}, {}, SectionMarker::Checkpoint},
                   {"End", {}, {}, SectionMarker::End}};
    const std::array blocks{Block::Stone, Block::Grass, Block::Timber, Block::Planks,
        Block::Water, Block::Leaves, Block::Lantern, Block::Crystal, Block::Path};
    for (std::size_t i = 0; i < hotbar_.size(); ++i)
        hotbar_[i] = {std::string(blockName(blocks[i])), blocks[i], {}, SectionMarker::None};
}

bool SectionEditor::menuOpen() const
{
    return screen_ != SectionEditorScreen::Build && screen_ != SectionEditorScreen::Test;
}
void SectionEditor::enter(Game& game)
{
    screen_ = SectionEditorScreen::Worlds;
    selection_ = static_cast<int>(selectedWorld_);
    game.target.reset();
    message_ = "CHOOSE THE WORLD THIS SECTION WILL BELONG TO";
}
bool SectionEditor::leave(Game& game)
{
    if (hasSection_ && dirty_ && !saveDraft(game)) return false;
    preview_.reset(); targetAsset_.reset();
    hasSection_ = dirty_ = testComplete_ = checkpointReached_ = false;
    assets_.clear(); ++revision_;
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
    assets_.clear();
    draftPath_.clear();
    acceptedPath_.clear();
    undo_.clear(); redo_.clear();
    hasSection_ = dirty_ = true;
    testComplete_ = checkpointReached_ = false;
    testElapsed_ = autosaveElapsed_ = 0;
    testFalls_ = 0;
    ++revision_;
    screen_ = SectionEditorScreen::Build;
    game.camera.reset();
    game.camera.setPosition(meta_.spawn + glm::vec3(0, 5, 1));
    pointCamera(game.camera, {0, -.25f, -1});
    message_ = "NEW 64 X 48 X 64 ROOM | E BLOCKS / ASSETS | F8 WALK TEST";
    saveDraft(game);
}
void SectionEditor::openSection(Game& game, const SectionEntry& entry, bool draft)
{
    if (hasSection_ && dirty_ && !saveDraft(game)) return;
    auto document = loadSection(entry.path); // A failed load leaves the current room intact.
    game.world = std::move(document.world);
    meta_ = std::move(document.meta);
    assets_ = std::move(document.assets);
    draftPath_ = draft ? entry.path : std::filesystem::path{};
    acceptedPath_ = draft ? std::filesystem::path{} : entry.path;
    // Reopened accepted sections must be walked again before publishing another revision.
    meta_.tested = false;
    hasSection_ = true; dirty_ = !draft;
    testComplete_ = checkpointReached_ = false;
    testElapsed_ = autosaveElapsed_ = 0; testFalls_ = 0;
    undo_.clear(); redo_.clear(); ++revision_;
    screen_ = SectionEditorScreen::Build;
    game.camera.setPosition(meta_.spawn + glm::vec3(0, 4, 1));
    pointCamera(game.camera, meta_.finish - meta_.spawn + glm::vec3(0, -8, 0));
    message_ = "OPENED " + meta_.name + " | F8 TEST | F10 ACCEPT A NEW COPY";
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
    invalidateTest();
    edit.afterMeta = meta_; edit.afterAssets = assets_;
    undo_.push_back(std::move(edit));
    if (undo_.size() > 256) undo_.pop_front();
    redo_.clear(); dirty_ = true; ++revision_;
}
void SectionEditor::undo(Game& game, bool redo)
{
    auto& source = redo ? redo_ : undo_;
    auto& destination = redo ? undo_ : redo_;
    if (source.empty()) { message_ = redo ? "NOTHING TO REDO" : "NOTHING TO UNDO"; return; }
    auto edit = std::move(source.back()); source.pop_back();
    if (edit.position) game.world.set(*edit.position, redo ? edit.after : edit.before);
    meta_ = redo ? edit.afterMeta : edit.beforeMeta;
    assets_ = redo ? edit.afterAssets : edit.beforeAssets;
    destination.push_back(std::move(edit));
    invalidateTest(); dirty_ = true; ++revision_;
    message_ = redo ? "REDID EDIT | TEST AGAIN BEFORE ACCEPTING" : "UNDID EDIT | TEST AGAIN BEFORE ACCEPTING";
}
void SectionEditor::update(Game& game, const InputActions& actions, float deltaSeconds)
{
    try {
        if (!actions.active && !actions.closeMenu) return;
        if (actions.save && hasSection_) saveDraft(game);
        if (actions.devLibrary || (actions.devClick && *actions.devClick == SectionUiLibrary)) {
            if (hasSection_ && dirty_ && !saveDraft(game)) return;
            if (screen_ == SectionEditorScreen::Test) stopTest(game);
            refreshLibrary(); screen_ = SectionEditorScreen::Library;
            preview_.reset(); game.target.reset(); return;
        }
        if (actions.devTest || (actions.devClick && *actions.devClick == SectionUiTest)) {
            if (screen_ == SectionEditorScreen::Test) stopTest(game);
            else if (hasSection_ && (screen_ == SectionEditorScreen::Build || screen_ == SectionEditorScreen::Palette)) startTest(game);
            return;
        }
        if (actions.devAccept || (actions.devClick && *actions.devClick == SectionUiAccept)) {
            if (hasSection_ && (screen_ == SectionEditorScreen::Build || screen_ == SectionEditorScreen::Test
                                || screen_ == SectionEditorScreen::Palette)) openAccept(game);
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
        if (actions.devTabStep) tab = wrap(tab + actions.devTabStep, 3);
        if (click >= SectionUiPaletteTabBase && click < SectionUiPaletteTabBase + 3) tab = click - SectionUiPaletteTabBase;
        if (tab != paletteTab_) { paletteTab_ = tab; paletteSelection_ = 0; }
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
    if (item.asset && game.target && game.target->normal != glm::ivec3(0)) {
        const auto cell = game.target->position + game.target->normal;
        if (sectionInterior(cell)) {
            const auto normal = game.target->normal;
            const int wallFacing = normal.x > 0 ? 1 : normal.x < 0 ? 3 : normal.z < 0 ? 2 : 0;
            const int rotation = (facing_ + (normal.y == 0 ? wallFacing : 0)) % 4;
            HubAsset candidate{*item.asset, glm::vec3(0), rotation, variation_};
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
                if (normal.y > 0) candidate.position.y = support.y + 1.0f - minimum.y;
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
    if (actions.devPalette) { screen_ = SectionEditorScreen::Palette; preview_.reset(); game.target.reset(); return; }
    if (actions.devSlot && *actions.devSlot >= 0 && *actions.devSlot < 9) selectedSlot_ = *actions.devSlot;
    else if (actions.materialStep || actions.toolStep) selectedSlot_ = wrap(selectedSlot_ + ((actions.materialStep + actions.toolStep) > 0 ? 1 : -1), 9);
    if (actions.devRotate) facing_ = (facing_ + 1) % 4;
    if (actions.devVariation) ++variation_;
    if (actions.undo || actions.redo) undo(game, actions.redo && !actions.undo);
    refreshTarget(game);
    if (!actions.remove && !actions.place && !actions.pick) return;
    Edit edit;
    edit.beforeMeta = meta_; edit.beforeAssets = assets_;
    if (targetAsset_ && (actions.remove || actions.pick)) {
        const auto index = *targetAsset_;
        if (actions.pick) {
            const auto item = std::find_if(palette_[1].begin(), palette_[1].end(), [&](const SectionPaletteItem& candidate) { return candidate.asset == assets_[index].kind; });
            if (item != palette_[1].end()) hotbar_[selectedSlot_] = *item;
            facing_ = assets_[index].facing; variation_ = assets_[index].variation;
            message_ = "PICKED " + hotbar_[selectedSlot_].name;
        } else {
            assets_.erase(assets_.begin() + static_cast<std::ptrdiff_t>(index));
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
    const auto& item = hotbar_[selectedSlot_];
    if (actions.remove) {
        if (item.marker != SectionMarker::None) {
            if (item.marker == SectionMarker::Checkpoint && meta_.checkpoint) {
                meta_.checkpoint.reset(); changed(std::move(edit)); message_ = "CHECKPOINT REMOVED";
            } else message_ = "START AND END ARE REQUIRED | RIGHT CLICK A TOP FACE TO MOVE A MARKER";
            return;
        }
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
        if (!preview_) { message_ = "AIM AT A BLOCK FACE INSIDE THE ROOM"; return; }
        if (assets_.size() >= SectionMaxAssets) { message_ = "ASSET LIMIT REACHED"; return; }
        const auto duplicate = std::find_if(assets_.begin(), assets_.end(), [&](const HubAsset& asset) {
            return asset.kind == preview_->kind && asset.position == preview_->position && asset.facing == preview_->facing;
        });
        if (duplicate != assets_.end()) { message_ = "THIS ASSET IS ALREADY HERE"; return; }
        assets_.push_back(*preview_); changed(std::move(edit));
        message_ = "PLACED " + item.name + " | DECORATIVE: BUILD WALKABLE SURFACES WITH BLOCKS";
    } else if (item.marker != SectionMarker::None) {
        const auto cell = hit.position + glm::ivec3(0, 1, 0);
        const auto feet = glm::vec3(cell) + glm::vec3(.5f, 0, .5f);
        if (hit.normal.y != 1 || !sectionInterior(cell) || !sectionMarkerSafe(game.world, feet)) {
            message_ = "MARKERS NEED A CLEAR, SUPPORTED TOP FACE WITH HEADROOM"; return;
        }
        if (item.marker == SectionMarker::Start) meta_.spawn = feet;
        else if (item.marker == SectionMarker::End) meta_.finish = feet;
        else meta_.checkpoint = feet;
        changed(std::move(edit)); message_ = "MOVED " + item.name + " | ONE OF EACH PER SECTION";
    }
    refreshTarget(game);
}

void SectionEditor::faceMarker(Game& game, glm::vec3 position)
{
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
    if (!sectionMarkerSafe(game.world, meta_.spawn) || !sectionMarkerSafe(game.world, meta_.finish)
        || (meta_.checkpoint && !sectionMarkerSafe(game.world, *meta_.checkpoint))) {
        message_ = "FIX START / CHECKPOINT / END: EACH NEEDS SOLID SUPPORT AND CLEAR HEADROOM"; return;
    }
    if (glm::distance(meta_.spawn, meta_.finish) < 3) { message_ = "PLACE THE END AT LEAST THREE BLOCKS FROM THE START"; return; }
    if (dirty_ && !saveDraft(game)) return;
    buildPosition_ = game.camera.position(); buildDirection_ = game.camera.forward();
    checkpointReached_ = testComplete_ = false; testElapsed_ = 0; testFalls_ = 0;
    faceMarker(game, meta_.spawn);
    screen_ = SectionEditorScreen::Test; game.target.reset(); preview_.reset();
    message_ = "WALK TO THE GOLD END | R RESET | F8 RETURN TO BUILD";
}
void SectionEditor::stopTest(Game& game)
{
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
    game.player.update(game.world, actions.movement, game.camera.forward(), actions.jump, actions.sprint, deltaSeconds);
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
