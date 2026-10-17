#pragma once

#include "graphics/opengl.h"
#include "graphics/post_process.h"
#include "world.h"
#include "editing.h"

#include <array>
#include <optional>
#include <vector>

namespace voxel {
class Camera;

// Each renderer keeps its own copy of the meshed chunks, so multiple views can
// share a world without consuming one another's pending updates.
class WorldRenderer {
public:
    WorldRenderer();
    ~WorldRenderer();
    void render(const World& world, const Camera& camera, int width, int height, bool wireframe,
                bool underground = false);
    std::size_t triangleCount() const;
    std::size_t visibleChunks() const { return visibleChunks_; }
    std::size_t drawnTriangles() const { return drawnTriangles_; }

private:
    struct Light { glm::vec3 position; glm::vec3 color; float radius; };
    Shader shader_;
    PostProcess postProcess_;
    std::array<GpuMesh, World::ChunkCount> meshes_;
    std::array<GpuMesh, World::ChunkCount> details_;
    std::optional<World> meshedWorld_;
    std::vector<Light> lights_; // Rebuilt only when world contents change.
    GLuint occupancy_ = 0; // One byte per voxel for contact shading and light occlusion.
    std::size_t visibleChunks_ = 0;
    std::size_t drawnTriangles_ = 0;
    bool underground_ = false;
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
