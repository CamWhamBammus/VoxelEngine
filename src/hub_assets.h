#pragma once

#include "mesh.h"
#include <array>
#include <vector>

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
    Forge, Anvil, ToolRack, SmithWorkbench, AnvilSign
};

struct HubAsset {
    HubAssetKind kind;
    glm::vec3 position;
    int facing = 0;
    std::uint32_t variation = 0;
};

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

} // namespace voxel
