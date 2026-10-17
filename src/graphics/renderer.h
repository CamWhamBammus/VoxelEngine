#pragma once

#include "graphics/opengl.h"
#include "world.h"
#include "editing.h"

#include <array>
#include <optional>

namespace voxel {
class Camera;
class Challenge;

// Each renderer keeps its own copy of the meshed chunks, so multiple views can
// share a world without consuming one another's pending updates.
class WorldRenderer {
public:
    WorldRenderer();
    void render(const World& world, const Camera& camera, int width, int height, bool wireframe);
    std::size_t triangleCount() const;
    std::size_t visibleChunks() const { return visibleChunks_; }
    std::size_t drawnTriangles() const { return drawnTriangles_; }

private:
    Shader shader_;
    std::array<GpuMesh, World::ChunkCount> meshes_;
    std::optional<World> meshedWorld_;
    std::size_t visibleChunks_ = 0;
    std::size_t drawnTriangles_ = 0;
};

// Bright world-space markers for the collectible crystals and return camp.
class ObjectivesRenderer {
public:
    ObjectivesRenderer();
    void draw(const Challenge& challenge, const Camera& camera, int width, int height,
              float elapsedSeconds = 0.0f);

private:
    Shader shader_;
    GpuMesh cube_;
};

// Draws the selected block outline and the screen-center crosshair.
class Overlay {
public:
    Overlay();
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    void draw(const std::optional<RayHit>& hit, const Camera& camera, int width, int height,
              float contentScale = 1.0f);

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
};

} // namespace voxel
