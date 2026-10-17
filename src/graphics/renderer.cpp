#include "graphics/renderer.h"
#include "camera.h"
#include "challenge.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace voxel {

namespace {

constexpr const char* worldVertexShader = R"glsl(#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
uniform mat4 viewProjection;
out vec3 worldPosition;
out vec3 surfaceNormal;
out vec3 surfaceColor;
void main() {
    worldPosition = position;
    surfaceNormal = normal;
    surfaceColor = color;
    gl_Position = viewProjection * vec4(position, 1.0);
}
)glsl";

constexpr const char* worldFragmentShader = R"glsl(#version 410 core
in vec3 worldPosition;
in vec3 surfaceNormal;
in vec3 surfaceColor;
uniform vec3 cameraPosition;
out vec4 fragmentColor;
void main() {
    float sun = max(dot(normalize(surfaceNormal), normalize(vec3(-0.6, 1.0, 0.3))), 0.0);
    vec3 lit = surfaceColor * (0.45 + 0.55 * sun);
    float fog = smoothstep(55.0, 150.0, distance(cameraPosition, worldPosition));
    fragmentColor = vec4(mix(lit, vec3(0.48, 0.70, 0.90), fog), 1.0);
}
)glsl";

constexpr const char* objectiveVertexShader = R"glsl(#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
uniform mat4 transform;
out float brightness;
void main() {
    brightness = 0.78 + 0.22 * max(dot(normal, normalize(vec3(0.4, 1.0, 0.2))), 0.0);
    gl_Position = transform * vec4(position, 1.0);
}
)glsl";
constexpr const char* objectiveFragmentShader = R"glsl(#version 410 core
in float brightness;
uniform vec3 color;
out vec4 fragmentColor;
void main() { fragmentColor = vec4(color * brightness, 1.0); }
)glsl";

} // namespace

WorldRenderer::WorldRenderer() : shader_(worldVertexShader, worldFragmentShader) {}

void WorldRenderer::render(const World& world, const Camera& camera, int width, int height, bool wireframe)
{
    if (width <= 0 || height <= 0) return;
    std::array<bool, World::ChunkCount> rebuild{};
    for (std::size_t index = 0; index < meshes_.size(); ++index) {
        if (meshedWorld_ && world.chunk(index) == meshedWorld_->chunk(index)) continue;
        rebuild[index] = true;
        // Neighbor faces can change too. Comparing chunk contents also handles
        // switching worlds or assigning a saved world to the same object.
        if (index % World::Width > 0) rebuild[index - 1] = true;
        if (index % World::Width + 1 < World::Width) rebuild[index + 1] = true;
        if (index >= World::Width) rebuild[index - World::Width] = true;
        if (index + World::Width < meshes_.size()) rebuild[index + World::Width] = true;
    }
    bool changed = false;
    for (std::size_t index = 0; index < meshes_.size(); ++index) {
        if (rebuild[index]) {
            meshes_[index].upload(buildMesh(world, index));
            changed = true;
        }
    }
    if (changed) meshedWorld_ = world;
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.48f, 0.70f, 0.90f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    shader_.use();
    const glm::mat4 viewProjection = camera.projectionMatrix(static_cast<float>(width) / height)
                                    * camera.viewMatrix();
    const Frustum frustum(viewProjection);
    shader_.setMat4("viewProjection", viewProjection);
    shader_.setVec3("cameraPosition", camera.position());
    visibleChunks_ = drawnTriangles_ = 0;
    for (std::size_t index = 0; index < meshes_.size(); ++index) {
        const glm::vec3 minimum(world.origin(index));
        if (meshes_[index].indexCount() == 0
            || !frustum.intersects(minimum, minimum + glm::vec3(Chunk::Size))) continue;
        meshes_[index].draw();
        ++visibleChunks_;
        drawnTriangles_ += meshes_[index].indexCount() / 3;
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

std::size_t WorldRenderer::triangleCount() const
{
    std::size_t total = 0;
    for (const auto& mesh : meshes_) total += mesh.indexCount() / 3;
    return total;
}

ObjectivesRenderer::ObjectivesRenderer() : shader_(objectiveVertexShader, objectiveFragmentShader)
{
    Chunk chunk;
    chunk.set(0, 0, 0, Block::Stone);
    GLint vao = 0, buffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    cube_.upload(buildMesh(chunk));
    glBindVertexArray(static_cast<GLuint>(vao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
}

void ObjectivesRenderer::draw(const Challenge& challenge, const Camera& camera,
                              int width, int height, float elapsedSeconds)
{
    if (width <= 0 || height <= 0) return;
    const float time = std::isfinite(elapsedSeconds) ? std::fmod(elapsedSeconds, 3600.0f) : 0.0f;
    const GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullFace = glIsEnabled(GL_CULL_FACE);
    const GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean depthMask = GL_TRUE;
    GLint program = 0, vao = 0, depthFunction = 0;
    std::array<GLint, 2> polygonMode{};
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
    glGetIntegerv(GL_POLYGON_MODE, polygonMode.data());
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader_.use();

    const glm::mat4 viewProjection = camera.projectionMatrix(static_cast<float>(width) / height)
                                    * camera.viewMatrix();
    const auto box = [&](glm::vec3 center, glm::vec3 size, glm::vec3 color, float spin = 0.0f) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
        model = glm::rotate(model, spin, glm::vec3(0, 1, 0));
        model = glm::scale(model, size);
        model = glm::translate(model, glm::vec3(-0.5f));
        shader_.setMat4("transform", viewProjection * model);
        shader_.setVec3("color", color);
        cube_.draw();
    };
    constexpr float beaconHeight = 10.0f;
    const std::array<glm::vec3, 3> colors{{{0.15f, 0.95f, 1.0f},
                                         {1.0f, 0.25f, 0.8f}, {1.0f, 0.9f, 0.15f}}};
    for (std::size_t index = 0; index < challenge.crystals().size(); ++index) {
        const auto& crystal = challenge.crystals()[index];
        if (crystal.collected) continue;
        // Thin opaque beacons are easy to spot above the hills. The cube stays
        // at interaction height; its animation does not move the pickup target.
        box(crystal.position + glm::vec3(0, beaconHeight * 0.5f, 0),
            {0.07f, beaconHeight, 0.07f}, colors[index]);
        const float bob = 0.12f * std::sin(time * 2.0f + static_cast<float>(index));
        box(crystal.position + glm::vec3(0, bob, 0), glm::vec3(0.65f), colors[index], time);
    }
    const glm::vec3 campColor = challenge.collectedCount() == 3
        ? glm::vec3(0.25f, 1.0f, 0.35f) : glm::vec3(1.0f, 0.55f, 0.12f);
    const auto camp = challenge.camp();
    box(camp - glm::vec3(0, 0.9f, 0), {1.8f, 0.2f, 1.8f}, campColor);
    box(camp + glm::vec3(0, beaconHeight * 0.5f - 1.0f, 0),
        {0.12f, beaconHeight, 0.12f}, campColor);
    box(camp + glm::vec3(0, beaconHeight - 1.0f, 0), glm::vec3(0.7f), campColor);

    glBindVertexArray(static_cast<GLuint>(vao));
    glUseProgram(static_cast<GLuint>(program));
    glDepthFunc(static_cast<GLenum>(depthFunction));
    glDepthMask(depthMask);
    if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
}

namespace {
constexpr const char* overlayVertexShader = R"glsl(#version 410 core
layout(location = 0) in vec3 position;
uniform mat4 transform;
void main() { gl_Position = transform * vec4(position, 1.0); }
)glsl";
constexpr const char* overlayFragmentShader = R"glsl(#version 410 core
uniform vec3 color;
out vec4 fragmentColor;
void main() { fragmentColor = vec4(color, 1.0); }
)glsl";
} // namespace

Overlay::Overlay() : shader_(overlayVertexShader, overlayFragmentShader)
{
    const std::array<glm::vec3, 48> vertices{{
        // Twelve edges of the selected block.
        {0,0,0}, {1,0,0}, {1,0,0}, {1,0,1}, {1,0,1}, {0,0,1}, {0,0,1}, {0,0,0},
        {0,1,0}, {1,1,0}, {1,1,0}, {1,1,1}, {1,1,1}, {0,1,1}, {0,1,1}, {0,1,0},
        {0,0,0}, {0,1,0}, {1,0,0}, {1,1,0}, {1,0,1}, {1,1,1}, {0,0,1}, {0,1,1},
        // Filled crosshair bars: dark border first, then the white center.
        {-9,-2,0}, {9,-2,0}, {9,2,0}, {-9,-2,0}, {9,2,0}, {-9,2,0},
        {-2,-9,0}, {2,-9,0}, {2,9,0}, {-2,-9,0}, {2,9,0}, {-2,9,0},
        {-8,-1,0}, {8,-1,0}, {8,1,0}, {-8,-1,0}, {8,1,0}, {-8,1,0},
        {-1,-8,0}, {1,-8,0}, {1,8,0}, {-1,-8,0}, {1,8,0}, {-1,8,0},
    }};
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &buffer_);
    if (!vao_ || !buffer_) {
        glDeleteBuffers(1, &buffer_);
        glDeleteVertexArrays(1, &vao_);
        throw std::runtime_error("Failed to allocate overlay resources");
    }
    GLint previousVao = 0;
    GLint previousBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousBuffer));
}

Overlay::~Overlay()
{
    glDeleteBuffers(1, &buffer_);
    glDeleteVertexArrays(1, &vao_);
}

void Overlay::draw(const std::optional<RayHit>& hit, const Camera& camera, int width, int height,
                   float contentScale)
{
    if (width <= 0 || height <= 0) return;
    if (!std::isfinite(contentScale) || contentScale <= 0.0f) contentScale = 1.0f;
    contentScale = std::clamp(contentScale, 0.5f, 4.0f);
    const GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullFace = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMask = GL_TRUE;
    std::array<GLint, 2> polygonMode{};
    GLint program = 0;
    GLint vao = 0;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_POLYGON_MODE, polygonMode.data());

    shader_.use();
    glBindVertexArray(vao_);
    glDepthMask(GL_FALSE);
    if (hit) {
        glEnable(GL_DEPTH_TEST);
        const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f),
            glm::vec3(hit->position) - glm::vec3(0.002f)), glm::vec3(1.004f));
        shader_.setMat4("transform", camera.projectionMatrix(static_cast<float>(width) / height)
                                   * camera.viewMatrix() * model);
        shader_.setVec3("color", {1.0f, 0.9f, 0.55f});
        glDrawArrays(GL_LINES, 0, 24);
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    // Triangle bars keep a readable thickness on high-DPI displays without
    // relying on wide OpenGL lines, which core-profile drivers may not support.
    shader_.setMat4("transform", glm::scale(glm::mat4(1.0f),
                                           {2.0f * contentScale / width,
                                            2.0f * contentScale / height, 1.0f}));
    shader_.setVec3("color", {0.0f, 0.0f, 0.0f});
    glDrawArrays(GL_TRIANGLES, 24, 12);
    shader_.setVec3("color", {1.0f, 1.0f, 1.0f});
    glDrawArrays(GL_TRIANGLES, 36, 12);
    // The overlay can be called between other passes without changing their
    // bindings or whether subsequent geometry reads/writes the depth buffer.
    glBindVertexArray(static_cast<GLuint>(vao));
    glUseProgram(static_cast<GLuint>(program));
    if (depthTest) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    glDepthMask(depthMask);
    if (cullFace) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
}

} // namespace voxel
