#include "level_editor.h"

#include "game.h"
#include "input.h"
#include "asset_collision.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace voxel {
namespace {
int pageCount(std::size_t count, int size) { return std::max(1, (static_cast<int>(count) + size - 1) / size); }
float stepTime(float seconds) { return std::isfinite(seconds) && seconds > 0 ? std::min(seconds, .1f) : 0.0f; }
bool atMarker(const Game& game, glm::vec3 marker)
{
    const auto delta = game.player.feetPosition() - marker;
    return game.player.grounded() && std::abs(delta.y) < .75f
        && delta.x * delta.x + delta.z * delta.z < 1.2f * 1.2f;
}
}

LevelController::LevelController(std::filesystem::path sectionRoot, std::filesystem::path levelRoot)
    : sectionRoot_(std::move(sectionRoot)), levelRoot_(std::move(levelRoot)) {}

void LevelController::enter(Game& game, bool creator)
{
    creator_ = creator;
    screen_ = LevelScreen::Worlds;
    running_ = false;
    selection_ = 0;
    textField_ = LevelTextField::None;
    assets_.clear();
    game.target.reset();
    message_ = creator ? "CHOOSE THE WORLD FOR YOUR LEVEL" : "CHOOSE A WORLD TO EXPLORE";
}

bool LevelController::leave(Game&)
{
    if (dirty_ && !saveDraft()) return false;
    screen_ = LevelScreen::Closed;
    running_ = false;
    assets_.clear();
    textField_ = LevelTextField::None;
    return true;
}

void LevelController::refreshLibrary()
{
    entries_ = listLevels(levelRoot_, selectedWorld_);
    sections_ = listSections(sectionRoot_, selectedWorld_);
    draft_.reset();
    if (creator_) {
        try { draft_ = loadLevelDraft(levelRoot_, selectedWorld_); }
        catch (const std::exception& error) { message_ = std::string("DRAFT UNAVAILABLE: ") + error.what(); }
    }
    selection_ = 0;
    libraryPage_ = sectionPage_ = 0;
}

void LevelController::beginComposition(const LevelDefinition& definition)
{
    definition_ = definition;
    numberText_ = std::to_string(definition.number);
    screen_ = LevelScreen::Compose;
    selectedQueue_ = definition.sections.empty() ? -1 : 0;
    textField_ = LevelTextField::None;
    textSelected_ = false;
    dirty_ = false;
    sectionPage_ = selection_ = 0;
    message_ = "CLICK SECTIONS TO ADD THEM IN ORDER | DUPLICATES ARE ALLOWED";
}

bool LevelController::readNumber()
{
    int number = 0;
    const auto result = std::from_chars(numberText_.data(), numberText_.data() + numberText_.size(), number);
    if (result.ec != std::errc{} || result.ptr != numberText_.data() + numberText_.size()
        || number < 1 || number > LevelMaxNumber) {
        message_ = "LEVEL NUMBER MUST BE 1 TO " + std::to_string(LevelMaxNumber);
        return false;
    }
    definition_.number = number;
    return true;
}

bool LevelController::saveDraft()
{
    if (!readNumber()) return false;
    try {
        saveLevelDraft(definition_, sectionRoot_, levelRoot_);
        draft_ = definition_;
        dirty_ = false;
        return true;
    } catch (const std::exception& error) {
        message_ = std::string("DRAFT NOT SAVED: ") + error.what();
        return false;
    }
}

void LevelController::changed()
{
    dirty_ = true;
    if (saveDraft()) message_ = "DRAFT SAVED | PUBLISH ADDS THIS LEVEL TO THE GAME";
}

std::string LevelController::sectionName(const std::filesystem::path& reference) const
{
    for (const auto& entry : sections_)
        if (entry.path.lexically_relative(sectionRoot_) == reference) return entry.meta.name;
    return reference.stem().string();
}

void LevelController::publish(Game&, bool replace)
{
    textField_ = LevelTextField::None;
    if (!readNumber()) return;
    try {
        validateLevel(definition_, sectionRoot_);
        const auto worlds = sectionWorldCatalog();
        const auto info = std::find_if(worlds.begin(), worlds.end(), [&](const auto& world) {
            return world.id == definition_.world;
        });
        auto number = std::to_string(definition_.number);
        number.insert(0, 3 - number.size(), '0');
        const auto destination = levelRoot_ / info->folder / ("level-" + number + ".vlevel");
        // Include corrupt manifests, which the readable catalog deliberately
        // skips: their number still needs an explicit replacement decision.
        if (!replace && std::filesystem::exists(destination)) {
            screen_ = LevelScreen::Replace;
            message_ = "THIS WORLD ALREADY HAS LEVEL " + std::to_string(definition_.number)
                + ". REPLACE ITS SECTION ORDER?";
            return;
        }
        saveLevel(definition_, sectionRoot_, levelRoot_, replace);
        dirty_ = true;
        const bool draftSaved = saveDraft();
        refreshLibrary();
        screen_ = LevelScreen::Compose;
        if (draftSaved) message_ = "PUBLISHED LEVEL " + std::to_string(definition_.number)
            + " | PLAY IT NOW OR FIND IT WITH F4";
    } catch (const std::exception& error) {
        screen_ = LevelScreen::Compose;
        message_ = std::string("LEVEL NOT PUBLISHED: ") + error.what();
    }
}

void LevelController::position(Game& game, glm::vec3 feet)
{
    if (!game.player.setFeetPosition(game.world, feet) || !game.player.grounded())
        throw std::runtime_error("The section marker no longer has a safe supported landing");
    const auto direction = meta_.finish - feet;
    const float yaw = std::hypot(direction.x, direction.z) > .001f
        ? glm::degrees(std::atan2(direction.z, direction.x)) : -90.0f;
    game.camera.reset();
    const auto facing = game.camera.forward();
    game.camera.rotate(yaw - glm::degrees(std::atan2(facing.z, facing.x)),
                       -6.0f - glm::degrees(std::asin(facing.y)));
    game.camera.setPosition(game.player.eyePosition());
}

bool LevelController::loadSectionAt(Game& game, std::size_t index)
{
    try {
        // Resolve and check the new player against a temporary document before
        // replacing the displayed world. Missing files never destroy the old room.
        auto document = resolveLevelSection(definition_, index, sectionRoot_);
        auto collision = std::make_shared<AssetCollision>();
        collision->rebuild(document.assets);
        Player candidate;
        candidate.setAssetCollision(std::move(collision));
        if (!candidate.setFeetPosition(document.world, document.meta.spawn) || !candidate.grounded())
            throw std::runtime_error("Section start does not have a safe supported landing");
        game.world = std::move(document.world);
        game.player = candidate;
        meta_ = std::move(document.meta);
        assets_ = std::move(document.assets);
        sectionIndex_ = index;
        checkpointReached_ = false;
        ++revision_;
        game.target.reset();
        position(game, meta_.spawn);
        screen_ = LevelScreen::Play;
        game.beginSectionLoot(assets_, false, static_cast<std::uint32_t>(definition_.number * 131 + index));
        message_ = "REACH END TO CONTINUE | R CHECKPOINT | SHIFT+R RESTART LEVEL | B HUB";
        return true;
    } catch (const std::exception& error) {
        message_ = std::string("SECTION UNAVAILABLE: ") + error.what();
        return false;
    }
}

bool LevelController::start(Game& game, const LevelDefinition& definition)
{
    try { validateLevel(definition, sectionRoot_); }
    catch (const std::exception& error) {
        message_ = std::string("CANNOT START LEVEL: ") + error.what();
        return false;
    }
    const auto previous = definition_;
    definition_ = definition;
    if (!loadSectionAt(game, 0)) { definition_ = previous; return false; }
    running_ = true;
    creator_ = false;
    dirty_ = false;
    elapsed_ = 0;
    falls_ = 0;
    textField_ = LevelTextField::None;
    return true;
}

void LevelController::reset(Game& game, bool full)
{
    if (full) {
        pendingSection_ = 0;
        if (loadSectionAt(game, 0)) { elapsed_ = 0; falls_ = 0; }
        else screen_ = LevelScreen::Error;
    } else {
        try { position(game, checkpointReached_ && meta_.checkpoint ? *meta_.checkpoint : meta_.spawn); }
        catch (const std::exception& error) { message_ = error.what(); }
    }
}

void LevelController::activate(Game& game, int id)
{
    if (id == LevelUiPlay && screen_ == LevelScreen::Worlds) id = LevelUiWorldBase + selection_;
    else if (id == LevelUiPlay && screen_ == LevelScreen::Library)
        id = entries_.empty() && creator_ ? LevelUiNew : LevelUiEntryBase + selection_;
    if (id == LevelUiHub) { game.leaveLevelMode(); return; }
    if (id == LevelUiBack || id == LevelUiCancel) {
        if (screen_ == LevelScreen::Replace) { screen_ = LevelScreen::Compose; message_ = "PUBLISH CANCELLED | YOUR DRAFT IS UNCHANGED"; }
        else if (screen_ == LevelScreen::Compose) {
            if (dirty_ && !saveDraft()) return;
            screen_ = LevelScreen::Library;
            textField_ = LevelTextField::None;
            refreshLibrary();
        } else if (screen_ == LevelScreen::Library) { screen_ = LevelScreen::Worlds; selection_ = 0; }
        else game.leaveLevelMode();
        return;
    }
    if (screen_ == LevelScreen::Worlds && id >= LevelUiWorldBase
        && id < LevelUiWorldBase + static_cast<int>(sectionWorldCatalog().size())) {
        selectedWorld_ = sectionWorldCatalog()[id - LevelUiWorldBase].id;
        message_ = creator_ ? "CREATE A LEVEL OR EDIT AN EXISTING ONE" : "SELECT A LEVEL TO BEGIN";
        refreshLibrary();
        screen_ = LevelScreen::Library;
    } else if (screen_ == LevelScreen::Library) {
        if (creator_ && id == LevelUiNew) {
            LevelDefinition fresh; fresh.world = selectedWorld_;
            while (fresh.number < LevelMaxNumber && std::any_of(entries_.begin(), entries_.end(), [&](const LevelEntry& entry) {
                return entry.definition.number == fresh.number;
            })) ++fresh.number;
            fresh.name = "Level " + std::to_string(fresh.number);
            beginComposition(fresh);
            changed();
        } else if (creator_ && id == LevelUiResume && draft_) beginComposition(*draft_);
        else if (id >= LevelUiEntryBase && id < LevelUiEntryBase + static_cast<int>(entries_.size())) {
            const auto& selected = entries_[id - LevelUiEntryBase];
            if (creator_) beginComposition(selected.definition);
            else start(game, selected.definition);
        } else if (id == LevelUiLibraryPrevious || id == LevelUiLibraryNext) {
            libraryPage_ = std::clamp(libraryPage_ + (id == LevelUiLibraryNext ? 1 : -1), 0,
                                     pageCount(entries_.size(), LevelLibraryPageSize) - 1);
            selection_ = libraryPage_ * LevelLibraryPageSize;
        }
    } else if (screen_ == LevelScreen::Compose) {
        if (id == LevelUiName || id == LevelUiNumber) {
            textField_ = id == LevelUiName ? LevelTextField::Name : LevelTextField::Number;
            textSelected_ = true;
        } else if (id == LevelUiPublish) publish(game, false);
        else if (id == LevelUiPlay) {
            if (saveDraft()) start(game, definition_);
        } else if (id >= LevelUiSectionBase && id < LevelUiSectionBase + static_cast<int>(sections_.size())) {
            textField_ = LevelTextField::None;
            if (definition_.sections.size() >= LevelMaxSections) { message_ = "A LEVEL CAN CONTAIN UP TO 64 SECTIONS"; return; }
            definition_.sections.push_back(sections_[id - LevelUiSectionBase].path.lexically_relative(sectionRoot_));
            selectedQueue_ = static_cast<int>(definition_.sections.size()) - 1;
            changed();
        } else if (id >= LevelUiQueueBase && id < LevelUiQueueBase + static_cast<int>(definition_.sections.size())) {
            selectedQueue_ = id - LevelUiQueueBase;
            textField_ = LevelTextField::None;
        } else if (id == LevelUiRemove || id == LevelUiMoveUp || id == LevelUiMoveDown) {
            textField_ = LevelTextField::None;
            if (selectedQueue_ < 0 || selectedQueue_ >= static_cast<int>(definition_.sections.size())) return;
            if (id == LevelUiRemove) {
                definition_.sections.erase(definition_.sections.begin() + selectedQueue_);
                selectedQueue_ = std::min(selectedQueue_, static_cast<int>(definition_.sections.size()) - 1);
            } else {
                const int next = selectedQueue_ + (id == LevelUiMoveUp ? -1 : 1);
                if (next < 0 || next >= static_cast<int>(definition_.sections.size())) return;
                std::swap(definition_.sections[next], definition_.sections[selectedQueue_]);
                selectedQueue_ = next;
            }
            changed();
        } else if (id == LevelUiSectionPrevious || id == LevelUiSectionNext) {
            sectionPage_ = std::clamp(sectionPage_ + (id == LevelUiSectionNext ? 1 : -1), 0,
                                     pageCount(sections_.size(), LevelSectionPageSize) - 1);
            selection_ = sectionPage_ * LevelSectionPageSize;
        } else if (id == LevelUiQueuePrevious || id == LevelUiQueueNext) {
            const int page = std::clamp(queuePage() + (id == LevelUiQueueNext ? 1 : -1), 0,
                                       pageCount(definition_.sections.size(), LevelQueuePageSize) - 1);
            selectedQueue_ = definition_.sections.empty() ? -1 : page * LevelQueuePageSize;
        }
    } else if (screen_ == LevelScreen::Replace && id == LevelUiReplace) publish(game, true);
    else if (screen_ == LevelScreen::Complete && id == LevelUiReplay) reset(game, true);
    else if (screen_ == LevelScreen::Error && id == LevelUiRetry) {
        if (!loadSectionAt(game, pendingSection_)) screen_ = LevelScreen::Error;
    }
}

void LevelController::updateMenu(Game& game, const InputActions& actions)
{
    if (actions.closeMenu) { activate(game, LevelUiBack); return; }
    if (!actions.active) return;
    if (actions.returnToHub) { game.leaveLevelMode(); return; }
    if (actions.devClick) { activate(game, *actions.devClick); return; }
    if (screen_ == LevelScreen::Transition) return;
    if (screen_ == LevelScreen::Compose && actions.devTabStep) {
        textField_ = textField_ == LevelTextField::None ? LevelTextField::Name
            : textField_ == LevelTextField::Name ? LevelTextField::Number : LevelTextField::None;
        textSelected_ = textField_ != LevelTextField::None;
        return;
    }
    if (screen_ == LevelScreen::Compose && textField_ != LevelTextField::None) {
        auto& field = textField_ == LevelTextField::Name ? definition_.name : numberText_;
        bool edited = false;
        if (actions.devSelectAll) textSelected_ = true;
        if (actions.devBackspace) {
            if (textSelected_) field.clear();
            else if (!field.empty()) field.pop_back();
            textSelected_ = false;
            edited = true;
        }
        for (const unsigned char c : actions.devText) {
            if (c < 32 || c > 126 || (textField_ == LevelTextField::Number && (c < '0' || c > '9'))) continue;
            if (textSelected_) { field.clear(); textSelected_ = false; }
            const std::size_t maximum = textField_ == LevelTextField::Name ? 64 : 3;
            if (field.size() < maximum) { field += static_cast<char>(c); edited = true; }
        }
        if (edited) changed();
        if (actions.upgrade) { textField_ = LevelTextField::None; if (dirty_) saveDraft(); }
        return;
    }
    if (screen_ == LevelScreen::Worlds) {
        const int count = static_cast<int>(sectionWorldCatalog().size());
        if (actions.devMenuStep.y) selection_ = (selection_ + (actions.devMenuStep.y > 0 ? 1 : count - 1)) % count;
        if (actions.upgrade) activate(game, LevelUiWorldBase + selection_);
    } else if (screen_ == LevelScreen::Library) {
        if (actions.devMenuStep.y && !entries_.empty()) {
            const int count = static_cast<int>(entries_.size());
            selection_ = (selection_ + (actions.devMenuStep.y > 0 ? 1 : count - 1)) % count;
            libraryPage_ = selection_ / LevelLibraryPageSize;
        }
        if (actions.upgrade) activate(game, entries_.empty() && creator_ ? LevelUiNew : LevelUiEntryBase + selection_);
    } else if (screen_ == LevelScreen::Compose) {
        if (actions.devMenuStep.y && !sections_.empty()) {
            const int count = static_cast<int>(sections_.size());
            selection_ = (selection_ + (actions.devMenuStep.y > 0 ? 1 : count - 1)) % count;
            sectionPage_ = selection_ / LevelSectionPageSize;
        }
        if (actions.upgrade && !sections_.empty()) activate(game, LevelUiSectionBase + selection_);
        else if (actions.save) publish(game, false);
    }
    else if (screen_ == LevelScreen::Complete && (actions.upgrade || actions.restartCrossing)) reset(game, true);
    else if (screen_ == LevelScreen::Error && actions.upgrade) activate(game, LevelUiRetry);
}

void LevelController::updatePlay(Game& game, const InputActions& actions, float deltaSeconds)
{
    if (!actions.active) return;
    if (actions.returnToHub) { game.leaveLevelMode(); return; }
    if (actions.restartCrossing || actions.respawn) { reset(game, actions.restartCrossing); return; }
    if (actions.selectedTool && *actions.selectedTool >= 0 && *actions.selectedTool < ToolCount)
        game.equipment.selectedSlot = *actions.selectedTool;
    else if (actions.toolStep) game.equipment.selectedSlot = ((game.equipment.selectedSlot
        + actions.toolStep % ToolCount) % ToolCount + ToolCount) % ToolCount;
    game.player.update(game.world, actions.movement, game.camera.forward(), actions.jump, actions.sprint, deltaSeconds, actions.descend);
    game.camera.setPosition(game.player.eyePosition());
    elapsed_ += stepTime(deltaSeconds);
    if (game.player.feetPosition().y < SectionInteriorMin.y - 2.0f) { ++falls_; reset(game, false); return; }
    if (meta_.checkpoint && atMarker(game, *meta_.checkpoint)) checkpointReached_ = true;
    if (!atMarker(game, meta_.finish)) return;
    if (sectionIndex_ + 1 == definition_.sections.size()) {
        screen_ = LevelScreen::Complete;
        message_ = "LEVEL COMPLETE | RETURN TO THE VILLAGE OR REPLAY";
    } else {
        pendingSection_ = sectionIndex_ + 1;
        transitionElapsed_ = 0;
        screen_ = LevelScreen::Transition;
        message_ = "CONTINUING TO SECTION " + std::to_string(pendingSection_ + 1);
    }
}

void LevelController::update(Game& game, const InputActions& actions, float deltaSeconds)
{
    try {
        if (screen_ == LevelScreen::Play) updatePlay(game, actions, deltaSeconds);
        else if (screen_ == LevelScreen::Transition) {
            if (actions.returnToHub || actions.closeMenu) { game.leaveLevelMode(); return; }
            if (!actions.active) return;
            transitionElapsed_ += stepTime(deltaSeconds);
            if (transitionElapsed_ >= .3f && !loadSectionAt(game, pendingSection_)) screen_ = LevelScreen::Error;
        } else updateMenu(game, actions);
    } catch (const std::exception& error) {
        message_ = std::string("LEVEL ACTION FAILED: ") + error.what();
    }
    game.status = message_;
}
} // namespace voxel
