#include "material_continuity_smoke.h"
#include "graphics/materials.h"
#include "graphics/opengl.h"
#include "ores.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace voxel {
namespace {
struct Probe {
    glm::vec3 point, normal;
    Block material;
    int decorative = 0;
    glm::vec3 anchorNormal{};
    float height = 0;
};
struct Surface {
    glm::vec3 albedo, normal;
    float emission, roughness;
};
void require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

// Evaluate the production material shader without the light rig, AO, fog, or
// tone mapping. A corner must retain the same paint while its normals continue
// to face different directions; comparing lit screenshots cannot separate those.
std::vector<Surface> evaluate(const std::vector<Probe>& probes)
{
    constexpr const char* vertex = R"glsl(#version 410 core
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0, 1);
}
)glsl";
    const auto fragment = std::string(R"glsl(#version 410 core
uniform sampler2D probes;
vec3 worldPosition, modelPosition, surfaceNormal, surfaceColor;
vec4 reliefSurface;
int materialId, decorative;
const float timeSeconds = 0.0;
out vec4 result;
)glsl") + materialShader + R"glsl(
void main() {
    int index = int(gl_FragCoord.x);
    vec4 point = texelFetch(probes, ivec2(index, 0), 0);
    vec4 face = texelFetch(probes, ivec2(index, 1), 0);
    worldPosition = modelPosition = point.xyz;
    materialId = int(point.w);
    surfaceNormal = face.xyz;
    decorative = int(face.w);
    surfaceColor = texelFetch(probes, ivec2(index, 2), 0).xyz;
    reliefSurface = texelFetch(probes, ivec2(index, 3), 0);
    MaterialSurface surface = evaluateMaterial();
    result = gl_FragCoord.y < 1.0 ? vec4(surface.albedo, surface.emission)
                                : vec4(surface.normal, surface.roughness);
}
)glsl";
    Shader shader(vertex, fragment.c_str());
    GLuint frame = 0, target = 0, data = 0, vao = 0;
    glGenFramebuffers(1, &frame); glGenTextures(1, &target);
    glGenTextures(1, &data); glGenVertexArrays(1, &vao);
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const auto cleanup = [&] {
        glBindFramebuffer(GL_FRAMEBUFFER, 0); glBindVertexArray(0); glUseProgram(0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteFramebuffers(1, &frame); glDeleteTextures(1, &target);
        glDeleteTextures(1, &data); glDeleteVertexArrays(1, &vao);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE);
    };
    std::vector<Surface> result;
    try {
        constexpr int capacity = 512;
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, target);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, capacity, 2, 0, GL_RGBA, GL_FLOAT, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, frame);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "Material continuity probe framebuffer is incomplete");
        glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_FRAMEBUFFER_SRGB);
        glBindVertexArray(vao); shader.use(); shader.setInt("probes", 0);
        glBindTexture(GL_TEXTURE_2D, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        for (std::size_t start = 0; start < probes.size(); start += capacity) {
            const int count = static_cast<int>(std::min<std::size_t>(capacity, probes.size() - start));
            std::vector<glm::vec4> input(static_cast<std::size_t>(count) * 4);
            for (int i = 0; i < count; ++i) {
                const auto& probe = probes[start + i];
                input[i] = glm::vec4(probe.point, static_cast<float>(probe.material));
                input[count + i] = glm::vec4(probe.normal, static_cast<float>(probe.decorative));
                input[count * 2 + i] = glm::vec4(blockColor(probe.material), 1);
                input[count * 3 + i] = glm::vec4(probe.anchorNormal, probe.height);
            }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, count, 4, 0, GL_RGBA, GL_FLOAT, input.data());
            glViewport(0, 0, count, 2); glDrawArrays(GL_TRIANGLES, 0, 3);
            std::vector<glm::vec4> pixels(static_cast<std::size_t>(count) * 2);
            glReadPixels(0, 0, count, 2, GL_RGBA, GL_FLOAT, pixels.data());
            require(glGetError() == GL_NO_ERROR, "Material continuity probe generated an OpenGL error");
            for (int i = 0; i < count; ++i)
                result.push_back({glm::vec3(pixels[i]), glm::vec3(pixels[count + i]),
                                  pixels[i].w, pixels[count + i].w});
        }
    } catch (...) { cleanup(); throw; }
    cleanup();
    return result;
}
} // namespace

void verifyMaterialContinuity()
{
    std::vector<Block> materials{Block::Grass, Block::Dirt, Block::Stone, Block::Limestone,
        Block::Slate, Block::MossStone, Block::Calcite, Block::Clay, Block::Silt,
        Block::Moss, Block::GlowMoss, Block::RootedDirt};
    for (const auto& ore : oreCatalog()) materials.push_back(ore.block);
    // Both signs and chunk-border coordinates catch face-local seeds and
    // truncation of negative coordinates masquerading as a seamless pattern.
    constexpr std::array cells{glm::vec3(3, 40, 5), glm::vec3(-5, 36, -9), glm::vec3(15, 31, -16)};
    std::vector<Probe> probes;
    struct Pair { std::size_t first; Block material; bool inset; };
    std::vector<Pair> pairs;
    for (const auto material : materials) for (const auto cell : cells)
        for (int a = 0; a < 3; ++a) for (int b = a + 1; b < 3; ++b)
            for (int endA = 0; endA <= 1; ++endA) for (int endB = 0; endB <= 1; ++endB)
                for (int i = 0; i < 16; ++i) {
                    glm::vec3 point = cell, normalA(0), normalB(0);
                    point[a] += endA; point[b] += endB;
                    point[3 - a - b] += (i + .5f) / 16;
                    normalA[a] = endA ? 1.0f : -1.0f;
                    normalB[b] = endB ? 1.0f : -1.0f;
                    // Rasterized side fragments approach the turf lip from
                    // inside its owning block, never the block above it.
                    if (material == Block::Grass && point.y == cell.y + 1) point.y -= .0001f;
                    if (material == Block::Grass && point.y == cell.y) point.y += .0001f;
                    pairs.push_back({probes.size(), material, false});
                    probes.push_back({point, normalA, material});
                    probes.push_back({point, normalB, material});
                    pairs.push_back({probes.size(), material, true});
                    probes.push_back({point - normalB / 128.0f, normalA, material});
                    probes.push_back({point - normalA / 128.0f, normalB, material});
                }

    const auto reliefStart = probes.size();
    const auto facet = glm::normalize(glm::vec3(.22f, .94f, .16f));
    for (const auto material : materials) for (const auto cell : cells) for (int i = 0; i < 12; ++i) {
        const auto point = cell + glm::vec3((i + .5f) / 12, 1, .375f);
        probes.push_back({point, {0, 1, 0}, material});
        probes.push_back({point + glm::vec3(0, .03125f, 0), facet, material, 2, {0, 1, 0}, .03125f});
    }
    const auto grassStart = probes.size();
    for (const auto cell : cells) for (int i = 0; i < 16; ++i) {
        const auto point = cell + glm::vec3((i + .5f) / 16, 0, .375f);
        probes.push_back({point, {0, -1, 0}, Block::Grass});
        probes.push_back({point, {0, -1, 0}, Block::Dirt});
        probes.push_back({point + glm::vec3(0, 1, 0), {0, 1, 0}, Block::Grass});
    }
    const auto samples = evaluate(probes);
    std::array<double, 256> insetError{};
    std::array<std::size_t, 256> insetCount{};
    for (const auto& pair : pairs) {
        const auto& a = samples[pair.first];
        const auto& b = samples[pair.first + 1];
        const auto error = glm::length(a.albedo - b.albedo);
        const auto label = std::string(blockName(pair.material));
        require(std::isfinite(error), "Material seam evaluation is not finite: " + label);
        if (pair.inset) {
            insetError[static_cast<unsigned char>(pair.material)] += error;
            ++insetCount[static_cast<unsigned char>(pair.material)];
        } else {
            require(error < .002f && std::abs(a.emission - b.emission) < .002f
                    && std::abs(a.roughness - b.roughness) < .002f,
                    "Material paint resets at a cube edge: " + label + " (" + std::to_string(error) + ")");
        }
    }
    for (const auto material : materials) {
        const auto id = static_cast<unsigned char>(material);
        const auto mean = insetError[id] / insetCount[id];
        // Adjacent points may straddle an actual mineral boundary. Their
        // average difference stays small when both faces share the same field.
        require(mean < .055, "Material changes abruptly near cube edges: "
            + std::string(blockName(material)) + " (" + std::to_string(mean) + ")");
    }
    for (std::size_t i = reliefStart; i < grassStart; i += 2) {
        require(glm::length(samples[i].albedo - samples[i + 1].albedo) < .002f,
                "Raised mineral paint drifted from its supporting block: " + std::string(blockName(probes[i].material)));
        require(glm::length(samples[i + 1].normal - facet) < .001f,
                "Seamless paint flattened the relief normals used by lighting");
    }
    for (std::size_t i = grassStart; i < samples.size(); i += 3) {
        require(glm::length(samples[i].albedo - samples[i + 1].albedo) < .002f,
                "Grass underside does not continue into the dirt material");
        require(samples[i].albedo.r > samples[i].albedo.g && samples[i + 2].albedo.g > samples[i + 2].albedo.r * 1.2f,
                "Grass lost its brown soil underside or green upper surface");
    }
    std::cout << "Material continuity passed: grass, 11 natural surfaces and 18 ores across all cube edges, negative coordinates, relief anchors and soil undersides\n";
}
} // namespace voxel
