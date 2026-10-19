#pragma once

#include "editing.h"
#include "sections.h"
#include "section_tools.h"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace voxel {
class Game;
struct InputActions;

enum class SectionEditorScreen { Worlds, Library, Build, Palette, Tools, Test, Accept };
enum class SectionBuildMode { Single, Select, Brush, Paste };
enum class SectionMarker { None, Start, Checkpoint, End };
struct SectionPaletteItem {
    std::string name;
    std::optional<Block> block;
    std::optional<HubAssetKind> asset;
    SectionMarker marker = SectionMarker::None;
};

// The HUD and controller share these IDs, so mouse and keyboard use the same actions.
inline constexpr int SectionUiWorldBase = 100;
inline constexpr int SectionUiLibraryNew = 200;
inline constexpr int SectionUiLibraryDraftBase = 300;
inline constexpr int SectionUiLibraryEntryBase = 1000;
inline constexpr int SectionUiPaletteTabBase = 2000;
inline constexpr int SectionUiPaletteItemBase = 2100;
inline constexpr int SectionUiHotbarBase = 3000;
inline constexpr int SectionUiCategoryBase = 3100;
inline constexpr int SectionUiConfirm = 3200, SectionUiBack = 3201;
inline constexpr int SectionUiTest = 3202, SectionUiAccept = 3203;
inline constexpr int SectionUiSave = 3204, SectionUiLibrary = 3205;
inline constexpr int SectionUiPalettePrevious = 3206, SectionUiPaletteNext = 3207;
inline constexpr int SectionUiLibraryPrevious = 3208, SectionUiLibraryNext = 3209;
inline constexpr int SectionUiLevelCreator = 3210;
inline constexpr int SectionUiModeBase = 3310;
inline constexpr int SectionUiBrushSizeBase = 3320; // Sizes 1, 3, 5, 9.
inline constexpr int SectionUiFill = 3330, SectionUiShell = 3331, SectionUiErase = 3332;
inline constexpr int SectionUiCopy = 3333, SectionUiPaste = 3334, SectionUiPasteAir = 3335;
inline constexpr int SectionUiClearSelection = 3336, SectionUiHeightDown = 3337, SectionUiHeightUp = 3338;

// One editable section lives in Game::world. This owns the authoring metadata,
// palette, undo records and test state; it never touches player equipment.
class SectionEditor {
public:
    explicit SectionEditor(std::filesystem::path libraryRoot = "sections");
    void enter(Game& game);
    bool leave(Game& game); // Saves a dirty draft first. Failure leaves the editor open.
    void update(Game& game, const InputActions& actions, float deltaSeconds);
    bool saveDraft(Game& game);

    SectionEditorScreen screen() const { return screen_; }
    bool menuOpen() const;
    bool flying() const { return screen_ == SectionEditorScreen::Build; }
    bool hasSection() const { return hasSection_; }
    SectionWorld selectedWorld() const { return selectedWorld_; }
    int selection() const { return selection_; }
    int paletteTab() const { return paletteTab_; }
    int paletteSelection() const { return paletteSelection_; }
    const std::vector<SectionPaletteItem>& paletteItems() const { return palette_[paletteTab_]; }
    const std::array<SectionPaletteItem, 9>& hotbar() const { return hotbar_; }
    int selectedSlot() const { return selectedSlot_; }
    int facing() const { return facing_; }
    std::uint32_t variation() const { return variation_; }
    const SectionMetadata& meta() const { return meta_; }
    const std::vector<HubAsset>& assets() const { return assets_; }
    const std::vector<SectionEntry>& entries() const { return entries_; }
    const std::vector<SectionEntry>& draftEntries() const { return drafts_; }
    const std::string& message() const { return message_; }
    bool dirty() const { return dirty_; }
    bool nameSelected() const { return nameSelected_; }
    bool testComplete() const { return testComplete_; }
    bool checkpointReached() const { return checkpointReached_; }
    float testElapsed() const { return testElapsed_; }
    int testFalls() const { return testFalls_; }
    std::uint64_t revision() const { return revision_; }
    std::optional<HubAsset> targetedAsset() const { return targetAsset_ && *targetAsset_ < assets_.size() ? std::optional<HubAsset>(assets_[*targetAsset_]) : std::nullopt; }
    const std::optional<HubAsset>& placementPreview() const { return preview_; }
    const std::filesystem::path& libraryRoot() const { return libraryRoot_; }
    const std::filesystem::path& acceptedPath() const { return acceptedPath_; }
    SectionBuildMode buildMode() const { return buildMode_; }
    int brushSize() const { return brushSize_; }
    bool brushErasing() const { return brushErasing_; }
    std::optional<SectionBox> buildSelection() const;
    const std::optional<glm::ivec3>& firstCorner() const { return cornerA_; }
    const std::optional<glm::ivec3>& secondCorner() const { return cornerB_; }
    const SectionClipboard& clipboard() const { return clipboard_; }
    std::uint64_t clipboardRevision() const { return clipboardRevision_; }
    int pasteRotation() const { return pasteRotation_; }
    bool pasteIncludesAir() const { return pasteIncludesAir_; }
    const std::optional<SectionBox>& toolPreview() const { return toolPreview_; }
    bool toolPreviewValid() const { return toolPreviewValid_; }

private:
    struct Edit {
        std::optional<glm::ivec3> position;
        Block before = Block::Air, after = Block::Air;
        std::vector<SectionBlockEdit> blocks;
        SectionMetadata beforeMeta, afterMeta;
        std::vector<HubAsset> beforeAssets, afterAssets;
    };
    void refreshLibrary();
    void newSection(Game& game);
    void openSection(Game& game, const SectionEntry& entry, bool draft);
    void selectLibrary(Game& game);
    void updateMenu(Game& game, const InputActions& actions);
    void updateBuild(Game& game, const InputActions& actions);
    void updateTest(Game& game, const InputActions& actions, float deltaSeconds);
    void startTest(Game& game);
    void stopTest(Game& game);
    void resetTest(Game& game, bool restart);
    void openAccept(Game& game);
    void accept(Game& game);
    void refreshTarget(Game& game);
    void changed(Edit edit);
    void undo(Game& game, bool redo);
    void invalidateTest();
    void faceMarker(Game& game, glm::vec3 position);
    void resetBuildTools();
    void updateToolsMenu(Game& game, const InputActions& actions);
    bool updateBuildTools(Game& game, const InputActions& actions);
    void refreshToolPreview(Game& game);
    void copySelection(Game& game);
    void beginPaste();
    void fillSelection(Game& game, SectionFillMode mode);
    void applyPlan(Game& game, SectionEditPlan plan, std::string_view label);

    std::filesystem::path libraryRoot_, draftPath_, acceptedPath_;
    SectionEditorScreen screen_ = SectionEditorScreen::Worlds;
    SectionWorld selectedWorld_ = SectionWorld::LushCaves;
    SectionMetadata meta_;
    std::vector<HubAsset> assets_;
    std::vector<SectionEntry> entries_, drafts_;
    std::array<std::vector<SectionPaletteItem>, 3> palette_;
    std::array<SectionPaletteItem, 9> hotbar_;
    int selection_ = 0, paletteTab_ = 0, paletteSelection_ = 0, selectedSlot_ = 0, facing_ = 0;
    std::uint32_t variation_ = 0;
    std::uint64_t revision_ = 0;
    bool nameSelected_ = false;
    bool hasSection_ = false, dirty_ = false, testComplete_ = false, checkpointReached_ = false;
    float testElapsed_ = 0.0f, autosaveElapsed_ = 0.0f;
    int testFalls_ = 0;
    glm::vec3 buildPosition_{0};
    glm::vec3 buildDirection_{0, 0, -1};
    struct AssetBounds { glm::vec3 minimum, maximum; };
    std::vector<AssetBounds> assetBounds_;
    std::uint64_t boundsRevision_ = ~std::uint64_t(0);
    std::optional<std::size_t> targetAsset_;
    std::optional<HubAsset> preview_;
    std::deque<Edit> undo_, redo_;
    std::string message_;
    SectionBuildMode buildMode_ = SectionBuildMode::Single;
    int brushSize_ = 3, pasteRotation_ = 0;
    bool pasteIncludesAir_ = false, toolPreviewValid_ = false, brushErasing_ = false;
    std::optional<glm::ivec3> cornerA_, cornerB_;
    std::optional<SectionBox> toolPreview_;
    SectionClipboard clipboard_;
    std::uint64_t clipboardRevision_ = 0;
    SectionEditPlan previewPlan_;
    std::optional<glm::ivec3> previewOrigin_;
    glm::ivec3 previewCameraCell_{0};
    std::uint64_t previewRevision_ = ~std::uint64_t(0);
    int previewRotation_ = -1;
    bool previewAir_ = false;
};
} // namespace voxel
