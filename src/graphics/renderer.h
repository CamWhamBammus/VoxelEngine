#pragma once

#include "graphics/opengl.h"
#include "graphics/post_process.h"
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
    void render(const World& world, const Camera& camera, int width, int height, bool wireframe,
                bool underground = false, float timeSeconds = 0.0f, bool carriedLantern = false,
                bool hubAssets = true, std::span<const HubAsset> placedAssets = {},
                std::span<const glm::vec3> pickupOffsets = {},
                std::span<const MonsterInstance> monsters = {},
                std::span<const EncounterProjectile> projectiles = {},
                std::optional<float> heldSwordSwing = {},
                std::span<const EncounterHit> hits = {},
                std::optional<float> heldPickaxeSwing = {},
                std::optional<float> heldLanternTime = {}, bool darkCave = false);
    std::size_t triangleCount() const;
    std::size_t visibleChunks() const { return visibleChunks_; }
    std::size_t drawnTriangles() const { return drawnTriangles_; }
    std::size_t sceneRebuildCount() const { return sceneRebuildCount_; }

private:
    struct Light { glm::vec3 position; glm::vec3 color; float radius; };
    void uploadLights(const World& world);
    Shader shader_;
    PostProcess postProcess_;
    std::array<GpuMesh, World::ChunkCount> meshes_;
    std::array<GpuMesh, World::ChunkCount> water_; // Drawn after the opaque scene snapshot.
    std::array<GpuMesh, World::ChunkCount> details_;
    std::array<GpuMesh, World::ChunkCount> relief_;
    std::array<GpuMesh, World::ChunkCount> assets_;
    std::array<GpuMesh, ItemCount> pickups_; // Local meshes; animated with per-draw transforms.
    std::array<std::vector<std::unique_ptr<GpuMesh>>,3> monsters_; // Uploaded once per joint.
    GpuMesh spellBolt_; // Shared by glowing wizard bolts and their short trails.
    std::array<GpuMesh,3> hitChips_; // Cached bone chips, amethyst shards, slime droplets.
    GpuMesh heldSword_;
    GpuMesh heldPickaxe_;
    GpuMesh heldLantern_;
    std::size_t heldSwordTriangles_ = 0;
    std::size_t heldPickaxeTriangles_ = 0;
    std::size_t heldLanternTriangles_ = 0;
    std::size_t monsterTriangles_ = 0;
    std::optional<World> meshedWorld_;
    std::vector<Light> lights_; // Rebuilt only when world contents change.
    std::array<int, World::ChunkCount> lightOffsets_{}, lightCounts_{};
    GLuint lightTexture_ = 0; // Per-chunk candidates: position/radius, then color.
    GLuint occupancy_ = 0; // Cube=255, stair=200..203, water=127, air/foliage=0; lighting and shoreline queries.
    std::size_t visibleChunks_ = 0;
    std::size_t drawnTriangles_ = 0;
    std::size_t pickupTriangles_ = 0, sceneRebuildCount_ = 0;
    bool underground_ = false;
    bool hubAssets_ = true;
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

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
    GpuMesh previewMesh_;
    GpuMesh markerCube_, startLabel_, endLabel_;
    std::optional<HubAsset> previewAsset_;
    std::array<GpuMesh, ItemCount> pickupMeshes_;
    GpuMesh clipboardMesh_, pasteChangesMesh_;
    std::uint64_t pasteChangesRevision_ = ~std::uint64_t(0);
    std::uint64_t clipboardRevision_ = ~std::uint64_t(0);
    int clipboardRotation_ = -1;
};

} // namespace voxel
