#pragma once

#include "editing.h"
#include "sections.h"
#include "section_presets.h"
#include "section_tools.h"
#include "asset_collision.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace voxel {
class Game;
struct InputActions;

enum class SectionEditorScreen { Worlds, Library, Build, Palette, Tools, PasteSettings, Test, Accept, ChestContents, SpawnerSettings, Rename, Create };
enum class SectionBuildMode { Single, Select, Brush, Paste, CarveRound, CarveBox };
enum class SectionMarker { None, Start, Checkpoint, End };
struct SectionPaletteItem {
    std::string name;
    std::optional<Block> block;
    std::optional<HubAssetKind> asset;
    SectionMarker marker = SectionMarker::None;
    std::optional<ItemId> pickup = std::nullopt;
    std::optional<SectionBuildMode> tool = std::nullopt;
};

// The HUD and controller share these IDs, so mouse and keyboard use the same actions.
inline constexpr int SectionUiWorldBase = 100;
inline constexpr int SectionUiLibraryNew = 200, SectionUiLibraryCave = 201;
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
inline constexpr int SectionUiPasteSettings = 3340, SectionUiPasteModeBase = 3350;
inline constexpr int SectionUiPasteResume = 3352;
inline constexpr int SectionUiCarveRound = 3380, SectionUiCarveBox = 3381;
inline constexpr int SectionUiLighting = 3382, SectionUiBuilderLight = 3383;
inline constexpr int SectionUiRename = 3384, SectionUiNameField = 3385, SectionUiRenameApply = 3386;
inline constexpr int SectionUiChestSlotBase = 3400;
inline constexpr int SectionUiChestItemBase = 3500;
inline constexpr int SectionUiChestMinus = 3600, SectionUiChestPlus = 3601;
inline constexpr int SectionUiChestRemove = 3602, SectionUiChestClear = 3603;
inline constexpr int SectionUiChestApply = 3604, SectionUiChestCancel = 3605, SectionUiChestQuantity = 3606;
inline constexpr int SectionUiPickupMinus = 3610, SectionUiPickupPlus = 3611;
inline constexpr int SectionUiSpawnerMinusBase = 3700, SectionUiSpawnerPlusBase = 3710;
inline constexpr int SectionUiSpawnerRewards = 3720, SectionUiSpawnerApply = 3721, SectionUiSpawnerCancel = 3722;
inline constexpr int SectionUiSpawnerLink = 3723, SectionUiSpawnerUnlink = 3724;
inline constexpr int SectionUiPresetBase = 3800, SectionUiLayoutBase = 3810, SectionUiCreate = 3820;

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
    bool typing() const { return screen_ == SectionEditorScreen::Accept || screen_ == SectionEditorScreen::ChestContents
        || screen_ == SectionEditorScreen::Rename; }
    bool hasSection() const { return hasSection_; }
    SectionWorld selectedWorld() const { return selectedWorld_; }
    bool creationCave() const { return creationCave_; }
    SectionPreset creationPreset() const { return creationPreset_; }
    int selection() const { return selection_; }
    int paletteTab() const { return paletteTab_; }
    int paletteSelection() const { return paletteSelection_; }
    const std::vector<SectionPaletteItem>& paletteItems() const { return palette_[paletteTab_]; }
    const std::array<SectionPaletteItem, 9>& hotbar() const { return hotbar_; }
    int selectedSlot() const { return selectedSlot_; }
    int facing() const { return facing_; }
    std::uint32_t variation() const { return variation_; }
    int pickupQuantity() const { return pickupQuantity_; }
    const SectionMetadata& meta() const { return meta_; }
    const std::vector<HubAsset>& assets() const { return assets_; }
    std::shared_ptr<const AssetCollision> assetCollision() const;
    const std::vector<SectionEntry>& entries() const { return entries_; }
    const std::vector<SectionEntry>& draftEntries() const { return drafts_; }
    const std::string& message() const { return message_; }
    bool messageVisible() const { return !message_.empty() && (message_ != shownMessage_ || messageSeconds_ > 0); }
    bool dirty() const { return dirty_; }
    bool nameSelected() const { return nameSelected_; }
    const std::string& renameText() const { return renameText_; }
    bool testComplete() const { return testComplete_; }
    bool checkpointReached() const { return checkpointReached_; }
    void respawnAfterDeath(Game& game);
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
    bool builderLight() const { return builderLight_; }
    std::optional<SectionBox> buildSelection() const;
    const std::optional<glm::ivec3>& firstCorner() const { return cornerA_; }
    const std::optional<glm::ivec3>& secondCorner() const { return cornerB_; }
    const SectionClipboard& clipboard() const { return clipboard_; }
    std::uint64_t clipboardRevision() const { return clipboardRevision_; }
    int pasteRotation() const { return pasteRotation_; }
    bool pasteIncludesAir() const { return pasteIncludesAir_; }
    bool pasteFillGaps() const { return pasteFillGaps_; }
    bool pastePinned() const { return pinnedPasteOrigin_.has_value(); }
    const std::optional<int>& pasteFloorHeight() const { return pasteFloorHeight_; }
    const std::vector<SectionBlockEdit>& previewBlockChanges() const { return previewPlan_.blocks; }
    std::uint64_t pastePreviewRevision() const { return pastePreviewRevision_; }
    const std::optional<SectionBox>& toolPreview() const { return toolPreview_; }
    bool toolPreviewValid() const { return toolPreviewValid_; }
    const std::array<ItemStack, ChestSlotCount>& chestSlots() const { return chestSlots_; }
    int chestSelection() const { return chestSelection_; }
    const std::string& chestQuantityText() const { return chestQuantityText_; }
    bool chestQuantitySelected() const { return chestQuantitySelected_; }
    HubAssetKind editingChestKind() const;
    const std::array<int, 3>& spawnerCounts() const { return spawnerCounts_; }
    const std::vector<ItemStack>& spawnerRewards() const { return spawnerRewards_; }
    std::optional<std::size_t> editingSpawnerIndex() const { return editingSpawner_; }
    std::optional<std::size_t> linkingSpawner() const { return pickingSpawnerLink_ ? editingSpawner_ : std::nullopt; }
    std::optional<std::size_t> spawnerNext() const { return spawnerNext_; }

private:
    struct Edit {
        bool nameOnly = false;
        std::optional<glm::ivec3> position;
        Block before = Block::Air, after = Block::Air;
        std::vector<SectionBlockEdit> blocks;
        SectionMetadata beforeMeta, afterMeta;
        std::vector<HubAsset> beforeAssets, afterAssets;
    };
    void refreshLibrary();
    void openCreate(bool cave);
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
    void openRename(Game& game);
    void updateRename(Game& game, const InputActions& actions);
    void refreshTarget(Game& game);
    void changed(Edit edit);
    void undo(Game& game, bool redo);
    void invalidateTest();
    void faceMarker(Game& game, glm::vec3 position);
    void resetBuildTools();
    void selectBuildTool(Game& game, SectionBuildMode mode);
    void updateToolsMenu(Game& game, const InputActions& actions);
    void updatePasteSettings(Game& game, const InputActions& actions);
    void positionPaste(Game& game, const InputActions& actions);
    bool updateBuildTools(Game& game, const InputActions& actions);
    void refreshToolPreview(Game& game);
    void copySelection(Game& game);
    void beginPaste();
    void fillSelection(Game& game, SectionFillMode mode);
    void applyPlan(Game& game, SectionEditPlan plan, std::string_view label);
    void validateAssetEdit(const World& world, SectionEditPlan& plan) const;
    void openChestContents(Game& game, std::size_t index);
    void updateChestContents(Game& game, const InputActions& actions);
    void closeChestContents();
    void selectChestSlot(int slot);
    void openSpawnerSettings(Game& game, std::size_t index);
    void updateSpawnerSettings(Game& game, const InputActions& actions);
    void closeSpawnerSettings();
    void updateSpawnerLink(Game& game, const InputActions& actions);

    std::filesystem::path libraryRoot_, draftPath_, acceptedPath_;
    SectionEditorScreen screen_ = SectionEditorScreen::Worlds;
    SectionWorld selectedWorld_ = SectionWorld::LushCaves;
    bool creationCave_ = false;
    SectionPreset creationPreset_ = SectionPreset::WorldTheme;
    SectionMetadata meta_;
    std::vector<HubAsset> assets_;
    std::vector<SectionEntry> entries_, drafts_;
    std::array<std::vector<SectionPaletteItem>, 5> palette_;
    std::array<SectionPaletteItem, 9> hotbar_;
    int selection_ = 0, paletteTab_ = 0, paletteSelection_ = 0, selectedSlot_ = 0, facing_ = 0;
    std::uint32_t variation_ = 0;
    int pickupQuantity_ = 1;
    std::uint64_t revision_ = 0;
    std::uint64_t assetRevision_ = 0;
    mutable std::uint64_t collisionRevision_ = ~std::uint64_t(0);
    mutable std::shared_ptr<AssetCollision> assetCollision_;
    bool nameSelected_ = false;
    std::string renameText_;
    SectionEditorScreen renameReturn_ = SectionEditorScreen::Build;
    bool hasSection_ = false, dirty_ = false, testComplete_ = false, checkpointReached_ = false;
    float testElapsed_ = 0.0f, autosaveElapsed_ = 0.0f;
    int testFalls_ = 0;
    // Mining belongs to the temporary walk test, never the authored section.
    std::optional<World> testWorld_;
    glm::vec3 buildPosition_{0};
    glm::vec3 buildDirection_{0, 0, -1};
    struct AssetBounds { glm::vec3 minimum, maximum; };
    std::vector<AssetBounds> assetBounds_;
    std::uint64_t boundsRevision_ = ~std::uint64_t(0);
    std::optional<std::size_t> targetAsset_;
    std::optional<AssetSurfaceHit> targetSurface_;
    std::optional<HubAsset> preview_;
    std::deque<Edit> undo_, redo_;
    std::string message_;
    std::string shownMessage_;
    float messageSeconds_ = 0;
    SectionBuildMode buildMode_ = SectionBuildMode::Single;
    int brushSize_ = 3, pasteRotation_ = 0;
    bool pasteIncludesAir_ = false, toolPreviewValid_ = false, brushErasing_ = false;
    bool builderLight_ = true;
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
    bool pasteFillGaps_ = false, previewFillGaps_ = false;
    std::optional<int> pasteFloorHeight_;
    std::optional<glm::ivec3> pinnedPasteOrigin_;
    std::uint64_t pastePreviewRevision_ = 0;
    std::optional<std::size_t> editingChest_;
    std::array<ItemStack, ChestSlotCount> chestSlots_{};
    int chestSelection_ = 0;
    std::string chestQuantityText_ = "1";
    bool chestQuantitySelected_ = true;
    std::optional<std::size_t> editingSpawner_, spawnerNext_;
    bool pickingSpawnerLink_ = false;
    std::array<int, 3> spawnerCounts_{2, 1, 1};
    std::vector<ItemStack> spawnerRewards_;
};
} // namespace voxel
