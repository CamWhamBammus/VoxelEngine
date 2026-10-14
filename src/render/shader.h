#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

namespace voxel {

// An initialized OpenGL context must remain current during construction, use,
// and destruction. Call use() before setting uniforms.
class Shader {
public:
    Shader(const char* vertexSource, const char* fragmentSource);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void setMat4(const char* name, const glm::mat4& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;

private:
    GLuint program_ = 0;
};

} // namespace voxel
