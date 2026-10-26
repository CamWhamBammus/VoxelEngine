#pragma once

#include "mesh.h"
#include "items.h"
#include <array>
#include <span>
#include <utility>
#include <vector>
#include <glm/mat4x4.hpp>

namespace voxel {

// Small handcrafted models supplement the meter-sized terrain blocks.
// Position places a model's origin in the world; facing turns its local +Z
// toward world +Z/+X/-Z/-X in quarter turns.
enum class HubAssetKind {
    LanternCap, WallBracket, HangingLamp, CrystalBeacon,
    WindowBox, Shutters, Awning, RoofBrace, Banner, CrateStack,
    Fern, Flowers, Mushrooms, Reeds, LilyPads, Vines, Shrub,
    PathPaving, PathEdging, PathStep,
    Boulder, RockLedge, StalactiteCluster, RootArch, RootCurtain,
    BroadleafClump, FernBed, MossGarden,
    Dormer, BayWindow, Chimney, Balcony, PorchCanopy,
    Forge, Anvil, ToolRack, SmithWorkbench, AnvilSign,
    TreeFern, LushSapling, HangingRoots, BlossomVines,
    GlowMushroomColony, GiantMushroom, ShelfFungi, CaveLotus,
    WaterGrass, MossyLog, RootFan, MossyStones,
    CrystalFlower, SporeStalks, CaveOrchid, HangingPlanter,
    LootChest, CrystalChest, IronboundChest, KeyPickup, ItemPickup,
    // Append new kinds: numeric IDs are stored in authored section files.
    TimberFence, PicketFence, FenceCorner, GardenGate, IronRailing,
    TimberPillar, TimberBeam, DiagonalBrace, StoneDoorway, WindowFrame, WoodenStairs, Barrel,
    TrialSpawner,
    JungleMonstera, JungleCycad, BambooClump, FiddleheadFerns,
    HangingFern, MossDrape, PitcherPlants, SeedPodCluster,
    RootBridge, RootStump, MossyPlatform, MossyColumn,
    OvergrownArch, RuinedPillar, RuinedWall, RuinSlab
};

inline constexpr std::size_t HubAssetCount = static_cast<std::size_t>(HubAssetKind::RuinSlab) + 1;

inline constexpr int SpawnerMobLimit = 20;
struct SpawnerSettings {
    // Skeleton, wizard, slime. The order matches MonsterKind.
    std::array<int, 3> counts{2, 1, 1};
    // Section-local identity keeps links stable when scenery is removed or moved.
    // Zero means unassigned/no successor. Only linked spawners need an identity.
    std::uint32_t id = 0;
    std::uint32_t next = 0;
    bool operator==(const SpawnerSettings&) const = default;
};
bool validSpawnerSettings(const SpawnerSettings& settings);

struct HubAsset {
    HubAssetKind kind;
    glm::vec3 position;
    int facing = 0;
    std::uint32_t variation = 0;
    // Authored loot is independent of the chest's appearance. New chests are empty.
    std::vector<ItemStack> chestItems{};
    // Generic ground pickups hold one explicit stack. Older KeyPickup assets
    // retain their implicit single key and keep this field empty.
    ItemStack pickupItem{};
    SpawnerSettings spawner{};
    bool operator==(const HubAsset&) const = default;
};

constexpr bool isChestAsset(HubAssetKind kind)
{
    return kind == HubAssetKind::LootChest || kind == HubAssetKind::CrystalChest
        || kind == HubAssetKind::IronboundChest;
}
constexpr bool isKeyPickup(HubAssetKind kind) { return kind == HubAssetKind::KeyPickup; }
constexpr bool isSpawnerAsset(HubAssetKind kind) { return kind == HubAssetKind::TrialSpawner; }
constexpr bool isItemPickup(HubAssetKind kind)
{
    return isKeyPickup(kind) || kind == HubAssetKind::ItemPickup;
}
// Keep the old staircase readable in saved sections; new builds use voxel stairs.
constexpr bool isCreativeAsset(HubAssetKind kind)
{
    return !isItemPickup(kind) && kind != HubAssetKind::WoodenStairs;
}
ItemStack itemPickupStack(const HubAsset& asset);
// Pickups keep a floor anchor in the document. Their cached local mesh uses
// this transform only while drawing; gameplay and selection use the envelope.
inline constexpr glm::vec3 PickupVisualCenter{0,.55f,0};
glm::mat4 pickupTransform(const HubAsset& asset, float timeSeconds);
std::pair<glm::vec3,glm::vec3> pickupVisualBounds(const HubAsset& asset);
std::string_view chestAssetName(HubAssetKind kind);
// Validate all item payloads: chest contents or one generic pickup stack.
// Other models, including legacy KeyPickup, have a canonical empty pickupItem.
bool validChestItems(const HubAsset& asset);

struct HubAssetLight {
    glm::vec3 position;
    glm::vec3 color;
    float radius;
};

constexpr bool isPathAsset(HubAssetKind kind)
{
    return kind >= HubAssetKind::PathPaving && kind <= HubAssetKind::PathStep;
}

constexpr bool isBlacksmithAsset(HubAssetKind kind)
{
    return kind >= HubAssetKind::Forge && kind <= HubAssetKind::AnvilSign;
}

constexpr bool isLushAsset(HubAssetKind kind)
{
    return kind >= HubAssetKind::TreeFern && kind <= HubAssetKind::HangingPlanter;
}

constexpr bool isJunglePlant(HubAssetKind kind)
{
    return kind >= HubAssetKind::JungleMonstera && kind <= HubAssetKind::SeedPodCluster;
}

constexpr bool isJungleStructure(HubAssetKind kind)
{
    return kind >= HubAssetKind::RootBridge && kind <= HubAssetKind::RuinSlab;
}

constexpr bool isFenceAsset(HubAssetKind kind)
{
    return kind >= HubAssetKind::TimberFence && kind <= HubAssetKind::IronRailing;
}

constexpr bool isBuildingKitAsset(HubAssetKind kind)
{
    return kind >= HubAssetKind::TimberPillar && kind <= HubAssetKind::Barrel;
}

// Building modules occupy whole grid cells even when their visible rails or
// beams are thin. Zero means free-form scenery with surface-based placement.
// Anchors are centered in X/Z and sit at the bottom of the module.
constexpr glm::ivec3 buildingGridSize(HubAssetKind kind, int facing = 0)
{
    if (kind == HubAssetKind::StoneDoorway)
        return facing & 1 ? glm::ivec3(1,3,3) : glm::ivec3(3,3,1);
    if (isFenceAsset(kind) || kind == HubAssetKind::TimberPillar
        || kind == HubAssetKind::TimberBeam || kind == HubAssetKind::DiagonalBrace
        || kind == HubAssetKind::WindowFrame || kind == HubAssetKind::Barrel || isSpawnerAsset(kind))
        return {1,1,1};
    return {};
}

// Lush ground models sit on their origin. HangingRoots/HangingPlanter grow
// down from a ceiling; BlossomVines/ShelfFungi/RootFan grow out of a local
// Z=0 wall toward +Z. CaveLotus/WaterGrass use the water surface as their origin.
// HangingFern also grows down from Y=0; MossDrape grows out of a Z=0 wall.
// Other jungle models stand on Y=0, including the root bridge's two footings.

// Includes full model extents for chunk culling and light candidate bounds.
inline constexpr float HubAssetMargin = 4.0f;
struct HubDressing {
    std::array<MeshData, World::ChunkCount> meshes;
    std::vector<HubAssetLight> lights;
};

std::vector<HubAsset> planHubAssets(const World& world);
std::vector<HubAsset> planLandscapeAssets(const World& world);
MeshData buildHubAsset(const HubAsset& asset);
HubDressing buildHubDressing(const World& world);
// Explicit placements used by authored sections, without village auto-dressing.
HubDressing buildAssetDressing(std::span<const HubAsset> assets, bool standaloneLights = true);

} // namespace voxel
