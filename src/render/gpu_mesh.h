#pragma once

#include <GL/glew.h>

#include "voxel/mesh.h"

#include <cstddef>

namespace voxel {

// Requires a current, initialized OpenGL context for its entire lifetime.
class GpuMesh {
public:
    GpuMesh();
    ~GpuMesh();

    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    void upload(const MeshData& mesh);
    void draw() const;
    std::size_t indexCount() const;

private:
    GLuint vao_ = 0;
    GLuint vertexBuffer_ = 0;
    GLuint indexBuffer_ = 0;
    GLsizei indexCount_ = 0;
};

} // namespace voxel
