#pragma once

#include "graphics/opengl.h"
#include "world.h"
#include "editing.h"

#include <array>
#include <optional>

namespace voxel {
class Camera;

// Keeps one GPU mesh per chunk and rebuilds it only when the world marks it dirty.
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

// Draws the selected block outline and the screen-center crosshair.
class Overlay {
public:
    Overlay();
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    void draw(const std::optional<RayHit>& hit, const Camera& camera, int width, int height);

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
};

} // namespace voxel
