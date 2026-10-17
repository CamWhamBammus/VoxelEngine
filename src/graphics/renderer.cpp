#include "graphics/renderer.h"
#include "camera.h"
#include "graphics/cave_details.h"

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
uniform int decorative;
uniform int lightCount;
uniform vec3 lightPositions[6];
uniform vec3 lightColors[6];
uniform float lightRadii[6];
uniform sampler3D occupancy;
uniform vec3 worldMinimum;
out vec4 fragmentColor;

struct MaterialSurface { vec3 albedo; vec3 normal; float roughness; float emission; };
float materialHash(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float materialNoise(vec2 p) {
    vec2 cell = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(materialHash(cell), materialHash(cell + vec2(1, 0)), f.x),
               mix(materialHash(cell + vec2(0, 1)), materialHash(cell + 1.0), f.x), f.y);
}
// One evaluation supplies color, shallow relief, roughness, and luminous glass.
// Correlated shapes are sampled on a 32-pixel grid, not random bright checkers.
MaterialSurface evaluateMaterial() {
    vec3 n = normalize(surfaceNormal);
    if (decorative != 0) {
        float brush = floor(materialNoise(worldPosition.xz * 2.0 + worldPosition.y) * 4.0) / 4.0;
        vec3 color = surfaceColor * (0.87 + 0.18 * brush);
        if (materialId == 10) color = min(color * vec3(0.55, 1.0, 1.0), vec3(0.96));
        return MaterialSurface(color, n,
            materialId == 10 ? 0.24 : 0.88, materialId == 10 ? 0.38 : 0.0);
    }
    bool top = abs(n.y) > 0.5, sideX = abs(n.x) > 0.5;
    vec3 u = top || !sideX ? vec3(1, 0, 0) : vec3(0, 0, 1);
    vec3 v = top ? vec3(0, 0, 1) : vec3(0, 1, 0);
    vec2 uv = vec2(dot(worldPosition, u), dot(worldPosition, v));
    vec2 p = (floor(uv * 32.0) + 0.5) / 32.0, local = fract(p);
    vec3 cell = floor(worldPosition - n * 0.001);
    float seed = dot(cell * abs(n), vec3(17, 37, 57)) + float(materialId) * 19.0;
    float broad = materialNoise(p * 1.7 + seed);
    float chips = materialNoise(p * 5.5 + seed + 8.0);
    float fine = materialHash(floor(p * 32.0) + seed) - 0.5;
    vec3 color = surfaceColor * (0.86 + 0.18 * broad + 0.06 * chips + 0.035 * fine);
    vec2 slope = vec2(0.0);
    float roughness = 0.90, emission = 0.0;
    if (materialId == 3 || materialId == 5 || materialId == 6 || materialId == 15
        || (materialId >= 7 && materialId <= 9)) {
        bool shale = materialId == 6 || materialId == 15;
        // Irregular mineral plates give stone broad painted planes and chipped
        // edges rather than the repeating wavy lines of a noise-only surface.
        vec2 plate = p * vec2(0.95, shale ? 2.4 : 1.15);
        vec2 tile = floor(plate), within = fract(plate);
        float nearest = 10.0, second = 10.0, plateColor = 0.0;
        vec2 plane = vec2(0.0);
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                vec2 offset = vec2(x, y);
                float h = materialHash(tile + offset + seed);
                vec2 center = offset + 0.18 + 0.64 * vec2(h, fract(h * 17.73));
                vec2 delta = within - center;
                float distance = dot(delta, delta);
                if (distance < nearest) {
                    second = nearest; nearest = distance; plane = delta; plateColor = h;
                } else second = min(second, distance);
            }
        }
        float edge = sqrt(second) - sqrt(nearest);
        float crack = 1.0 - smoothstep(0.018, 0.065, edge);
        float brush = floor((dot(plane, vec2(-0.65, 0.35)) + 0.8) * 5.0) / 5.0;
        if (materialId >= 7 && materialId <= 9) color = vec3(0.40, 0.44, 0.47);
        color *= (0.75 + 0.24 * plateColor + 0.15 * brush) * (1.0 - 0.42 * crack);
        color *= mix(vec3(0.78, 0.88, 1.12), vec3(1.04, 0.98, 0.83), plateColor);
        color += vec3(0.035, 0.055, 0.065) * (1.0 - smoothstep(0.06, 0.12, edge))
                 * step(0.0, plane.y);
        slope = plane * (shale ? 0.12 : 0.18);
        if (materialId >= 7 && materialId <= 9) {
            vec2 mineralCell = floor(p * 3.0);
            float h = materialHash(mineralCell + seed), h2 = materialHash(mineralCell + seed + 11.0);
            vec2 q = fract(p * 3.0) - vec2(0.35 + 0.30 * h, 0.35 + 0.30 * h2);
            q = mat2(0.8, -0.6, 0.6, 0.8) * q;
            float shape = max(abs(q.x) * 0.83 + abs(q.y) * 0.66, max(abs(q.x), abs(q.y)) * 0.94);
            float radius = 0.21 + 0.16 * h2;
            if (h > 0.48 && shape < radius + 0.035) {
                vec3 ore = materialId == 7 ? vec3(0.065, 0.080, 0.095)
                    : (materialId == 8 ? vec3(0.68, 0.34, 0.16) : vec3(0.58, 0.57, 0.50));
                if (materialId == 8 && h2 > 0.77) ore = mix(ore, vec3(0.20, 0.42, 0.34), 0.58);
                float facet = q.x + q.y * 0.7 > 0.0 ? 1.17 : 0.78;
                color = shape > radius ? vec3(0.16, 0.19, 0.20) : ore * facet;
                if (shape < radius && q.y > radius * 0.40) color += vec3(0.07, 0.065, 0.05);
                roughness = materialId == 7 ? 0.64 : 0.48;
                slope += vec2(sign(q.x), sign(q.y)) * 0.12;
            }
        }
    } else if (materialId == 11 || materialId == 12) {
        bool timber = materialId == 11;
        if (timber && top) {
            vec2 q = local - vec2(0.46, 0.52);
            float radius = length(q * vec2(1.0, 1.12)) + chips * 0.025;
            float ring = smoothstep(0.68, 0.87, fract(radius * 13.0));
            color *= 1.12 - 0.29 * ring;
            if (abs(q.x + q.y * 0.25) < 0.018 && radius > 0.12) color *= 0.72;
        } else {
            vec2 along = timber ? p.yx : p.xy;
            float fibers = materialNoise(along * vec2(1.2, 25.0) + seed);
            float bend = materialNoise(along * vec2(0.7, 4.0) + seed + 2.0);
            color *= 0.84 + 0.20 * fibers + 0.12 * bend;
            vec2 knotCell = floor(along / vec2(2.0, 1.0));
            float knotSeed = materialHash(knotCell + seed);
            vec2 q = fract(along / vec2(2.0, 1.0)) - vec2(0.25 + knotSeed * 0.5, 0.5);
            float knot = length(q * vec2(2.8, 7.0));
            if (knotSeed > 0.45 && knot < 0.65) {
                color *= 0.65 + 0.29 * smoothstep(0.2, 0.5, knot);
                color *= 0.88 + 0.12 * sin(knot * 32.0);
            }
            slope = (timber ? vec2(fibers - 0.5, 0.0) : vec2(0.0, fibers - 0.5)) * 0.09;
        }
        if (!timber) {
            float row = floor(p.y * 2.0);
            vec2 board = vec2(p.x / 2.0 + mod(row, 3.0) / 3.0, p.y * 2.0);
            vec2 f = fract(board), edge = min(f, 1.0 - f) * vec2(2.0, 0.5);
            color *= 0.92 + 0.12 * materialHash(floor(board) + seed);
            if (min(edge.x, edge.y) < 0.023) color *= 0.43;
            else slope += sign(f - 0.5) * (1.0 - smoothstep(vec2(0.025), vec2(0.06), edge)) * 0.22;
        }
        roughness = 0.78;
    } else if (materialId == 13) {
        float row = floor(p.y * 2.0);
        vec2 brick = vec2(p.x + mod(row, 2.0) * 0.5, p.y * 2.0);
        vec2 f = fract(brick), edge = min(f, 1.0 - f) * vec2(1.0, 0.5);
        color *= 0.88 + 0.22 * materialHash(floor(brick) + seed);
        if (min(edge.x, edge.y) < 0.024) color = vec3(0.24, 0.225, 0.20) * (0.94 + chips * 0.09);
        else {
            slope = sign(f - 0.5) * (1.0 - smoothstep(vec2(0.025), vec2(0.065), edge)) * 0.28;
            color *= 0.91 + 0.09 * smoothstep(0.025, 0.06, min(edge.x, edge.y));
        }
    } else if (materialId == 10) {
        vec2 center = 0.41 + 0.18 * fract(vec2(dot(cell, vec3(0.73, 0.41, 0.67)),
                                                 dot(cell, vec3(0.37, 0.83, 0.53))));
        vec2 q = local - center;
        bool vertical = abs(q.y) > abs(q.x);
        float facet = vertical ? (q.y > 0.0 ? 1.12 : 0.62) : (q.x > 0.0 ? 0.87 : 0.72);
        color = mix(vec3(0.015, 0.26, 0.48), vec3(0.04, 0.76, 0.93), facet)
                * (0.91 + 0.09 * broad);
        float ridge = min(abs(q.x - q.y), abs(q.x + q.y));
        color += vec3(0.015, 0.13, 0.16) * (1.0 - smoothstep(0.015, 0.045, ridge));
        color = min(color, vec3(0.96));
        slope = vertical ? vec2(0.0, sign(q.y) * 0.32) : vec2(sign(q.x) * 0.32, 0.0);
        roughness = 0.23; emission = 0.38;
    } else if (materialId == 14) {
        vec2 edge = min(local, 1.0 - local);
        bool metal = min(edge.x, edge.y) < 0.095 || abs(local.x - 0.5) < 0.027
            || abs(local.y - 0.72) < 0.025 || (top && abs(local.y - 0.5) < 0.04);
        if (metal) {
            color = vec3(0.16, 0.13, 0.09) * (0.85 + chips * 0.23);
            roughness = 0.48;
        } else {
            float glow = 1.0 - smoothstep(0.05, 0.55, length((local - vec2(0.5, 0.4)) * vec2(1.2, 1.0)));
            color = mix(vec3(0.81, 0.36, 0.075), vec3(1.0, 0.72, 0.26), glow);
            roughness = 0.22; emission = 1.0;
        }
        slope = sign(local - 0.5) * (1.0 - smoothstep(vec2(0.07), vec2(0.14), edge)) * 0.20;
    } else if (materialId == 1 && !top) {
        float grass = step(0.78 + chips * 0.12, local.y);
        color = mix(vec3(0.39, 0.27, 0.15) * (0.90 + 0.14 * broad), color, grass);
    }
    return MaterialSurface(max(color, vec3(0.0)), normalize(n + u * slope.x + v * slope.y), roughness, emission);
}

bool solid(ivec3 cell) {
    ivec3 local = cell - ivec3(worldMinimum);
    if (any(lessThan(local, ivec3(0))) || any(greaterThanEqual(local, textureSize(occupancy, 0)))) return false;
    return texelFetch(occupancy, local, 0).r > 0.5;
}

float contactShade() {
    vec3 normal = normalize(surfaceNormal);
    ivec3 outside = ivec3(floor(worldPosition + normal * 0.02));
    ivec3 u = abs(normal.x) > 0.5 ? ivec3(0, 0, 1) : ivec3(1, 0, 0);
    ivec3 v = abs(normal.y) > 0.5 ? ivec3(0, 0, 1) : ivec3(0, 1, 0);
    vec2 position = fract(vec2(dot(worldPosition, vec3(u)), dot(worldPosition, vec3(v))));
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

void main() {
    MaterialSurface surface = evaluateMaterial();
    vec3 normal = normalize(surface.normal);
    vec3 color = surface.albedo;
    vec3 toCamera = cameraPosition - worldPosition;
    float cameraDistance = length(toCamera);
    vec3 viewDirection = toCamera / max(cameraDistance, 0.001);
    vec3 lit;
    vec3 fogColor;
    float fog;
    if (underground != 0) {
        float ao = decorative != 0 ? 1.0 : contactShade();
        float cone = smoothstep(0.35, 0.94, dot(-viewDirection, cameraForward));
        float headlamp = 0.46 * (0.2 + 0.8 * max(dot(normal, viewDirection), 0.0))
            * (0.25 + 0.75 * cone) / (1.0 + 0.035 * cameraDistance * cameraDistance);
        vec3 ambient = mix(vec3(0.10, 0.055, 0.19), vec3(0.025, 0.14, 0.17),
                           surfaceNormal.y * 0.5 + 0.5);
        vec3 light = (ambient + vec3(0.68, 0.91, 1.08) * headlamp) * ao;
        vec3 pointLight = vec3(0.0);
        vec3 highlight = vec3(0.0);
        vec3 start = worldPosition + surfaceNormal * 0.03;
        for (int i = 0; i < lightCount; ++i) {
            vec3 offset = lightPositions[i] - worldPosition;
            float distanceToLight = length(offset);
            float falloff = max(1.0 - distanceToLight / lightRadii[i], 0.0);
            vec3 direction = offset / max(distanceToLight, 0.001);
            float facing = max(dot(normal, direction), 0.0);
            if (falloff <= 0.0 || facing <= 0.0 || dot(surfaceNormal, direction) <= 0.0) continue;
            float strength = falloff * falloff * visibleLight(start, lightPositions[i]);
            if (strength <= 0.0) continue;
            pointLight += lightColors[i] * strength * 1.65 * facing;
            vec3 halfDirection = direction + viewDirection;
            halfDirection /= max(length(halfDirection), 0.001);
            float shine = pow(max(dot(normal, halfDirection), 0.0), mix(64.0, 8.0, surface.roughness));
            highlight += lightColors[i] * strength * shine * (1.0 - surface.roughness) * 0.14;
        }
        float strongest = max(pointLight.r, max(pointLight.g, pointLight.b));
        light += pointLight / max(1.0, strongest) * mix(0.65, 1.0, ao);
        lit = color * light + highlight;
        fogColor = mix(vec3(0.018, 0.105, 0.14), vec3(0.065, 0.032, 0.13),
                       smoothstep(4.0, 12.0, worldPosition.y));
        fog = smoothstep(12.0, 55.0, cameraDistance) * 0.88;
    } else {
        float sun = max(dot(normal, normalize(vec3(-0.6, 1.0, 0.3))), 0.0);
        lit = color * (0.45 + 0.55 * sun);
        fogColor = vec3(0.48, 0.70, 0.90);
        fog = smoothstep(55.0, 150.0, cameraDistance);
    }
    lit = max(lit, color * surface.emission * (materialId == 10 ? 2.0 : 1.0));
    // Keep turquoise pigment and facet contrast under overlapping crystal lights.
    if (materialId == 10) lit = min(lit, color);
    if (underground != 0) lit = pow(max(lit, vec3(0.0)), vec3(1.0 / 1.6));
    fragmentColor = vec4(mix(lit, fogColor, fog), 1.0);
}
)glsl";

} // namespace

WorldRenderer::WorldRenderer() : shader_(worldVertexShader, worldFragmentShader)
{
    glGenTextures(1, &occupancy_);
    if (!occupancy_) throw std::runtime_error("Failed to allocate voxel lighting texture");
}

WorldRenderer::~WorldRenderer()
{
    glDeleteTextures(1, &occupancy_);
}

void WorldRenderer::render(const World& world, const Camera& camera, int width, int height,
                           bool wireframe, bool underground)
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
        if (index % World::Width > 0) rebuild[index - 1] = true;
        if (index % World::Width + 1 < World::Width) rebuild[index + 1] = true;
        if (index >= World::Width) rebuild[index - World::Width] = true;
        if (index + World::Width < meshes_.size()) rebuild[index + World::Width] = true;
    }
    bool changed = false;
    for (std::size_t index = 0; index < meshes_.size(); ++index) {
        if (rebuild[index]) {
            meshes_[index].upload(buildMesh(world, index));
            details_[index].upload(buildCaveDetails(world, index));
            changed = true;
        }
    }
    GLint activeTexture = 0, previousTexture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &previousTexture);
    glBindTexture(GL_TEXTURE_3D, occupancy_);
    if (changed) {
        lights_.clear();
        constexpr auto size = World::MaxExclusive - World::Min;
        std::vector<unsigned char> voxels(size.x * size.y * size.z);
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
                        voxels[local.x + size.x * (local.y + size.y * local.z)] = isSolid(block) ? 255 : 0;
                        if (block != Block::Lantern && block != Block::Crystal) continue;
                        bool exposed = false;
                        for (const auto offset : {glm::ivec3(1,0,0), glm::ivec3(-1,0,0),
                             glm::ivec3(0,1,0), glm::ivec3(0,-1,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1)}) {
                            exposed |= !isSolid(world.get(position + offset));
                        }
                        if (!exposed) continue;
                        lights_.push_back({glm::vec3(position) + 0.5f,
                            block == Block::Lantern ? glm::vec3(1.15f, 0.58f, 0.18f)
                                                   : glm::vec3(0.10f, 0.85f, 1.15f),
                            block == Block::Lantern ? 11.0f : 8.0f});
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
        meshedWorld_ = world;
    }
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(underground ? 0.025f : 0.48f, underground ? 0.026f : 0.70f,
                 underground ? 0.065f : 0.90f, 1.0f);
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
    shader_.setInt("decorative", 0);
    shader_.setInt("occupancy", 0);
    shader_.setVec3("worldMinimum", glm::vec3(World::Min));
    // Cache the light sources with the world, then select a small fixed number
    // near the camera, spreading them out so one dense vein cannot take every
    // slot. Walking never rescans blocks or rebuilds geometry.
    std::array<const Light*, 6> nearest{};
    if (underground) {
        for (std::size_t slot = 0; slot < nearest.size(); ++slot) {
            float closest = std::numeric_limits<float>::max();
            for (const auto& light : lights_) {
                bool clustered = false;
                for (std::size_t prior = 0; prior < slot; ++prior) {
                    const auto offset = light.position - nearest[prior]->position;
                    clustered |= glm::dot(offset, offset) < 4.0f;
                }
                if (clustered) continue;
                const auto offset = light.position - camera.position();
                const float distance = glm::dot(offset, offset);
                if (distance < closest) { closest = distance; nearest[slot] = &light; }
            }
            if (!nearest[slot]) break;
        }
    }
    int lightCount = 0;
    for (const auto* light : nearest) {
        if (!light) break;
        const auto index = std::to_string(lightCount++);
        shader_.setVec3(("lightPositions[" + index + "]").c_str(), light->position);
        shader_.setVec3(("lightColors[" + index + "]").c_str(), light->color);
        shader_.setFloat(("lightRadii[" + index + "]").c_str(), light->radius);
    }
    shader_.setInt("lightCount", lightCount);
    visibleChunks_ = drawnTriangles_ = 0;
    for (std::size_t index = 0; index < meshes_.size(); ++index) {
        const glm::vec3 minimum(world.origin(index));
        const float margin = underground && details_[index].indexCount() != 0 ? 1.0f : 0.0f;
        if (meshes_[index].indexCount() == 0
            || !frustum.intersects(minimum - margin, minimum + glm::vec3(Chunk::Size + margin))) continue;
        meshes_[index].draw();
        ++visibleChunks_;
        drawnTriangles_ += meshes_[index].indexCount() / 3;
        if (underground && details_[index].indexCount() != 0) {
            shader_.setInt("decorative", 1);
            details_[index].draw();
            drawnTriangles_ += details_[index].indexCount() / 3;
            shader_.setInt("decorative", 0);
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindTexture(GL_TEXTURE_3D, static_cast<GLuint>(previousTexture));
    glActiveTexture(static_cast<GLenum>(activeTexture));
}

std::size_t WorldRenderer::triangleCount() const
{
    std::size_t total = 0;
    for (const auto& mesh : meshes_) total += mesh.indexCount() / 3;
    if (underground_) for (const auto& mesh : details_) total += mesh.indexCount() / 3;
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
