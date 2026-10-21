#pragma once

#include "graphics/opengl.h"
#include "hub_assets.h"

#include <array>
#include <span>

namespace voxel {

// Actual world meshes and material paint, rendered once into transparent tiles.
// Keep the OpenGL context current for construction, ensure(), and destruction.
class PalettePreviews {
public:
    static constexpr int TileSize = 128, Columns = 12;
    static constexpr int Count = static_cast<int>(BlockCount + HubAssetCount) + ItemCount;
    static constexpr int Rows = (Count + Columns - 1) / Columns;
    static constexpr int Width = Columns * TileSize, Height = Rows * TileSize;
    static constexpr int blockId(Block block) { return static_cast<int>(block); }
    static constexpr int assetId(HubAssetKind asset) { return static_cast<int>(BlockCount) + static_cast<int>(asset); }
    static constexpr int itemId(ItemId item) { return static_cast<int>(BlockCount + HubAssetCount) + static_cast<int>(item); }
    // {uMin,vMin,uMax,vMax}; V follows OpenGL's bottom-to-top convention.
    static glm::vec4 uvBounds(int id);

    PalettePreviews();
    ~PalettePreviews();
    PalettePreviews(const PalettePreviews&) = delete;
    PalettePreviews& operator=(const PalettePreviews&) = delete;

    // Invalid IDs are ignored; repeated requests do not touch GL state.
    // The first request allocates the atlas. All modified caller state is restored.
    void ensure(std::span<const int> ids);
    GLuint texture() const { return texture_; }
    std::size_t generatedCount() const { return generatedCount_; }

private:
    void allocate();
    void drawTile(int id);
    Shader shader_;
    GpuMesh mesh_, relief_;
    GLuint texture_ = 0, framebuffer_ = 0, depth_ = 0;
    std::array<bool, Count> ready_{};
    std::size_t generatedCount_ = 0;
    bool allocated_ = false;
};

} // namespace voxel
