#include "graphics/renderer.h"
#include "camera.h"
#include "graphics/cave_details.h"
#include "graphics/block_relief.h"
#include "graphics/materials.h"
#include "graphics/water.h"
#include "hub_assets.h"
#include "sections.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace voxel {

namespace {

constexpr const char* worldVertexShader = R"glsl(#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in float material;
layout(location = 4) in vec4 relief;
uniform mat4 viewProjection;
out vec3 worldPosition;
out vec3 surfaceNormal;
out vec3 surfaceColor;
out vec4 reliefSurface;
flat out int materialId;
void main() {
    worldPosition = position;
    surfaceNormal = normal;
    surfaceColor = color;
    reliefSurface = relief;
    materialId = int(material + 0.5);
    gl_Position = viewProjection * vec4(position, 1.0);
}
)glsl";

const std::string worldFragmentShader = std::string(R"glsl(#version 410 core
in vec3 worldPosition;
in vec3 surfaceNormal;
in vec3 surfaceColor;
in vec4 reliefSurface;
flat in int materialId;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform int underground;
uniform int decorative;
uniform float timeSeconds;
uniform int carriedLantern;
uniform int lightCount;
uniform int lightOffset;
uniform sampler2D lightSources;
uniform sampler3D occupancy;
uniform vec3 worldMinimum;
out vec4 fragmentColor;

)glsl") + materialShader + R"glsl(

vec4 lightData(int index, int part) {
    int address = (lightOffset + index) * 2 + part;
    int width = textureSize(lightSources, 0).x;
    return texelFetch(lightSources, ivec2(address % width, address / width), 0);
}

bool solid(ivec3 cell) {
    ivec3 local = cell - ivec3(worldMinimum);
    if (any(lessThan(local, ivec3(0))) || any(greaterThanEqual(local, textureSize(occupancy, 0)))) return false;
    return texelFetch(occupancy, local, 0).r > 0.5;
}

float contactShade() {
    vec3 normal = decorative == 2 ? reliefSurface.xyz : normalize(surfaceNormal);
    vec3 anchor = decorative == 2 ? worldPosition - normal * reliefSurface.w : worldPosition;
    ivec3 outside = ivec3(floor(anchor + normal * 0.02));
    ivec3 u = abs(normal.x) > 0.5 ? ivec3(0, 0, 1) : ivec3(1, 0, 0);
    ivec3 v = abs(normal.y) > 0.5 ? ivec3(0, 0, 1) : ivec3(0, 1, 0);
    vec2 position = fract(vec2(dot(anchor, vec3(u)), dot(anchor, vec3(v))));
    vec2 low = 1.0 - smoothstep(vec2(0.0), vec2(0.65), position);
    vec2 high = 1.0 - smoothstep(vec2(0.0), vec2(0.65), 1.0 - position);
    float shade = float(solid(outside - u)) * low.x + float(solid(outside + u)) * high.x
        + float(solid(outside - v)) * low.y + float(solid(outside + v)) * high.y;
    float corners = float(solid(outside - u - v)) * low.x * low.y
        + float(solid(outside - u + v)) * low.x * high.y
        + float(solid(outside + u - v)) * high.x * low.y
        + float(solid(outside + u + v)) * high.x * high.y;
    return max(0.42, 1.0 - shade * 0.24 - corners * 0.16);
}

// Walk crossed voxels, not fixed-distance samples: even a one-block wall casts
// a reliable shadow. The final cell is the emitting block and is not a blocker.
float visibleLight(vec3 start, vec3 end) {
    vec3 ray = end - start;
    ivec3 cell = ivec3(floor(start));
    ivec3 target = ivec3(floor(end));
    ivec3 advance = ivec3(sign(ray));
    vec3 delta = 1.0 / max(abs(ray), vec3(0.000001));
    vec3 next = mix(start - vec3(cell), vec3(cell + 1) - start, greaterThan(ray, vec3(0.0))) * delta;
    next = mix(vec3(1e20), next, greaterThan(abs(ray), vec3(0.000001)));
    // Sources reach at most eleven blocks: 32 crossings cover every direction.
    for (int step = 0; step < 32; ++step) {
        if (all(equal(cell, target))) return 1.0;
        if (solid(cell)) return 0.0;
        if (next.x < next.y && next.x < next.z) { cell.x += advance.x; next.x += delta.x; }
        else if (next.y < next.z) { cell.y += advance.y; next.y += delta.y; }
        else { cell.z += advance.z; next.z += delta.z; }
    }
    return 0.0;
}

float lightVisibility(vec3 start, vec3 center, vec3 direction, float influence) {
    float centerVisibility = visibleLight(start, center);
    // Fade in area samples for influential lights without a hard rank cutoff.
    // All endpoints stay inside the emitting voxel, so fully blocked walls
    // remain dark while nearby shadow edges gain a narrow penumbra.
    float softness = 0.3 * smoothstep(0.035, 0.08, influence);
    if (softness == 0.0) return centerVisibility;
    vec3 axis = abs(direction.y) < 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(direction, axis));
    vec3 offset = tangent * 0.22 + cross(direction, tangent) * 0.12;
    return centerVisibility * (1.0 - 2.0 * softness) + softness * visibleLight(start, center + offset)
         + softness * visibleLight(start, center - offset);
}

)glsl" + waterShader + R"glsl(

void main() {
    MaterialSurface surface = evaluateMaterial();
    vec3 normal = normalize(surface.normal);
    vec3 color = surface.albedo;
    vec3 toCamera = cameraPosition - worldPosition;
    float cameraDistance = length(toCamera);
    vec3 viewDirection = toCamera / max(cameraDistance, 0.001);
    vec3 lit;
    vec3 surfaceHighlight = vec3(0.0);
    vec3 fogColor;
    float fog;
    if (underground != 0) {
        float ao = decorative == 1 || materialId == 16 ? 1.0 : contactShade();
        float cone = smoothstep(0.35, 0.94, dot(-viewDirection, cameraForward));
        float headlamp = 0.24 * (0.2 + 0.8 * max(dot(normal, viewDirection), 0.0))
            * (0.25 + 0.75 * cone) / (1.0 + 0.035 * cameraDistance * cameraDistance);
        vec3 ambient = mix(vec3(0.05, 0.035, 0.065), vec3(0.045, 0.09, 0.085),
                           surfaceNormal.y * 0.5 + 0.5);
        float lantern = carriedLantern != 0 ? 0.65 * (0.35 + 0.65 * max(dot(normal, viewDirection), 0.0))
            / (1.0 + 0.045 * cameraDistance * cameraDistance) : 0.0;
        vec3 light = (ambient + vec3(0.82, 0.91, 1.0) * headlamp
                      + vec3(1.0, 0.60, 0.24) * lantern) * ao;
        vec3 pointLight = vec3(0.0);
        vec3 highlight = vec3(0.0);
        vec3 start = worldPosition + surfaceNormal * 0.03;
        // Every source that reaches this surface contributes. A nearest-light
        // cap would cause visible boundaries wherever the source ranking swaps,
        // even if the ranking were based on world position instead of the camera.
        for (int i = 0; i < lightCount; ++i) {
            vec4 source = lightData(i, 0);
            vec3 offset = source.xyz - worldPosition;
            float distanceSquared = dot(offset, offset);
            if (distanceSquared >= source.w * source.w || dot(surfaceNormal, offset) <= 0.0) continue;
            vec3 lightColor = lightData(i, 1).rgb;
            float distanceToLight = sqrt(distanceSquared);
            float falloff = max(1.0 - distanceToLight / source.w, 0.0);
            vec3 direction = offset / max(distanceToLight, 0.001);
            float facing = max(dot(normal, direction), 0.0);
            if (facing <= 0.0) continue;
            float strength = falloff * falloff
                * lightVisibility(start, source.xyz, direction, falloff * falloff * facing);
            if (strength <= 0.0) continue;
            pointLight += lightColor * strength * 1.65 * facing;
            vec3 halfDirection = direction + viewDirection;
            halfDirection /= max(length(halfDirection), 0.001);
            float shine = pow(max(dot(normal, halfDirection), 0.0), mix(64.0, 8.0, surface.roughness));
            highlight += lightColor * strength * shine * (1.0 - surface.roughness) * (materialId == 16 ? 0.65 : 0.14);
        }
        float strongest = max(pointLight.r, max(pointLight.g, pointLight.b));
        light += pointLight / max(1.0, strongest) * mix(0.65, 1.0, ao);
        lit = color * light + highlight;
        surfaceHighlight = highlight;
        fogColor = mix(vec3(0.012, 0.035, 0.043), vec3(0.025, 0.027, 0.044),
                       smoothstep(8.0, 46.0, worldPosition.y));
        fog = smoothstep(35.0, 135.0, cameraDistance) * 0.55;
    } else {
        float sun = max(dot(normal, normalize(vec3(-0.6, 1.0, 0.3))), 0.0);
        lit = color * (0.45 + 0.55 * sun);
        fogColor = vec3(0.48, 0.70, 0.90);
        fog = smoothstep(55.0, 150.0, cameraDistance);
    }
    if (materialId == 16)
        lit = shadeWater(lit, normal, viewDirection) + surfaceHighlight * 0.75;
    if (underground != 0) {
        // HDR composition owns exposure and gamma. Alpha marks luminous
        // surfaces so bloom cannot turn every bright stone surface into haze.
        lit += color * surface.emission * (materialId == 14 ? 2.8 : 2.2);
        fragmentColor = vec4(mix(max(lit, vec3(0.0)), fogColor, fog),
                             clamp(surface.emission, 0.0, 1.0) * (1.0 - fog));
    } else {
        lit = max(lit, color * surface.emission * (materialId == 10 ? 2.0 : 1.0));
        if (materialId == 10) lit = min(lit, color);
        fragmentColor = vec4(mix(lit, fogColor, fog), 1.0);
    }
}
)glsl";

} // namespace

WorldRenderer::WorldRenderer() : shader_(worldVertexShader, worldFragmentShader.c_str())
{
    glGenTextures(1, &occupancy_);
    glGenTextures(1, &lightTexture_);
    if (!occupancy_ || !lightTexture_) {
        glDeleteTextures(1, &occupancy_);
        glDeleteTextures(1, &lightTexture_);
        throw std::runtime_error("Failed to allocate voxel lighting textures");
    }
}

WorldRenderer::~WorldRenderer()
{
    glDeleteTextures(1, &occupancy_);
    glDeleteTextures(1, &lightTexture_);
}

void WorldRenderer::uploadLights(const World& world)
{
    std::vector<glm::vec4> data;
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        // Models can reach beyond their owning chunk. Include every source
        // touching their full bounds; never truncate a chunk's light list.
        const float margin = assets_[index].indexCount() ? HubAssetMargin : 1.0f;
        const glm::vec3 minimum = glm::vec3(world.origin(index)) - margin;
        const glm::vec3 maximum = minimum + float(Chunk::Size) + 2.0f * margin;
        lightOffsets_[index] = static_cast<int>(data.size() / 2);
        for (const auto& light : lights_) {
            const auto delta = light.position - glm::clamp(light.position, minimum, maximum);
            if (glm::dot(delta, delta) >= light.radius * light.radius) continue;
            data.emplace_back(light.position, light.radius);
            data.emplace_back(light.color, 0.0f);
        }
        lightCounts_[index] = static_cast<int>(data.size() / 2) - lightOffsets_[index];
    }
    // A packed image avoids a fixed shader uniform limit when players place
    // more lights. Two texels hold one candidate; rows only affect storage.
    GLint maximumSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumSize);
    const int width = std::min(1024, maximumSize);
    const int height = std::max(1, static_cast<int>((data.size() + width - 1) / width));
    if (height > maximumSize) throw std::runtime_error("Too many voxel light candidates for the GPU");
    data.resize(static_cast<std::size_t>(width) * height, glm::vec4(0));
    glActiveTexture(GL_TEXTURE1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);
}

void WorldRenderer::render(const World& world, const Camera& camera, int width, int height,
                           bool wireframe, bool underground, float timeSeconds, bool carriedLantern, bool hubAssets,
                           std::span<const HubAsset> placedAssets)
{
    if (width <= 0 || height <= 0) return;
    underground_ = underground;
    std::array<bool, World::ChunkCount> rebuild{};
    for (std::size_t index = 0; index < meshes_.size(); ++index) {
        if (meshedWorld_ && world.seed() == meshedWorld_->seed()
            && world.chunk(index) == meshedWorld_->chunk(index)) continue;
        rebuild[index] = true;
        // Neighbor faces can change too. Comparing chunk contents also handles
        // switching worlds or assigning a saved world to the same object.
        constexpr std::size_t layer = World::Width * World::Width;
        const auto x = index % World::Width, z = index / World::Width % World::Width;
        if (x > 0) rebuild[index - 1] = true;
        if (x + 1 < World::Width) rebuild[index + 1] = true;
        if (z > 0) rebuild[index - World::Width] = true;
        if (z + 1 < World::Width) rebuild[index + World::Width] = true;
        if (index >= layer) rebuild[index - layer] = true;
        if (index + layer < meshes_.size()) rebuild[index + layer] = true;
    }
    bool changed = hubAssets != hubAssets_
        || !std::equal(placedAssets.begin(), placedAssets.end(), placedAssets_.begin(), placedAssets_.end());
    for (std::size_t index = 0; index < meshes_.size(); ++index) {
        if (rebuild[index]) {
            auto opaque = buildMesh(world, index);
            const auto water = splitWaterMesh(opaque);
            meshes_[index].upload(opaque);
            water_[index].upload(water);
            details_[index].upload(buildCaveDetails(world, index));
            relief_[index].upload(buildBlockRelief(world, index));
            changed = true;
        }
    }
    std::vector<HubAssetLight> assetLights;
    if (changed) {
        // Plans can depend on supports farther than an adjacent voxel, so a
        // changed world refreshes the entire small set of handcrafted models.
        // Expedition test rooms keep voxel roots and natural cave details but
        // do not inherit fittings from the village's fixed architectural plan.
        auto dressing = hubAssets ? buildHubDressing(world) : buildAssetDressing(placedAssets);
        for (std::size_t index = 0; index < assets_.size(); ++index) {
            assets_[index].upload(dressing.meshes[index]);
        }
        assetLights = std::move(dressing.lights);
    }
    GLint activeTexture = 0, previousTexture = 0, previousLights = 0;
    std::array<GLint, 2> previousSceneTextures{};
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    for (int unit = 2; unit <= 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousSceneTextures[unit - 2]);
    }
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousLights);
    glBindTexture(GL_TEXTURE_2D, lightTexture_);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &previousTexture);
    glBindTexture(GL_TEXTURE_3D, occupancy_);
    if (changed) {
        lights_.clear();
        constexpr auto size = World::MaxExclusive - World::Min;
        std::vector<unsigned char> voxels(size.x * size.y * size.z);
        std::vector<bool> crystalCovered(voxels.size());
        std::vector<bool> foliageCovered(voxels.size());
        for (std::size_t index = 0; index < World::ChunkCount; ++index) {
            const auto origin = world.origin(index);
            const auto& chunk = world.chunk(index);
            for (int y = 0; y < Chunk::Size; ++y) {
                for (int z = 0; z < Chunk::Size; ++z) {
                    for (int x = 0; x < Chunk::Size; ++x) {
                        const auto block = chunk.get(x, y, z);
                        const auto position = origin + glm::ivec3(x, y, z);
                        const auto local = position - World::Min;
                        // OpenGL 3D images run x, then y, then z (unlike Chunk).
                        voxels[local.x + size.x * (local.y + size.y * local.z)] = isSolid(block) ? 255 : block == Block::Water ? 127 : 0;
                        if (block != Block::Lantern && block != Block::Crystal && block != Block::GlowLeaves) continue;
                        bool exposed = false;
                        for (const auto offset : {glm::ivec3(1,0,0), glm::ivec3(-1,0,0),
                             glm::ivec3(0,1,0), glm::ivec3(0,-1,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1)}) {
                            exposed |= !isSolid(world.get(position + offset));
                        }
                        if (!exposed) continue;
                        if (block == Block::Crystal || block == Block::GlowLeaves) {
                            // Dense crystals and glowing foliage share fixed
                            // sources at actual exposed blocks. Foliage uses a
                            // wider spacing so a canopy cannot flood the light list.
                            // This world-order grouping never follows the camera;
                            // every lantern still keeps its own light.
                            auto& covered = block == Block::Crystal ? crystalCovered : foliageCovered;
                            const int spacing = block == Block::Crystal ? 1 : 3;
                            if (covered[local.x + size.x * (local.y + size.y * local.z)]) continue;
                            for (int dz = -spacing; dz <= spacing; ++dz) {
                                for (int dy = -spacing; dy <= spacing; ++dy) {
                                    for (int dx = -spacing; dx <= spacing; ++dx) {
                                        const auto neighbor = local + glm::ivec3(dx, dy, dz);
                                        if (World::contains(position + glm::ivec3(dx, dy, dz))) {
                                            covered[neighbor.x + size.x * (neighbor.y + size.y * neighbor.z)] = true;
                                        }
                                    }
                                }
                            }
                        }
                        lights_.push_back({glm::vec3(position) + 0.5f,
                            block == Block::Lantern ? glm::vec3(1.15f, 0.58f, 0.18f)
                            : block == Block::Crystal ? glm::vec3(0.10f, 0.85f, 1.15f)
                            : position.y > 34 ? glm::vec3(0.38f, 0.14f, 0.29f)
                                              : glm::vec3(0.08f, 0.48f, 0.31f),
                            block == Block::Lantern ? 11.0f : block == Block::Crystal ? 8.0f : 7.0f});
                    }
                }
            }
        }
        GLint unpackAlignment = 0;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, size.x, size.y, size.z, 0,
                     GL_RED, GL_UNSIGNED_BYTE, voxels.data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
        for (const auto& light : assetLights) {
            lights_.push_back({light.position, light.color, light.radius});
        }
        uploadLights(world);
        meshedWorld_ = world;
        hubAssets_ = hubAssets;
        placedAssets_.assign(placedAssets.begin(), placedAssets.end());
    }
    glViewport(0, 0, width, height);
    if (underground) postProcess_.begin(width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glCullFace(GL_BACK);
    glClearColor(underground ? 0.025f : 0.48f, underground ? 0.026f : 0.70f,
                 underground ? 0.065f : 0.90f, underground ? 0.0f : 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    shader_.use();
    const glm::mat4 viewProjection = camera.projectionMatrix(static_cast<float>(width) / height)
                                    * camera.viewMatrix();
    const Frustum frustum(viewProjection);
    shader_.setMat4("viewProjection", viewProjection);
    shader_.setMat4("inverseViewProjection", glm::inverse(viewProjection));
    // Keep unused 2D samplers bound to a complete texture during the opaque
    // pass, including the first frame before a scene snapshot exists.
    shader_.setInt("opaqueColor", 1);
    shader_.setInt("opaqueDepth", 1);
    shader_.setInt("refractiveWater", 0);
    shader_.setVec3("cameraPosition", camera.position());
    shader_.setVec3("cameraForward", camera.forward());
    shader_.setInt("underground", underground);
    shader_.setInt("carriedLantern", carriedLantern);
    shader_.setInt("decorative", 0);
    shader_.setFloat("timeSeconds", std::isfinite(timeSeconds) ? timeSeconds : 0.0f);
    shader_.setInt("occupancy", 0);
    shader_.setInt("lightSources", 1);
    shader_.setVec3("worldMinimum", glm::vec3(World::Min));
    visibleChunks_ = drawnTriangles_ = 0;
    std::vector<std::size_t> visibleWater;
    for (std::size_t index = 0; index < meshes_.size(); ++index) {
        const glm::vec3 minimum(world.origin(index));
        const bool hasDetails = details_[index].indexCount() != 0 || relief_[index].indexCount() != 0;
        const bool hasAssets = assets_[index].indexCount() != 0;
        const float margin = underground ? (hasAssets ? HubAssetMargin : hasDetails ? 1.0f : 0.0f) : 0.0f;
        if ((meshes_[index].indexCount() == 0 && water_[index].indexCount() == 0
             && !(underground && (hasDetails || hasAssets)))
            || !frustum.intersects(minimum - margin, minimum + glm::vec3(Chunk::Size + margin))) continue;
        if (water_[index].indexCount() != 0) visibleWater.push_back(index);
        shader_.setInt("lightOffset", lightOffsets_[index]);
        shader_.setInt("lightCount", underground ? lightCounts_[index] : 0);
        // Draw raised faces first so their depth can hide the block paint below.
        if (underground && relief_[index].indexCount() != 0) {
            shader_.setInt("decorative", 2);
            relief_[index].draw();
            drawnTriangles_ += relief_[index].indexCount() / 3;
            shader_.setInt("decorative", 0);
        }
        meshes_[index].draw();
        ++visibleChunks_;
        drawnTriangles_ += meshes_[index].indexCount() / 3;
        if (underground && (details_[index].indexCount() != 0 || hasAssets)) {
            shader_.setInt("decorative", 1);
            details_[index].draw();
            drawnTriangles_ += details_[index].indexCount() / 3;
            assets_[index].draw();
            drawnTriangles_ += assets_[index].indexCount() / 3;
            shader_.setInt("decorative", 0);
        }
    }
    if (!visibleWater.empty()) {
        if (underground && !wireframe) {
            postProcess_.captureScene();
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, postProcess_.sceneColor());
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, postProcess_.sceneDepth());
            shader_.setInt("opaqueColor", 2);
            shader_.setInt("opaqueDepth", 3);
            shader_.setInt("refractiveWater", 1);
        }
        // Water writes depth just like terrain; the opaque snapshot supplies
        // transmission without alpha-sorting adjacent chunks or waterfalls.
        glDisable(GL_CULL_FACE);
        for (const auto index : visibleWater) {
            shader_.setInt("lightOffset", lightOffsets_[index]);
            shader_.setInt("lightCount", underground ? lightCounts_[index] : 0);
            water_[index].draw();
            drawnTriangles_ += water_[index].indexCount() / 3;
        }
        glEnable(GL_CULL_FACE);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (underground) postProcess_.finish();
    for (int unit = 2; unit <= 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousSceneTextures[unit - 2]));
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, static_cast<GLuint>(previousTexture));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousLights));
    glActiveTexture(static_cast<GLenum>(activeTexture));
}

std::size_t WorldRenderer::triangleCount() const
{
    std::size_t total = 0;
    for (const auto& mesh : meshes_) total += mesh.indexCount() / 3;
    for (const auto& mesh : water_) total += mesh.indexCount() / 3;
    if (underground_) {
        for (const auto& mesh : details_) total += mesh.indexCount() / 3;
        for (const auto& mesh : relief_) total += mesh.indexCount() / 3;
        for (const auto& mesh : assets_) total += mesh.indexCount() / 3;
    }
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

void Overlay::drawSection(const SectionMetadata& section, const std::optional<HubAsset>& preview,
                           const std::optional<HubAsset>& selectedAsset, const Camera& camera, int width, int height)
{
    if (width <= 0 || height <= 0) return;
    const GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST), cullFace = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMask = GL_TRUE;
    GLint program = 0, vao = 0, buffer = 0;
    std::array<GLint,2> polygon{};
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_POLYGON_MODE, polygon.data());
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    shader_.use();
    const auto projection = camera.projectionMatrix(float(width) / height) * camera.viewMatrix();
    const auto box = [&](glm::vec3 minimum, glm::vec3 extent, glm::vec3 color) {
        glBindVertexArray(vao_);
        shader_.setMat4("transform", projection * glm::scale(glm::translate(glm::mat4(1), minimum), extent));
        shader_.setVec3("color", color);
        glDrawArrays(GL_LINES, 0, 24);
    };
    const auto marker = [&](glm::vec3 position, glm::vec3 color) {
        box(position + glm::vec3(-0.8f, 0.035f, -0.8f), {1.6f,0.06f,1.6f}, color);
        box(position + glm::vec3(-0.55f, 0.04f, -0.55f), {1.1f,2.0f,1.1f}, color);
    };
    marker(section.spawn, {0.40f,1.0f,0.48f});
    if (section.checkpoint) marker(*section.checkpoint, {0.15f,0.95f,1.0f});
    marker(section.finish, {1.0f,0.70f,0.16f});
    if (selectedAsset) {
        const auto mesh = buildHubAsset(*selectedAsset);
        if (!mesh.vertices.empty()) {
            auto low = mesh.vertices.front().position, high = low;
            for (const auto& vertex : mesh.vertices) {
                low = glm::min(low, vertex.position); high = glm::max(high, vertex.position);
            }
            box(low - 0.015f, high - low + 0.03f, {1.0f,0.85f,0.45f});
        }
    }
    if (preview) {
        if (previewAsset_ != preview) {
            previewMesh_.upload(buildHubAsset(*preview));
            previewAsset_ = preview;
        }
        shader_.setMat4("transform", projection);
        shader_.setVec3("color", {0.45f,1.0f,0.65f});
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        previewMesh_.draw();
    }
    glBindVertexArray(static_cast<GLuint>(vao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
    glUseProgram(static_cast<GLuint>(program));
    if (!depthTest) glDisable(GL_DEPTH_TEST);
    if (cullFace) glEnable(GL_CULL_FACE);
    glDepthMask(depthMask);
    glPolygonMode(GL_FRONT_AND_BACK, polygon[0]);
}

} // namespace voxel
