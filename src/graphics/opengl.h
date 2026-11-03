#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "mesh.h"

#include <cstddef>

namespace voxel {

// Owns one linked shader program: the vertex and fragment code run by the GPU.
// Keep an initialized OpenGL context current for its lifetime; use() binds the
// program before setting uniforms or drawing.
class Shader {
public:
    Shader(const char* vertexSource, const char* fragmentSource);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void setInt(const char* name, int value) const;
    void setFloat(const char* name, float value) const;
    void setMat4(const char* name, const glm::mat4& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;

private:
    GLuint program_ = 0;
};

// Owns vertex/index buffers and the vertex array that describes their layout.
// upload() copies a CPU mesh to the GPU; draw() renders those indexed triangles.
// Keep an initialized OpenGL context current for its lifetime.
class GpuMesh {
public:
    GpuMesh();
    ~GpuMesh();

    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    void upload(const MeshData& mesh);
    void draw() const;
    // Draw complete triangles from the existing index buffer without uploading
    // another mesh. Invalid or non-triangle-aligned ranges throw before GL use.
    void drawRange(std::size_t firstIndex, std::size_t count) const;
    std::size_t indexCount() const;

private:
    GLuint vao_ = 0;
    GLuint vertexBuffer_ = 0;
    GLuint indexBuffer_ = 0;
    GLsizei indexCount_ = 0;
};

} // namespace voxel
