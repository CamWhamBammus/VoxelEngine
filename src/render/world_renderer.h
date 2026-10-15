#pragma once

#include "render/gpu_mesh.h"
#include "render/shader.h"
#include "voxel/world.h"

#include <array>

namespace voxel {
class Camera;

class WorldRenderer {
public:
    WorldRenderer();
    void render(World& world, const Camera& camera, int width, int height, bool wireframe);
    std::size_t triangleCount() const;
    std::size_t visibleChunks() const { return visibleChunks_; }
    std::size_t drawnTriangles() const { return drawnTriangles_; }

private:
    Shader shader_;
    std::array<GpuMesh, World::ChunkCount> meshes_;
    std::size_t visibleChunks_ = 0;
    std::size_t drawnTriangles_ = 0;
};

} // namespace voxel
