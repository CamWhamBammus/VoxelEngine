#include "render/overlay.h"
#include "core/camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>

namespace voxel {
namespace {
constexpr const char* vertexShader = R"glsl(#version 410 core
layout(location = 0) in vec3 position;
uniform mat4 transform;
void main() { gl_Position = transform * vec4(position, 1.0); }
)glsl";
constexpr const char* fragmentShader = R"glsl(#version 410 core
uniform vec3 color;
out vec4 fragmentColor;
void main() { fragmentColor = vec4(color, 1.0); }
)glsl";
} // namespace

Overlay::Overlay() : shader_(vertexShader, fragmentShader)
{
    const std::array<glm::vec3, 28> lines{{
        {0,0,0}, {1,0,0}, {1,0,0}, {1,0,1}, {1,0,1}, {0,0,1}, {0,0,1}, {0,0,0},
        {0,1,0}, {1,1,0}, {1,1,0}, {1,1,1}, {1,1,1}, {0,1,1}, {0,1,1}, {0,1,0},
        {0,0,0}, {0,1,0}, {1,0,0}, {1,1,0}, {1,0,1}, {1,1,1}, {0,0,1}, {0,1,1},
        {-8,0,0}, {8,0,0}, {0,-8,0}, {0,8,0},
    }};
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &buffer_);
    if (!vao_ || !buffer_) {
        glDeleteBuffers(1, &buffer_);
        glDeleteVertexArrays(1, &vao_);
        throw std::runtime_error("Failed to allocate overlay resources");
    }
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Overlay::~Overlay()
{
    glDeleteBuffers(1, &buffer_);
    glDeleteVertexArrays(1, &vao_);
}

void Overlay::draw(const std::optional<RayHit>& hit, const Camera& camera, int width, int height)
{
    if (width <= 0 || height <= 0) return;
    shader_.use();
    glBindVertexArray(vao_);
    glDepthMask(GL_FALSE);
    if (hit) {
        const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f),
            glm::vec3(hit->position) - glm::vec3(0.002f)), glm::vec3(1.004f));
        shader_.setMat4("transform", camera.projectionMatrix(static_cast<float>(width) / height)
                                   * camera.viewMatrix() * model);
        shader_.setVec3("color", {1.0f, 0.9f, 0.55f});
        glDrawArrays(GL_LINES, 0, 24);
    }
    glDisable(GL_DEPTH_TEST);
    shader_.setMat4("transform", glm::scale(glm::mat4(1.0f),
                                           {2.0f / width, 2.0f / height, 1.0f}));
    shader_.setVec3("color", {1.0f, 1.0f, 1.0f});
    glDrawArrays(GL_LINES, 24, 4);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

} // namespace voxel
