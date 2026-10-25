#pragma once

#include "camera.h"
#include "editing.h"
#include "equipment.h"
#include "encounters.h"
#include "health.h"
#include "level_editor.h"
#include "loot.h"
#include "mining.h"
#include "monster_preview.h"
#include "player.h"
#include "root_crossings.h"
#include "section_editor.h"
#include "world.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace voxel {

// Startup settings. The command line is read once, before opening the window.
struct Options {
    bool monsters = false;
    bool editor = false;
    bool dev = false;
    bool levelCreator = false;
    std::string levelLibrary = "levels";
    std::string sectionLibrary = "sections";
    int rootCrossing = -1; // -1 starts the hub; otherwise a zero-based test course.
    std::uint32_t seed = 1337;
    int editReach = 8;
    bool seedExplicit = false;
    bool smokeTest = false;
    bool help = false;
    bool worldExplicit = false;
    std::string screenshot;
    std::string worldPath = "world.vxl";
    std::string progressPath; // Empty disables profile I/O for tests and smoke runs.
};

inline constexpr const char* Usage =
    "Usage: VoxelEngine [options]\n\n"
    "  --monsters         Open the creature animation preview.\n"
    "  --dev              Open the section builder and world library.\n"
    "  --level-creator    Assemble accepted sections into playable numbered levels.\n"
    "  --editor           Open the flying terrain editor instead of the cave sample.\n"
    "  --root-crossing N  Start Root Crossing test 1 to 10 directly.\n"
    "  --seed N           Generate terrain from an unsigned 32-bit seed (default: 1337).\n"
    "  --world path.vxl   Choose the save file (default: world.vxl).\n"
    "  --reach N          Editor reach in blocks, 1 to 64 (default: 8).\n"
    "  --smoke-test       Check rendering in a hidden window, then exit.\n"
    "  --screenshot path.ppm\n"
    "                     Run the smoke test and write a rendered image.\n"
    "  -h, --help         Show this help without opening a window.\n\n"
    "The village terrain starts fresh; inventory and upgrades use progress.vxp.\n"
    "Editor startup loads world.vxl unless --seed is given.\n"
    "In the editor, an explicit existing --world file takes precedence over --seed.\n"
    "Use --editor with --world; game progress is not saved to world files.\n"
    "Smoke tests only load files explicitly selected with --world.\n"
    "Press H for controls and Q to quit. Editor: F5 saves, F9 reloads.\n"
    "Editor changes are not saved automatically on exit.\n\n"
    "Examples:\n"
    "  VoxelEngine --seed 42\n"
    "  VoxelEngine --editor --world my-world.vxl\n"
    "  VoxelEngine --seed 42 --screenshot terrain.ppm\n";
Options parseOptions(std::span<const std::string_view> arguments);

struct InputActions;

// Everything the player is working with. OpenGL and keyboard polling live elsewhere.
class Game {
public:
    explicit Game(const Options& options);
    void update(const InputActions& actions, float deltaSeconds = 0.0f);
    const Health& health() const { return health_; }
    bool healthVisible() const;
    std::string_view respawnDestination() const;
    bool damagePlayer(int amount, glm::vec3 direction = glm::vec3(0));
    glm::vec3 playerHitDirection() const { return playerHitDirection_; }
    Camera viewCamera() const;
    void restoreHealth();
    // Mining may remove a marker's support. Respawn on surviving ground nearby
    // without regenerating the section or changing any collection records.
    bool placeAtSectionMarker(glm::vec3 feet);
    // Called after movement, before checking checkpoints or section exits.
    bool checkPlayerDamage(float deathHeight);
    bool updateCombat(const InputActions& actions, float seconds);
    const EncounterSession& encounters() const { return encounters_; }
    const MiningSession& mining() const { return mining_; }
    const std::string& miningMessage() const { return miningMessage_; }
    bool sectionEncountersCleared() const { return encounters_.remaining() == 0; }
    bool monsterPreviewActive() const { return monsterPreviewActive_; }
    const MonsterPreview& monsterPreview() const { return monsterPreview_; }
    std::span<const MonsterInstance> monsters() const
    { return monsterPreviewActive_ ? monsterPreview_.monsters() : encounters_.monsters(); }
    bool editorMode() const { return editorMode_; }
    bool devMode() const { return devMode_; }
    // Render the current section's lighting even while a menu or transition is open.
    bool darkCave() const;
    bool carriedLantern() const;
    bool devFlying() const { return devMode_ && sectionEditor_.flying(); }
    bool devMenuOpen() const { return devMode_ && sectionEditor_.menuOpen(); }
    SectionEditor& sectionEditor() { return sectionEditor_; }
    const SectionEditor& sectionEditor() const { return sectionEditor_; }
    bool hasUnsavedChanges() const;
    bool saveAuthoringDraft();
    bool levelCreatorActive() const { return levelController_.open() && levelController_.creator(); }
    bool levelMenuOpen() const { return levelController_.menuOpen(); }
    bool levelActive() const { return levelController_.active(); }
    LevelController& levelController() { return levelController_; }
    const LevelController& levelController() const { return levelController_; }
    bool smithOpen() const { return smithOpen_; }
    bool inventoryOpen() const { return inventoryOpen_; }
    bool chestOpen() const { return openChest_.has_value(); }
    int inventorySelection() const { return inventorySelection_; }
    const std::vector<ItemStack>& chestContents() const;
    const std::string& chestName() const { return chestName_; }
    const std::string& inventoryMessage() const { return inventoryMessage_; }
    const std::string& lootPrompt() const { return lootPrompt_; }
    // Called by section mode transitions, never by a checkpoint respawn.
    void beginSectionLoot(std::span<const HubAsset> assets, bool playtest, std::string_view section = {});
    void endSectionLoot();
    std::span<const HubAsset> sectionSceneAssets() const;
    std::span<const glm::vec3> sectionPickupOffsets() const
    {
        return sectionLootActive_ ? std::span<const glm::vec3>(loot_.scenePickupOffsets()) : std::span<const glm::vec3>{};
    }
    std::shared_ptr<const AssetCollision> sectionSceneCollision() const
    {
        return sectionLootActive_ ? loot_.collision() : sectionEditor_.assetCollision();
    }
    bool nearBlacksmith() const;
    bool crossingMenuOpen() const { return crossingMenuOpen_; }
    bool rootCrossingActive() const { return rootCrossingIndex_ >= 0; }
    int rootCrossingIndex() const { return rootCrossingIndex_; }
    int crossingMenuSelection() const { return crossingMenuSelection_; }
    bool crossingCompleted() const { return crossingCompleted_; }
    bool crossingCheckpointReached() const { return crossingCheckpointReached_; }
    float crossingElapsed() const { return crossingElapsed_; }
    float crossingBestTime() const;
    int crossingFalls() const { return crossingFalls_; }
    bool nearExpeditionGate() const;
    glm::vec3 compassTarget() const;
    bool menuOpen() const { return inventoryOpen_ || smithOpen_ || crossingMenuOpen_ || devMenuOpen() || levelMenuOpen(); }
    std::string statusText(std::size_t visibleChunks, std::size_t drawnTriangles,
                           std::size_t totalTriangles, double fps,
                           bool captured, bool showHelp) const;

    World world;
    Camera camera;
    Player player;
    EditHistory history;
    Equipment equipment;
    Block selectedBlock = Block::Grass;
    std::optional<RayHit> target;
    std::string status;

private:
    friend class LevelController;
    void resetPlayerHit();
    void updateMining(const InputActions& actions, float seconds);
    void updateDeath(const InputActions& actions, float deltaSeconds);
    void enterMonsterPreview();
    void resetMonsterCamera();
    void updateMonsters(const InputActions& actions, float deltaSeconds);
    void enterLevelMode(bool creator);
    void leaveLevelMode();
    void buildVillage();
    bool returnToVillage();
    void enterDevMode();
    void leaveDevMode();
    void updatePlay(const InputActions& actions, float deltaSeconds);
    void upgradeEquipment();
    bool updateInventory(const InputActions& actions, float deltaSeconds);
    bool commitEquipment(const Equipment& next, std::string_view action);
    void refreshLootPrompt();
    void collectNearbyItems(float deltaSeconds);
    void closeInventory();
    bool openLoot(std::size_t index);
    void takeLoot(std::size_t index, std::optional<std::size_t> slot = {});
    void discardSectionKeys();
    void openCrossingMenu();
    void startRootCrossing(int index);
    void leaveRootCrossing();
    void resetRootCrossing(bool fullRestart, bool fallen = false);
    void positionForCrossing(glm::vec3 feet, bool checkpoint = false);
    void updateRootCrossing(float deltaSeconds);
    std::string playStatusText(bool captured, bool showHelp) const;
    void handleFiles(const InputActions& actions);
    void handleUndoRedo(const InputActions& actions);
    void findTarget();
    void editTarget(const InputActions& actions);

    Health health_;
    static constexpr float PlayerKnockbackDuration = .22f;
    static constexpr float PlayerKnockbackDistance = .40f;
    glm::vec3 playerHitDirection_{0};
    float playerKnockbackRemaining_ = 0;
    EncounterSession encounters_;
    MiningSession mining_;
    std::string miningMessage_;
    float miningMessageSeconds_ = 0;
    float encounterSaveRetry_ = 0;
    MonsterPreview monsterPreview_;
    bool monsterPreviewActive_ = false;
    SectionEditor sectionEditor_;
    LevelController levelController_;
    bool devMode_ = false;
    World savedWorld_;
    std::string savePath_;
    int editReach_;
    bool editorMode_;
    bool smithOpen_ = false;
    std::string progressPath_;
    std::string progressError_;
    LootSession loot_;
    struct HeldKey { std::string source; std::size_t slot; int count; };
    std::vector<HeldKey> heldKeys_;
    // A failed/full pickup retries after leaving its reach or changing the bag.
    std::vector<std::optional<Inventory>> pickupAttempts_;
    bool sectionLootActive_ = false;
    std::optional<Equipment> playtestEquipment_;
    bool inventoryOpen_ = false;
    int inventorySelection_ = -1;
    std::optional<std::size_t> openChest_;
    std::string inventoryMessage_, chestName_, lootPrompt_;
    RootCrossingCourse crossingCourse_;
    int rootCrossingIndex_ = -1;
    int crossingMenuSelection_ = 0;
    bool crossingMenuOpen_ = false;
    bool crossingCompleted_ = false;
    bool crossingCheckpointReached_ = false;
    float crossingElapsed_ = 0.0f;
    int crossingFalls_ = 0;
    std::array<float, RootCrossingCount> crossingBestTimes_{};
};

} // namespace voxel
