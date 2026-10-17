#include "graphics/renderer.h"
#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace voxel {

namespace {

constexpr const char* worldVertexShader = R"glsl(#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in float material;
uniform mat4 viewProjection;
out vec3 worldPosition;
out vec3 surfaceNormal;
out vec3 surfaceColor;
flat out int materialId;
void main() {
    worldPosition = position;
    surfaceNormal = normal;
    surfaceColor = color;
    materialId = int(material + 0.5);
    gl_Position = viewProjection * vec4(position, 1.0);
}
)glsl";

constexpr const char* worldFragmentShader = R"glsl(#version 410 core
in vec3 worldPosition;
in vec3 surfaceNormal;
in vec3 surfaceColor;
flat in int materialId;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform int underground;
uniform int lightCount;
uniform vec3 lightPositions[12];
uniform vec3 lightColors[12];
out vec4 fragmentColor;

float grain(vec3 cell) {
    return fract(sin(dot(cell, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

// World-space pixels repeat per voxel, including across greedily merged faces.
vec3 materialColor() {
    vec2 uv = abs(surfaceNormal.y) > 0.5 ? worldPosition.xz
        : (abs(surfaceNormal.x) > 0.5 ? worldPosition.zy : worldPosition.xy);
    vec2 pixel = floor(uv * 16.0);
    float noise = grain(vec3(pixel, float(materialId)));
    vec3 cell = floor(worldPosition - surfaceNormal * 0.001);
    vec3 color = surfaceColor * (0.89 + noise * 0.19) * (0.95 + 0.08 * grain(cell));
    if (materialId >= 7 && materialId <= 9) {
        float orePatch = grain(vec3(floor(uv * 4.0), float(materialId)));
        vec3 ore = materialId == 7 ? vec3(0.065, 0.07, 0.09)
            : (materialId == 8 ? vec3(0.82, 0.43, 0.20) : vec3(0.73, 0.70, 0.61));
        color = (orePatch > 0.66 && noise > 0.22 ? ore : vec3(0.39, 0.41, 0.44))
                * (0.85 + 0.24 * noise);
    } else if (materialId == 6 || materialId == 15) {
        color *= mod(pixel.y, 4.0) < 1.0 ? 0.77 : 1.0;
    } else if (materialId == 11) {
        // Timber has long bark grain; end faces show square growth rings.
        float ring = max(abs(fract(uv.x) - 0.5), abs(fract(uv.y) - 0.5));
        color *= abs(surfaceNormal.y) > 0.5 ? (mod(floor(ring * 24.0), 2.0) < 1.0 ? 0.7 : 1.0)
            : (0.65 + 0.40 * grain(vec3(floor(uv.x * 12.0), floor(uv.y), 1.0)));
    } else if (materialId == 12) {
        float row = floor(uv.y * 4.0);
        float joint = fract((uv.x + mod(row, 3.0) * 0.8) / 2.5);
        float fibers = grain(vec3(floor(uv.x * 2.0), floor(uv.y * 32.0), 12.0));
        color *= 0.83 + 0.24 * fibers;
        if (fract(uv.y * 4.0) < 0.065 || joint < 0.012) color *= 0.45;
    } else if (materialId == 13) {
        float row = floor(uv.y * 4.0);
        float joint = fract(uv.x * 2.0 + mod(row, 2.0) * 0.5);
        bool seam = fract(uv.y * 4.0) < 0.08 || joint < 0.04;
        if (seam) color = vec3(0.25, 0.24, 0.23);
    } else if (materialId == 14) {
        vec2 edge = min(fract(uv), 1.0 - fract(uv));
        color = min(edge.x, edge.y) < 0.12 ? vec3(0.29, 0.17, 0.07)
            : vec3(1.0, 0.72, 0.30) * (0.94 + 0.06 * noise);
    }
    return color;
}

void main() {
    vec3 normal = normalize(surfaceNormal);
    vec3 color = materialColor();
    vec3 toCamera = cameraPosition - worldPosition;
    float cameraDistance = length(toCamera);
    vec3 lit;
    vec3 fogColor;
    float fog;
    if (underground != 0) {
        vec3 direction = toCamera / max(cameraDistance, 0.001);
        float cone = smoothstep(0.2, 0.92, dot(-direction, cameraForward));
        float headlamp = (0.25 + 0.75 * max(dot(normal, direction), 0.0))
            * (0.45 + 0.55 * cone) / (1.0 + 0.025 * cameraDistance * cameraDistance);
        vec3 light = vec3(0.13, 0.16, 0.21) + vec3(0.94, 0.97, 1.0) * headlamp;
        vec3 pointLight = vec3(0.0);
        for (int i = 0; i < lightCount; ++i) {
            vec3 offset = lightPositions[i] - worldPosition;
            float distanceToLight = length(offset);
            float falloff = max(1.0 - distanceToLight / 11.0, 0.0);
            float facing = max(dot(normal, offset / max(distanceToLight, 0.001)), 0.0);
            pointLight += lightColors[i] * falloff * falloff * (0.25 + 1.35 * facing);
        }
        // Dense crystal veins should cast colored light without bleaching rock.
        float strongest = max(pointLight.r, max(pointLight.g, pointLight.b));
        light += pointLight / max(1.0, strongest);
        lit = color * light;
        fogColor = vec3(0.018, 0.026, 0.042);
        fog = smoothstep(25.0, 90.0, cameraDistance);
    } else {
        float sun = max(dot(normal, normalize(vec3(-0.6, 1.0, 0.3))), 0.0);
        lit = color * (0.45 + 0.55 * sun);
        fogColor = vec3(0.48, 0.70, 0.90);
        fog = smoothstep(55.0, 150.0, cameraDistance);
    }
    if (materialId == 14) lit = max(lit, color);
    if (materialId == 10) lit = max(lit, color * 0.75 + vec3(0.02, 0.12, 0.16));
    if (underground != 0) lit = pow(max(lit, vec3(0.0)), vec3(1.0 / 1.6));
    fragmentColor = vec4(mix(lit, fogColor, fog), 1.0);
}
)glsl";

} // namespace

WorldRenderer::WorldRenderer() : shader_(worldVertexShader, worldFragmentShader) {}

void WorldRenderer::render(const World& world, const Camera& camera, int width, int height,
                           bool wireframe, bool underground)
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
    if (changed) {
        lights_.clear();
        for (std::size_t index = 0; index < World::ChunkCount; ++index) {
            const auto origin = world.origin(index);
            const auto& chunk = world.chunk(index);
            for (int y = 0; y < Chunk::Size; ++y) {
                for (int z = 0; z < Chunk::Size; ++z) {
                    for (int x = 0; x < Chunk::Size; ++x) {
                        const auto block = chunk.get(x, y, z);
                        if (block != Block::Lantern && block != Block::Crystal) continue;
                        lights_.push_back({glm::vec3(origin + glm::ivec3(x, y, z)) + 0.5f,
                            block == Block::Lantern ? glm::vec3(1.0f, 0.62f, 0.24f)
                                                   : glm::vec3(0.24f, 0.68f, 1.0f)});
                    }
                }
            }
        }
        meshedWorld_ = world;
    }
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(underground ? 0.018f : 0.48f, underground ? 0.026f : 0.70f,
                 underground ? 0.042f : 0.90f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    shader_.use();
    const glm::mat4 viewProjection = camera.projectionMatrix(static_cast<float>(width) / height)
                                    * camera.viewMatrix();
    const Frustum frustum(viewProjection);
    shader_.setMat4("viewProjection", viewProjection);
    shader_.setVec3("cameraPosition", camera.position());
    shader_.setVec3("cameraForward", camera.forward());
    shader_.setInt("underground", underground);
    // Cache the light sources with the world, then select a small fixed number
    // near the camera. Walking never rescans blocks or rebuilds geometry.
    std::array<const Light*, 12> nearest{};
    std::array<float, 12> distances;
    distances.fill(std::numeric_limits<float>::max());
    if (underground) {
        for (const auto& light : lights_) {
            const auto delta = light.position - camera.position();
            const float distance = glm::dot(delta, delta);
            if (distance >= distances.back()) continue;
            std::size_t slot = nearest.size() - 1;
            while (slot > 0 && distance < distances[slot - 1]) {
                distances[slot] = distances[slot - 1];
                nearest[slot] = nearest[slot - 1];
                --slot;
            }
            distances[slot] = distance;
            nearest[slot] = &light;
        }
    }
    int lightCount = 0;
    for (const auto* light : nearest) {
        if (!light) break;
        const auto index = std::to_string(lightCount++);
        shader_.setVec3(("lightPositions[" + index + "]").c_str(), light->position);
        shader_.setVec3(("lightColors[" + index + "]").c_str(), light->color);
    }
    shader_.setInt("lightCount", lightCount);
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
