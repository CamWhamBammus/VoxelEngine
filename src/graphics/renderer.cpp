#include "graphics/renderer.h"
#include "camera.h"
#include "graphics/cave_details.h"
#include "graphics/block_relief.h"
#include "graphics/materials.h"
#include "graphics/water.h"
#include "graphics/voxel_lighting.h"
#include "hub_assets.h"
#include "block_shapes.h"
#include "sections.h"
#include "section_editor.h"
#include "graphics/asset_builder.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace voxel {

namespace {

GpuMesh* pickupMesh(std::array<GpuMesh,ItemCount>& cache, const HubAsset& asset)
{
    const auto stack = itemPickupStack(asset);
    if (!validItem(stack.item) || stack.count <= 0) return nullptr;
    auto& mesh = cache[static_cast<std::size_t>(stack.item)];
    if (!mesh.indexCount())
        mesh.upload(buildHubAsset({HubAssetKind::ItemPickup,{},0,0,{}, {stack.item,1}}));
    return &mesh;
}

constexpr const char* worldVertexShader = R"glsl(#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in float material;
layout(location = 4) in vec4 relief;
uniform mat4 viewProjection;
uniform mat4 model;
out vec3 worldPosition;
out vec3 surfaceNormal;
out vec3 surfaceColor;
out vec4 reliefSurface;
flat out int materialId;
void main() {
    worldPosition = (model * vec4(position, 1.0)).xyz;
    surfaceNormal = normalize(mat3(model) * normal);
    surfaceColor = color;
    reliefSurface = relief;
    materialId = int(material + 0.5);
    gl_Position = viewProjection * vec4(worldPosition, 1.0);
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
uniform int cameraInWater;
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

)glsl" + voxelLightingShader + R"glsl(

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
    // Opaque lakebeds and walls have no water face between them and a swimmer.
    // Shade those surfaces here; looking out through a water face is handled
    // by shadeWater(), so dry scenery beyond the surface is not tinted twice.
    if (cameraInWater != 0 && materialId != 16
        && waterCell(ivec3(floor(worldPosition + surfaceNormal * 0.04)))) {
        vec3 transmission = exp(-vec3(0.22, 0.075, 0.05) * min(cameraDistance, 36.0));
        fragmentColor.rgb = fragmentColor.rgb * transmission
            + vec3(0.012, 0.095, 0.105) * (1.0 - transmission);
        if (underground != 0) fragmentColor.a *= transmission.g;
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
                           std::span<const HubAsset> placedAssets, std::span<const glm::vec3> pickupOffsets)
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
    // Floating items are independent draw instances. Their movement, stack size
    // and collection must never invalidate terrain or the world-light textures.
    auto cached = placedAssets_.begin();
    bool changed = hubAssets != hubAssets_;
    for (const auto& asset : placedAssets) {
        if (isItemPickup(asset.kind)) continue;
        if (cached == placedAssets_.end() || *cached != asset) { changed = true; break; }
        ++cached;
    }
    changed |= cached != placedAssets_.end();
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
        ++sceneRebuildCount_;
        placedAssets_.clear();
        for (const auto& asset : placedAssets)
            if (!isItemPickup(asset.kind)) placedAssets_.push_back(asset);
        // Plans can depend on supports farther than an adjacent voxel, so a
        // changed world refreshes the entire small set of handcrafted models.
        // Expedition test rooms keep voxel roots and natural cave details but
        // do not inherit fittings from the village's fixed architectural plan.
        auto dressing = hubAssets ? buildHubDressing(world) : buildAssetDressing(placedAssets_);
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
        std::vector<bool> amberCovered(voxels.size());
        std::vector<bool> foliageCovered(voxels.size());
        std::vector<bool> mossCovered(voxels.size());
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
                        // Cutout leaf holes transmit light; collision is independent.
                        // Keep water's distinct code for shoreline/refraction queries.
                        voxels[local.x + size.x * (local.y + size.y * local.z)] = isStair(block) ? 200 + stairFacing(block)
                            : isOpaque(block) ? 255 : block == Block::Water ? 127 : 0;
                        if (block != Block::Lantern && block != Block::Crystal && block != Block::GlowLeaves
                            && block != Block::AmberCrystal && block != Block::GlowMoss) continue;
                        bool exposed = false;
                        for (const auto offset : {glm::ivec3(1,0,0), glm::ivec3(-1,0,0),
                             glm::ivec3(0,1,0), glm::ivec3(0,-1,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1)}) {
                            exposed |= !occludes(world.get(position + offset), block);
                        }
                        if (!exposed) continue;
                        if (block != Block::Lantern) {
                            // Dense crystals and glowing foliage share fixed
                            // sources at actual exposed blocks. Foliage uses a
                            // wider spacing so a canopy cannot flood the light list.
                            // This world-order grouping never follows the camera;
                            // every lantern still keeps its own light.
                            auto& covered = block == Block::Crystal ? crystalCovered
                                : block == Block::AmberCrystal ? amberCovered
                                : block == Block::GlowMoss ? mossCovered : foliageCovered;
                            const int spacing = block == Block::Crystal || block == Block::AmberCrystal ? 1 : 3;
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
                            : block == Block::AmberCrystal ? glm::vec3(1.05f, 0.48f, 0.10f)
                            : block == Block::GlowMoss ? glm::vec3(0.08f, 0.40f, 0.24f)
                            : position.y > 34 ? glm::vec3(0.38f, 0.14f, 0.29f)
                                              : glm::vec3(0.08f, 0.48f, 0.31f),
                            block == Block::Lantern ? 11.0f : block == Block::GlowMoss ? 5.5f
                            : block == Block::Crystal || block == Block::AmberCrystal ? 8.0f : 7.0f});
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
    }
    glViewport(0, 0, width, height);
    if (underground) postProcess_.begin(width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glCullFace(GL_BACK);
    const auto eye = camera.position();
    const bool cameraInWater = glm::all(glm::greaterThanEqual(eye, glm::vec3(World::Min)))
        && glm::all(glm::lessThan(eye, glm::vec3(World::MaxExclusive)))
        && world.get(glm::ivec3(glm::floor(eye))) == Block::Water;
    const auto background = cameraInWater ? glm::vec3(0.012f, 0.095f, 0.105f)
        : underground ? glm::vec3(0.025f, 0.026f, 0.065f) : glm::vec3(0.48f, 0.70f, 0.90f);
    glClearColor(background.r, background.g, background.b, underground ? 0.0f : 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    shader_.use();
    const glm::mat4 viewProjection = camera.projectionMatrix(static_cast<float>(width) / height)
                                    * camera.viewMatrix();
    const Frustum frustum(viewProjection);
    shader_.setMat4("viewProjection", viewProjection);
    shader_.setMat4("model", glm::mat4(1));
    shader_.setMat4("inverseViewProjection", glm::inverse(viewProjection));
    // Keep unused 2D samplers bound to a complete texture during the opaque
    // pass, including the first frame before a scene snapshot exists.
    shader_.setInt("opaqueColor", 1);
    shader_.setInt("opaqueDepth", 1);
    shader_.setInt("refractiveWater", 0);
    shader_.setVec3("cameraPosition", camera.position());
    shader_.setVec3("cameraForward", camera.forward());
    shader_.setInt("underground", underground);
    shader_.setInt("cameraInWater", cameraInWater);
    shader_.setInt("carriedLantern", carriedLantern);
    shader_.setInt("decorative", 0);
    shader_.setFloat("timeSeconds", std::isfinite(timeSeconds) ? timeSeconds : 0.0f);
    shader_.setInt("occupancy", 0);
    shader_.setInt("lightSources", 1);
    shader_.setVec3("worldMinimum", glm::vec3(World::Min));
    visibleChunks_ = drawnTriangles_ = pickupTriangles_ = 0;
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
    // Render before the opaque snapshot so water refraction and scene depth
    // include animated drops, just as they include walls and static props.
    if (underground && !hubAssets) {
        shader_.setInt("decorative", 1);
        for (std::size_t index = 0; index < placedAssets.size(); ++index) {
            const auto& asset = placedAssets[index];
            if (!isItemPickup(asset.kind) || !World::contains(glm::ivec3(glm::floor(asset.position)))) continue;
            const auto mesh = pickupMesh(pickups_,asset);
            if (!mesh) continue;
            pickupTriangles_ += mesh->indexCount()/3;
            const auto offset = index < pickupOffsets.size() ? pickupOffsets[index] : glm::vec3(0);
            const auto [minimum,maximum] = pickupVisualBounds(asset);
            if (!frustum.intersects(minimum+offset,maximum+offset)) continue;
            const auto cell = glm::clamp(glm::ivec3(glm::floor(asset.position+offset)),World::Min,World::MaxExclusive-1)-World::Min;
            const auto chunk = static_cast<std::size_t>(cell.x/Chunk::Size
                + World::Width*(cell.z/Chunk::Size+World::Width*(cell.y/Chunk::Size)));
            shader_.setInt("lightOffset", lightOffsets_[chunk]);
            shader_.setInt("lightCount", lightCounts_[chunk]);
            shader_.setMat4("model", glm::translate(glm::mat4(1),offset)*pickupTransform(asset,timeSeconds));
            mesh->draw();
            drawnTriangles_ += mesh->indexCount()/3;
        }
        shader_.setMat4("model", glm::mat4(1));
        shader_.setInt("decorative", 0);
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
        total += pickupTriangles_;
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

// The existing greedy mesher makes a recognizable wire preview without drawing
// every hidden cube. Very complex selections fall back to the exact volume grid.
MeshData clipboardPreviewMesh(const SectionClipboard& clipboard, int rotation)
{
    MeshData mesh;
    constexpr std::size_t vertexBudget = 180000;
    const auto sample = [&](glm::ivec3 point) {
        if (glm::any(glm::lessThan(point, glm::ivec3(0))) || glm::any(glm::greaterThanEqual(point, clipboard.size)))
            return Block::Air;
        const auto index = static_cast<std::size_t>(point.x + clipboard.size.x * (point.y + clipboard.size.y * point.z));
        return index < clipboard.blocks.size() ? clipboard.blocks[index] : Block::Air;
    };
    const auto append = [&](const MeshData& piece) {
        if (mesh.vertices.size() + piece.vertices.size() > vertexBudget) return false;
        const auto offset = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.insert(mesh.vertices.end(), piece.vertices.begin(), piece.vertices.end());
        for (const auto index : piece.indices) mesh.indices.push_back(offset + index);
        return true;
    };
    for (int z = 0; z < clipboard.size.z; z += Chunk::Size)
        for (int y = 0; y < clipboard.size.y; y += Chunk::Size)
            for (int x = 0; x < clipboard.size.x; x += Chunk::Size) {
                Chunk chunk;
                const glm::ivec3 origin(x,y,z);
                for (int k = 0; k < Chunk::Size; ++k) for (int j = 0; j < Chunk::Size; ++j)
                    for (int i = 0; i < Chunk::Size; ++i) chunk.set(i,j,k,sample(origin + glm::ivec3(i,j,k)));
                if (!append(buildMesh(chunk, origin, sample))) return {};
            }
    for (const auto& asset : clipboard.assets)
        if (!isItemPickup(asset.kind) && !append(buildHubAsset(asset))) return {};
    const auto turn = [&](glm::vec3 point, bool position) -> glm::vec3 {
        const auto size = position ? glm::vec3(clipboard.size) : glm::vec3(0);
        switch (rotation & 3) {
        case 1: return {point.z, point.y, size.x - point.x};
        case 2: return {size.x - point.x, point.y, size.z - point.z};
        case 3: return {size.z - point.z, point.y, point.x};
        default: return point;
        }
    };
    for (auto& vertex : mesh.vertices) {
        vertex.position = turn(vertex.position, true);
        vertex.normal = turn(vertex.normal, false);
    }
    return mesh;
}

MeshData markerLabel(std::string_view word)
{
    MeshData mesh;
    const HubAsset origin{HubAssetKind::LanternCap,{0,0,0}};
    AssetBuilder model(mesh, origin);
    constexpr float pixel = 0.045f;
    const float left = -(static_cast<float>(word.size()) * 6.0f - 1.0f) * pixel * 0.5f;
    for (std::size_t i = 0; i < word.size(); ++i) {
        std::array<unsigned,7> rows{};
        switch (word[i]) {
        case 'S': rows={15,16,16,14,1,1,30}; break;
        case 'T': rows={31,4,4,4,4,4,4}; break;
        case 'A': rows={14,17,17,31,17,17,17}; break;
        case 'R': rows={30,17,17,30,20,18,17}; break;
        case 'E': rows={31,16,16,30,16,16,31}; break;
        case 'N': rows={17,25,25,21,19,19,17}; break;
        case 'D': rows={30,17,17,17,17,17,30}; break;
        }
        for (int row=0; row<7; ++row) for (int column=0; column<5; ++column) {
            if ((rows[row] & (1u << (4-column))) == 0) continue;
            const glm::vec3 low(left + (i*6+column)*pixel, (6-row)*pixel, -0.01f);
            model.box(low, low+glm::vec3(pixel*0.9f,pixel*0.9f,0.02f), Block::Crystal, glm::vec3(1));
        }
    }
    return mesh;
}
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
    Chunk cube;
    cube.set(0,0,0,Block::Stone);
    markerCube_.upload(buildMesh(cube));
    startLabel_.upload(markerLabel("START"));
    endLabel_.upload(markerLabel("END"));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousBuffer));
}

Overlay::~Overlay()
{
    glDeleteBuffers(1, &buffer_);
    glDeleteVertexArrays(1, &vao_);
}

void Overlay::draw(const std::optional<RayHit>& hit, const Camera& camera, int width, int height,
                   float contentScale, Block material)
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
        shader_.setVec3("color", {1.0f, 0.9f, 0.55f});
        const auto shape = blockShape(isStair(material) ? material : Block::Stone);
        for (int i = 0; i < shape.count; ++i) {
            const auto& box = shape.boxes[i];
            const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f),
                glm::vec3(hit->position) + box.minimum - glm::vec3(.002f)),
                box.maximum - box.minimum + glm::vec3(.004f));
            shader_.setMat4("transform", camera.projectionMatrix(static_cast<float>(width) / height)
                                       * camera.viewMatrix() * model);
            glDrawArrays(GL_LINES, 0, 24);
        }
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
                           const std::optional<HubAsset>& selectedAsset, const Camera& camera, int width, int height,
                           float timeSeconds)
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
    const auto marker = [&](glm::vec3 position, glm::vec3 color, const GpuMesh* label) {
        box(position + glm::vec3(-0.8f, 0.035f, -0.8f), {1.6f,0.06f,1.6f}, color);
        if (!label) {
            box(position + glm::vec3(-0.55f, 0.04f, -0.55f), {1.1f,2.0f,1.1f}, color);
            return;
        }
        const auto beam = [&](glm::vec3 minimum, glm::vec3 extent, glm::vec3 tint) {
            shader_.setMat4("transform", projection * glm::scale(glm::translate(glm::mat4(1), position+minimum), extent));
            shader_.setVec3("color", tint);
            markerCube_.draw();
        };
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        for (float side : {-0.66f,0.54f}) {
            beam({side,0.04f,-0.1f}, {0.12f,1.96f,0.2f}, color*0.65f);
            beam({side+0.035f,0.15f,-0.115f}, {0.05f,1.72f,0.23f}, color);
        }
        beam({-0.66f,1.94f,-0.1f}, {1.32f,0.12f,0.2f}, color);
        const auto offset = camera.position() - position;
        const auto facing = glm::rotate(glm::mat4(1), std::atan2(offset.x,offset.z), glm::vec3(0,1,0));
        shader_.setMat4("transform", projection * glm::translate(glm::mat4(1),position+glm::vec3(0,2.18f,0)) * facing);
        shader_.setVec3("color", color);
        label->draw();
    };
    marker(section.spawn, {0.40f,1.0f,0.48f}, &startLabel_);
    if (section.checkpoint) marker(*section.checkpoint, {0.15f,0.95f,1.0f}, nullptr);
    marker(section.finish, {1.0f,0.70f,0.16f}, &endLabel_);
    const auto drop = [&](const HubAsset& asset, glm::vec3 color) {
        if (const auto mesh = pickupMesh(pickupMeshes_,asset)) {
            shader_.setMat4("transform",projection*pickupTransform(asset,timeSeconds));
            shader_.setVec3("color",color);
            glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            mesh->draw();
        }
    };
    if (selectedAsset && isItemPickup(selectedAsset->kind)) {
        drop(*selectedAsset,{1.0f,.85f,.45f});
    } else if (selectedAsset) {
        const auto mesh = buildHubAsset(*selectedAsset);
        if (!mesh.vertices.empty()) {
            auto low = mesh.vertices.front().position, high = low;
            for (const auto& vertex : mesh.vertices) {
                low = glm::min(low, vertex.position); high = glm::max(high, vertex.position);
            }
            box(low - 0.015f, high - low + 0.03f, {1.0f,0.85f,0.45f});
        }
    }
    if (preview && isItemPickup(preview->kind)) {
        drop(*preview,{.45f,1.0f,.65f});
    } else if (preview) {
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

void Overlay::drawBuildTools(const SectionEditor& editor, const Camera& camera, int width, int height,
                             float timeSeconds)
{
    if (width <= 0 || height <= 0) return;
    const auto selection = editor.buildSelection();
    const auto& preview = editor.toolPreview();
    if (!selection && !preview && !editor.firstCorner() && !editor.secondCorner()) return;
    const GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST), cullFace = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMask = GL_TRUE;
    GLint program = 0, vao = 0, buffer = 0;
    std::array<GLint,2> polygon{};
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_POLYGON_MODE, polygon.data());
    glDisable(GL_DEPTH_TEST);
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
    const glm::vec3 cyan(0.25f,0.83f,1.0f), green(0.35f,1.0f,0.51f), gold(1.0f,0.78f,0.23f);
    if (selection) box(glm::vec3(selection->min)-0.01f, glm::vec3(selection->max-selection->min+1)+0.02f, cyan);
    if (editor.firstCorner()) box(glm::vec3(*editor.firstCorner())-0.025f, glm::vec3(1.05f), green);
    if (editor.secondCorner()) box(glm::vec3(*editor.secondCorner())-0.035f, glm::vec3(1.07f), gold);
    if (preview) {
        const auto color = editor.toolPreviewValid() ? green : glm::vec3(1.0f,0.28f,0.21f);
        const auto origin = glm::vec3(preview->min);
        const auto size = glm::vec3(preview->max-preview->min+1);
        const bool fillGaps = editor.buildMode() == SectionBuildMode::Paste && editor.pasteFillGaps();
        if (editor.buildMode() == SectionBuildMode::Paste && !editor.clipboard().empty()) {
            shader_.setMat4("transform", projection * glm::translate(glm::mat4(1),origin));
            glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            if (fillGaps) {
                // Match the controller's exact plan: existing terrain, copied air
                // and decorative models must never look like new additions.
                if (pasteChangesRevision_ != editor.pastePreviewRevision()) {
                    SectionClipboard additions;
                    additions.size = preview->max-preview->min+1;
                    additions.blocks.resize(static_cast<std::size_t>(additions.size.x)*additions.size.y*additions.size.z,Block::Air);
                    for (const auto& change : editor.previewBlockChanges()) {
                        const auto local = change.position-preview->min;
                        if (glm::any(glm::lessThan(local,glm::ivec3(0))) || glm::any(glm::greaterThanEqual(local,additions.size))) continue;
                        const auto index = static_cast<std::size_t>(local.x+additions.size.x*(local.y+additions.size.y*local.z));
                        additions.blocks[index] = change.after;
                    }
                    pasteChangesMesh_.upload(clipboardPreviewMesh(additions,0));
                    pasteChangesRevision_ = editor.pastePreviewRevision();
                }
                shader_.setVec3("color",color*0.88f);
                pasteChangesMesh_.draw();
            } else {
                if (clipboardRevision_ != editor.clipboardRevision() || clipboardRotation_ != editor.pasteRotation()) {
                    clipboardMesh_.upload(clipboardPreviewMesh(editor.clipboard(), editor.pasteRotation()));
                    clipboardRevision_ = editor.clipboardRevision();
                    clipboardRotation_ = editor.pasteRotation();
                }
                shader_.setVec3("color",color*0.66f);
                clipboardMesh_.draw();
                for (auto asset : editor.clipboard().assets) {
                    if (!isItemPickup(asset.kind)) continue;
                    const auto& clipboard = editor.clipboard();
                    const auto point = asset.position;
                    switch (editor.pasteRotation() & 3) {
                    case 1: asset.position={point.z,point.y,clipboard.size.x-point.x}; break;
                    case 2: asset.position={clipboard.size.x-point.x,point.y,clipboard.size.z-point.z}; break;
                    case 3: asset.position={clipboard.size.z-point.z,point.y,point.x}; break;
                    default: break;
                    }
                    asset.position += origin;
                    asset.facing = (asset.facing+editor.pasteRotation())&3;
                    if (const auto mesh = pickupMesh(pickupMeshes_,asset)) {
                        shader_.setMat4("transform",projection*pickupTransform(asset,timeSeconds));
                        mesh->draw();
                    }
                }
            }
        }
        const auto boundsColor = fillGaps && editor.toolPreviewValid() ? cyan*0.30f : color;
        box(origin - 0.018f, size + 0.036f, boundsColor);
        // Up to sixteen grid divisions per axis keeps large copies readable.
        const int spacing = std::max(1,static_cast<int>(std::ceil(std::max(size.x,size.z)/16.0f)));
        for (int x = spacing; x < size.x; x += spacing)
            box(origin+glm::vec3(x,0.025f,0),{0,0,size.z},boundsColor*0.57f);
        for (int z = spacing; z < size.z; z += spacing)
            box(origin+glm::vec3(0,0.025f,z),{size.x,0,0},boundsColor*0.57f);
        box(origin-glm::vec3(0.045f),glm::vec3(0.09f),glm::vec3(1));
        box(origin,{std::min(2.0f,size.x),0,0},{1.0f,0.36f,0.28f});
        box(origin,{0,std::min(2.0f,size.y),0},{0.40f,1.0f,0.48f});
        box(origin,{0,0,std::min(2.0f,size.z)},{0.36f,0.65f,1.0f});
    }
    glBindVertexArray(static_cast<GLuint>(vao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
    glUseProgram(static_cast<GLuint>(program));
    if (depthTest) glEnable(GL_DEPTH_TEST);
    if (cullFace) glEnable(GL_CULL_FACE);
    glDepthMask(depthMask);
    glPolygonMode(GL_FRONT_AND_BACK, polygon[0]);
}

} // namespace voxel
