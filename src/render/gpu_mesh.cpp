#include "render/gpu_mesh.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace voxel {

static_assert(std::is_standard_layout_v<MeshVertex>);
static_assert(sizeof(MeshVertex) <= static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()));

GpuMesh::GpuMesh()
{
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vertexBuffer_);
    glGenBuffers(1, &indexBuffer_);
    if (vao_ == 0 || vertexBuffer_ == 0 || indexBuffer_ == 0) {
        glDeleteBuffers(1, &indexBuffer_);
        glDeleteBuffers(1, &vertexBuffer_);
        glDeleteVertexArrays(1, &vao_);
        throw std::runtime_error("Failed to allocate OpenGL mesh resources.");
    }
}

GpuMesh::~GpuMesh()
{
    glDeleteBuffers(1, &indexBuffer_);
    glDeleteBuffers(1, &vertexBuffer_);
    glDeleteVertexArrays(1, &vao_);
}

void GpuMesh::upload(const MeshData& mesh)
{
    const auto maxBytes = static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max());
    if (mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
        mesh.vertices.size() > maxBytes / sizeof(MeshVertex) ||
        mesh.indices.size() > maxBytes / sizeof(std::uint32_t)) {
        throw std::length_error("Mesh exceeds OpenGL buffer or draw count limits.");
    }
    for (const auto index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            throw std::invalid_argument("Mesh index references a missing vertex.");
        }
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(MeshVertex)),
                 mesh.vertices.empty() ? nullptr : mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                 mesh.indices.empty() ? nullptr : mesh.indices.data(), GL_STATIC_DRAW);

    constexpr auto stride = static_cast<GLsizei>(sizeof(MeshVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(MeshVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(MeshVertex, color)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    indexCount_ = static_cast<GLsizei>(mesh.indices.size());
}

void GpuMesh::draw() const
{
    if (indexCount_ == 0) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

std::size_t GpuMesh::indexCount() const
{
    return static_cast<std::size_t>(indexCount_);
}

} // namespace voxel
