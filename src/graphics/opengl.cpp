#include "graphics/opengl.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace voxel {

namespace {

std::string shaderLog(GLuint shader)
{
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return "No shader log was provided by the driver.";
    }
    std::vector<GLchar> log(static_cast<std::size_t>(length));
    GLsizei written = 0;
    glGetShaderInfoLog(shader, length, &written, log.data());
    return std::string(log.data(), static_cast<std::size_t>(written));
}

std::string programLog(GLuint program)
{
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return "No program log was provided by the driver.";
    }
    std::vector<GLchar> log(static_cast<std::size_t>(length));
    GLsizei written = 0;
    glGetProgramInfoLog(program, length, &written, log.data());
    return std::string(log.data(), static_cast<std::size_t>(written));
}

GLuint compileShader(GLenum type, const char* source, const char* stage)
{
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        throw std::runtime_error(std::string("Failed to create ") + stage + " shader.");
    }
    try {
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE) {
            throw std::runtime_error(std::string(stage) + " shader compilation failed:\n" + shaderLog(shader));
        }
    } catch (...) {
        glDeleteShader(shader);
        throw;
    }
    return shader;
}

} // namespace

Shader::Shader(const char* vertexSource, const char* fragmentSource)
{
    if (vertexSource == nullptr || fragmentSource == nullptr) {
        throw std::invalid_argument("Shader source must not be null.");
    }
    GLuint vertex = 0;
    GLuint fragment = 0;
    try {
        vertex = compileShader(GL_VERTEX_SHADER, vertexSource, "Vertex");
        fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "Fragment");
        program_ = glCreateProgram();
        if (program_ == 0) {
            throw std::runtime_error("Failed to create shader program.");
        }
        glAttachShader(program_, vertex);
        glAttachShader(program_, fragment);
        glLinkProgram(program_);
        GLint linked = GL_FALSE;
        glGetProgramiv(program_, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            throw std::runtime_error("Shader program linking failed:\n" + programLog(program_));
        }
        glDetachShader(program_, vertex);
        glDetachShader(program_, fragment);
    } catch (...) {
        if (program_ != 0) {
            glDeleteProgram(program_);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        throw;
    }
    glDeleteShader(fragment);
    glDeleteShader(vertex);
}

Shader::~Shader()
{
    glDeleteProgram(program_);
}

void Shader::use() const
{
    glUseProgram(program_);
}

void Shader::setInt(const char* name, int value) const
{
    glUniform1i(glGetUniformLocation(program_, name), value);
}

void Shader::setMat4(const char* name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(program_, name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const char* name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(program_, name), 1, glm::value_ptr(value));
}

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
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(MeshVertex, material)));
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
