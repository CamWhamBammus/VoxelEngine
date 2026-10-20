#pragma once

#include "levels.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace voxel {
class Game;
struct InputActions;

enum class LevelScreen { Closed, Worlds, Library, Compose, Replace, Play, Transition, Complete, Error };
enum class LevelTextField { None, Name, Number };

inline constexpr int LevelUiWorldBase = 4000;
inline constexpr int LevelUiEntryBase = 4100;
inline constexpr int LevelUiSectionBase = 100000;
inline constexpr int LevelUiQueueBase = 200000;
inline constexpr int LevelUiNew = 300000, LevelUiResume = 300001, LevelUiBack = 300002;
inline constexpr int LevelUiName = 300003, LevelUiNumber = 300004, LevelUiPublish = 300005;
inline constexpr int LevelUiMoveUp = 300006, LevelUiMoveDown = 300007, LevelUiRemove = 300008;
inline constexpr int LevelUiReplace = 300009, LevelUiCancel = 300010, LevelUiPlay = 300011;
inline constexpr int LevelUiReplay = 300012, LevelUiHub = 300013, LevelUiRetry = 300014;
inline constexpr int LevelUiLibraryPrevious = 300015, LevelUiLibraryNext = 300016;
inline constexpr int LevelUiSectionPrevious = 300017, LevelUiSectionNext = 300018;
inline constexpr int LevelUiQueuePrevious = 300019, LevelUiQueueNext = 300020;
inline constexpr int LevelLibraryPageSize = 7, LevelSectionPageSize = 7, LevelQueuePageSize = 7;

// Level assembly stores references to accepted sections. Playing loads one section
// at a time into Game::world. Loot goes into the player's saved inventory;
// creative section documents are never modified by playing a level.
class LevelController {
public:
    explicit LevelController(std::filesystem::path sectionRoot = "sections", std::filesystem::path levelRoot = "levels");
    void enter(Game& game, bool creator);
    bool leave(Game& game);
    bool saveDraft();
    void update(Game& game, const InputActions& actions, float deltaSeconds);
    bool start(Game& game, const LevelDefinition& definition);

    LevelScreen screen() const { return screen_; }
    bool open() const { return screen_ != LevelScreen::Closed; }
    bool creator() const { return creator_; }
    bool active() const { return running_; }
    bool menuOpen() const { return open() && screen_ != LevelScreen::Play; }
    bool typing() const { return screen_ == LevelScreen::Compose && textField_ != LevelTextField::None; }
    SectionWorld selectedWorld() const { return selectedWorld_; }
    int selection() const { return selection_; }
    int selectedQueue() const { return selectedQueue_; }
    int libraryPage() const { return libraryPage_; }
    int sectionPage() const { return sectionPage_; }
    int queuePage() const { return selectedQueue_ < 0 ? 0 : selectedQueue_ / LevelQueuePageSize; }
    LevelTextField textField() const { return textField_; }
    bool textSelected() const { return textSelected_; }
    const std::string& numberText() const { return numberText_; }
    const std::vector<LevelEntry>& entries() const { return entries_; }
    const std::vector<SectionEntry>& sections() const { return sections_; }
    const LevelDefinition& definition() const { return definition_; }
    const std::optional<LevelDefinition>& draft() const { return draft_; }
    const std::string& message() const { return message_; }
    bool dirty() const { return dirty_; }
    const SectionMetadata& meta() const { return meta_; }
    const std::vector<HubAsset>& assets() const { return assets_; }
    std::uint64_t revision() const { return revision_; }
    std::size_t sectionIndex() const { return sectionIndex_; }
    bool checkpointReached() const { return checkpointReached_; }
    float elapsed() const { return elapsed_; }
    int falls() const { return falls_; }
    const std::filesystem::path& libraryRoot() const { return levelRoot_; }
    std::string sectionName(const std::filesystem::path& reference) const;

private:
    void refreshLibrary();
    void beginComposition(const LevelDefinition& definition);
    bool readNumber();
    void changed();
    void publish(Game& game, bool replace);
    bool loadSectionAt(Game& game, std::size_t index);
    void position(Game& game, glm::vec3 feet);
    void reset(Game& game, bool full);
    void updateMenu(Game& game, const InputActions& actions);
    void updatePlay(Game& game, const InputActions& actions, float deltaSeconds);
    void activate(Game& game, int id);

    std::filesystem::path sectionRoot_, levelRoot_;
    LevelScreen screen_ = LevelScreen::Closed;
    SectionWorld selectedWorld_ = SectionWorld::LushCaves;
    bool creator_ = false, running_ = false, dirty_ = false, textSelected_ = false;
    LevelTextField textField_ = LevelTextField::None;
    int selection_ = 0, selectedQueue_ = -1, libraryPage_ = 0, sectionPage_ = 0;
    LevelDefinition definition_;
    std::optional<LevelDefinition> draft_;
    std::vector<LevelEntry> entries_;
    std::vector<SectionEntry> sections_;
    std::string numberText_ = "1", message_;
    SectionMetadata meta_;
    std::vector<HubAsset> assets_;
    std::uint64_t revision_ = 0;
    std::size_t sectionIndex_ = 0, pendingSection_ = 0;
    bool checkpointReached_ = false;
    float elapsed_ = 0.0f, transitionElapsed_ = 0.0f;
    int falls_ = 0;
};
} // namespace voxel
