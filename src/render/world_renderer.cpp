#include "render/world_renderer.h"
#include "core/camera.h"

namespace voxel {
namespace {

constexpr const char* vertexShader = R"glsl(#version 410 core
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

constexpr const char* fragmentShader = R"glsl(#version 410 core
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

} // namespace

WorldRenderer::WorldRenderer() : shader_(vertexShader, fragmentShader) {}

void WorldRenderer::render(World& world, const Camera& camera, int width, int height, bool wireframe)
{
    if (width <= 0 || height <= 0) return;
    for (const auto index : world.takeDirtyChunks()) {
        meshes_[index].upload(world.mesh(index));
    }
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.48f, 0.70f, 0.90f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    shader_.use();
    shader_.setMat4("viewProjection", camera.projectionMatrix(static_cast<float>(width) / height)
                                    * camera.viewMatrix());
    shader_.setVec3("cameraPosition", camera.position());
    for (const auto& mesh : meshes_) mesh.draw();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

std::size_t WorldRenderer::triangleCount() const
{
    std::size_t total = 0;
    for (const auto& mesh : meshes_) total += mesh.indexCount() / 3;
    return total;
}

} // namespace voxel
