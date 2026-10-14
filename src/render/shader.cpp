#include "render/shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>
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

void Shader::setMat4(const char* name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(program_, name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const char* name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(program_, name), 1, glm::value_ptr(value));
}

} // namespace voxel
