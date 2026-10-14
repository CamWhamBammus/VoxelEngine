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

private:
    Shader shader_;
    std::array<GpuMesh, World::ChunkCount> meshes_;
};

} // namespace voxel
