#pragma once

#include "graphics/opengl.h"
#include "graphics/post_process.h"
#include "graphics/light_grid.h"
#include "graphics/detail_lod.h"
#include "graphics/held_compass.h"
#include "graphics_settings.h"
#include "world.h"
#include "editing.h"
#include "hub_assets.h"
#include "monsters.h"
#include "encounters.h"
#include <memory>

#include <array>
#include <optional>
#include <vector>

namespace voxel {
class Camera;
struct SectionMetadata;
class SectionEditor;

// Each renderer keeps its own copy of the meshed chunks, so multiple views can
// share a world without consuming one another's pending updates.
class WorldRenderer {
public:
    WorldRenderer();
    ~WorldRenderer();
    void setRenderScale(float scale) { postProcess_.setRenderScale(scale); }
    void setSamples(int samples) { postProcess_.setSamples(samples); }
    void setLightingQuality(LightingQuality quality);
    // Reference path for comparing the spatial light lookup against every source.
    void setLightGridEnabled(bool enabled) { lightGridEnabled_ = enabled; }
    void render(const World& world, const Camera& camera, int width, int height, bool wireframe,
                bool underground = false, float timeSeconds = 0.0f, bool carriedLantern = false,
                bool hubAssets = true, std::span<const HubAsset> placedAssets = {},
                std::span<const glm::vec3> pickupOffsets = {},
                std::span<const MonsterInstance> monsters = {},
                std::span<const EncounterProjectile> projectiles = {},
                std::optional<float> heldSwordSwing = {},
                std::span<const EncounterHit> hits = {},
                std::optional<float> heldPickaxeSwing = {},
                std::optional<float> heldLanternTime = {}, bool darkCave = false,
                std::optional<HeldCompassState> heldCompass = {},
                std::span<const EncounterActivation> activations = {},
                int swordTier = 1, int pickaxeTier = 1);
    std::size_t triangleCount() const;
    std::size_t visibleChunks() const { return visibleChunks_; }
    std::size_t drawnTriangles() const { return drawnTriangles_; }
    std::size_t sceneRebuildCount() const { return sceneRebuildCount_; }

private:
    void uploadLights(const World& world);
    Shader shader_;
    PostProcess postProcess_;
    std::array<GpuMesh, World::ChunkCount> meshes_;
    std::array<GpuMesh, World::ChunkCount> water_; // Drawn after the opaque scene snapshot.
    std::array<GpuMesh, World::ChunkCount> details_;
    std::array<GpuMesh, World::ChunkCount> relief_;
    std::array<std::array<DetailLodRange,DetailLodBandCount>,World::ChunkCount> detailRanges_{}, reliefRanges_{};
    std::array<GpuMesh, World::ChunkCount> assets_;
    std::array<GpuMesh, ItemCount> pickups_; // Local meshes; animated with per-draw transforms.
    std::array<std::vector<std::unique_ptr<GpuMesh>>,3> monsters_; // Uploaded once per joint.
    GpuMesh activationRing_, activationBeam_;
    GpuMesh spellBolt_; // Shared by glowing wizard bolts and their short trails.
    std::array<GpuMesh,3> hitChips_; // Cached bone chips, amethyst shards, slime droplets.
    GpuMesh heldSword_;
    GpuMesh heldPickaxe_;
    int heldSwordTier_ = 0, heldPickaxeTier_ = 0;
    GpuMesh heldLantern_;
    GpuMesh heldCompass_, compassNeedle_;
    std::size_t heldSwordTriangles_ = 0;
    std::size_t heldPickaxeTriangles_ = 0;
    std::size_t heldLanternTriangles_ = 0;
    std::size_t heldCompassTriangles_ = 0;
    std::size_t monsterTriangles_ = 0;
    std::optional<World> meshedWorld_;
    std::vector<PointLight> lights_; // Rebuilt for world edits or a lighting-mode change.
    std::vector<HubAssetLight> assetLights_;
    GLuint lightTexture_ = 0; // Global source position/radius, then color.
    GLuint lightRanges_ = 0, lightIndices_ = 0; // Fixed four-block spatial bins.
    GLuint occupancy_ = 0; // Cube=255, stair=200..203, water=127, air/foliage=0; lighting and shoreline queries.
    std::size_t visibleChunks_ = 0;
    std::size_t drawnTriangles_ = 0;
    std::size_t pickupTriangles_ = 0, sceneRebuildCount_ = 0;
    bool underground_ = false;
    bool hubAssets_ = true;
    bool hubLighting_ = false;
    bool lightGridEnabled_ = true;
    LightingQuality lightingQuality_ = LightingQuality::High;
    std::vector<HubAsset> placedAssets_;
};

// Draws the selected block outline and the screen-center crosshair.
class Overlay {
public:
    Overlay();
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    void draw(const std::optional<RayHit>& hit, const Camera& camera, int width, int height,
              float contentScale = 1.0f, Block material = Block::Stone);
    void drawSection(const SectionMetadata& section, const std::optional<HubAsset>& preview,
                     const std::optional<HubAsset>& selectedAsset, const Camera& camera, int width, int height,
                     float timeSeconds = 0);
    void drawBuildTools(const SectionEditor& editor, const Camera& camera, int width, int height,
                       float timeSeconds = 0);
    void drawSpawnerLinks(const SectionEditor& editor, const Camera& camera, int width, int height,
                          float timeSeconds = 0);

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
    GpuMesh previewMesh_;
    GpuMesh markerCube_, startLabel_, endLabel_;
    GpuMesh spawnerLinks_;
    std::array<GpuMesh,10> numberLabels_;
    std::vector<HubAsset> linkedSpawners_;
    std::optional<HubAsset> previewAsset_;
    std::array<GpuMesh, ItemCount> pickupMeshes_;
    GpuMesh clipboardMesh_, pasteChangesMesh_;
    std::uint64_t pasteChangesRevision_ = ~std::uint64_t(0);
    std::uint64_t clipboardRevision_ = ~std::uint64_t(0);
    int clipboardRotation_ = -1;
};

} // namespace voxel
