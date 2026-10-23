#include "graphics/held_sword.h"
#include "graphics/asset_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace voxel {
namespace {

constexpr glm::vec3 Steel{0.58f, 0.73f, 0.77f};
constexpr glm::vec3 Brass{0.62f, 0.39f, 0.15f};
constexpr glm::vec3 Leather{0.23f, 0.105f, 0.055f};
constexpr glm::vec3 LeatherEdge{0.36f, 0.19f, 0.09f};
constexpr glm::vec3 Turquoise{0.12f, 0.64f, 0.70f};

// Closed eight-sided blocks retain broad, readable facets at first-person size.
void bevelBox(AssetBuilder& b, glm::vec3 lo, glm::vec3 hi, float cut,
              Block material, glm::vec3 color)
{
    const std::array<glm::vec2, 8> ring{{
        {lo.x + cut, lo.z}, {hi.x - cut, lo.z},
        {hi.x, lo.z + cut}, {hi.x, hi.z - cut},
        {hi.x - cut, hi.z}, {lo.x + cut, hi.z},
        {lo.x, hi.z - cut}, {lo.x, lo.z + cut}
    }};
    const glm::vec3 bottom{(lo.x + hi.x) * .5f, lo.y, (lo.z + hi.z) * .5f};
    const glm::vec3 top{bottom.x, hi.y, bottom.z};
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const auto a = ring[i], c = ring[(i + 1) % ring.size()];
        const glm::vec3 al{a.x, lo.y, a.y}, ah{a.x, hi.y, a.y};
        const glm::vec3 cl{c.x, lo.y, c.y}, ch{c.x, hi.y, c.y};
        b.quad(al, ah, ch, cl, material, color * ((i & 1) ? .90f : 1.0f));
        b.triangle(bottom, al, cl, material, color * .85f);
        b.triangle(top, ch, ah, material, color);
    }
}

using BladeRing = std::array<glm::vec3, 8>;
BladeRing bladeRing(float y, float width, float depth)
{
    return {{{-width,y,0}, {-width*.74f,y,-depth*.55f}, {0,y,-depth},
             {width*.74f,y,-depth*.55f}, {width,y,0},
             {width*.74f,y,depth*.55f}, {0,y,depth}, {-width*.74f,y,depth*.55f}}};
}

float ease(float t) { return t * t * (3.0f - 2.0f * t); }

struct PoseKey {
    float time;
    glm::vec3 translation;
    glm::vec3 rotation;
};

} // namespace

MeshData buildHeldSword()
{
    MeshData mesh;
    const HubAsset origin{};
    AssetBuilder b(mesh, origin);

    // The cutting edges, broad bevels and raised central ridge are geometry on
    // both sides. Each ring narrows toward a solid four-sided-looking point.
    const std::array<BladeRing, 3> blade{{bladeRing(.17f,.050f,.020f),
        bladeRing(.24f,.061f,.026f), bladeRing(.74f,.043f,.018f)}};
    for (std::size_t ring = 0; ring + 1 < blade.size(); ++ring) {
        for (std::size_t i = 0; i < blade[ring].size(); ++i) {
            const auto next = (i + 1) % blade[ring].size();
            const bool edge = i == 0 || i == 3 || i == 4 || i == 7;
            const auto color = Steel * (edge ? 1.22f : ((i & 1) ? .91f : 1.04f));
            b.quad(blade[ring][i], blade[ring + 1][i], blade[ring + 1][next],
                   blade[ring][next], Block::IronOre, color);
        }
    }
    for (std::size_t i = 0; i < blade.front().size(); ++i) {
        const auto next = (i + 1) % blade.front().size();
        b.triangle({0,.17f,0}, blade.front()[i], blade.front()[next], Block::IronOre, Steel);
        b.triangle(blade.back()[i], {0,.89f,0}, blade.back()[next], Block::IronOre,
                   Steel * ((i & 1) ? 1.12f : .94f));
    }

    // A short brass collar and swept guard, with cool mineral inlay at the hilt.
    bevelBox(b, {-.056f,.142f,-.030f}, {.056f,.202f,.030f}, .012f, Block::CopperOre, Brass);
    bevelBox(b, {-.16f,.105f,-.045f}, {.16f,.153f,.045f}, .014f, Block::CopperOre, Brass);
    for (float side : {-1.0f, 1.0f}) {
        b.beam({side*.125f,.129f,0}, {side*.195f,.164f,0}, .044f, Block::CopperOre, Brass * 1.12f);
        bevelBox(b, {side*.195f-.022f,.145f,-.027f},
                    {side*.195f+.022f,.186f,.027f}, .009f, Block::IronOre, Steel * .62f);
    }
    bevelBox(b, {-.027f,.115f,.046f}, {.027f,.157f,.058f}, .008f, Block::Slate, Turquoise);

    bevelBox(b, {-.034f,-.168f,-.033f}, {.034f,.108f,.033f}, .010f, Block::Timber, Leather);
    // Raised overlapping bindings run around the handle, including its back.
    for (int i = 0; i < 9; ++i) {
        const float y = -.153f + i * .029f;
        b.beam({-.035f,y,.034f},{.035f,y+.020f,.034f},.010f,Block::Timber,LeatherEdge);
        b.beam({.035f,y+.020f,.034f},{.035f,y+.023f,-.034f},.010f,Block::Timber,LeatherEdge);
        b.beam({.035f,y+.023f,-.034f},{-.035f,y+.003f,-.034f},.010f,Block::Timber,LeatherEdge);
        b.beam({-.035f,y+.003f,-.034f},{-.035f,y,.034f},.010f,Block::Timber,LeatherEdge);
    }
    bevelBox(b, {-.047f,-.192f,-.043f}, {.047f,-.160f,.043f}, .014f, Block::CopperOre, Brass);
    bevelBox(b, {-.054f,-.238f,-.046f}, {.054f,-.190f,.046f}, .018f, Block::IronOre, Steel * .59f);
    bevelBox(b, {-.026f,-.228f,.047f}, {.026f,-.198f,.059f}, .009f, Block::Slate, Turquoise);

    // A right hand curled around the grip. Separated finger bands and an angled
    // thumb make the hold legible without a large arm obscuring the weapon.
    bevelBox(b, {.018f,-.118f,-.057f}, {.115f,.061f,.060f}, .022f, Block::Timber, Leather);
    for (int finger = 0; finger < 4; ++finger) {
        const float y = -.112f + finger * .040f;
        bevelBox(b, {-.052f,y,.018f}, {.081f,y+.033f,.080f}, .012f,
                    Block::Timber, LeatherEdge * (1.0f - finger*.025f));
    }
    b.beam({.10f,.030f,-.027f},{.024f,-.020f,-.061f},.052f,Block::Timber,LeatherEdge);
    b.beam({.024f,-.020f,-.061f},{-.018f,-.043f,-.015f},.046f,Block::Timber,LeatherEdge);
    bevelBox(b, {.065f,-.180f,-.070f}, {.180f,-.105f,.075f}, .019f, Block::Timber, Leather * .77f);
    bevelBox(b, {.075f,-.166f,.076f}, {.157f,-.136f,.087f}, .009f, Block::CopperOre, Brass * .76f);
    return mesh;
}

glm::mat4 heldSwordPose(float swingRemaining, float aspectRatio)
{
    if (!std::isfinite(swingRemaining)) swingRemaining = 0;
    if (!std::isfinite(aspectRatio) || aspectRatio <= 0) aspectRatio = 16.0f / 9.0f;
    const float aspect = std::clamp(aspectRatio, 1.0f, 3.6f);
    const float t = 1.0f - std::clamp(swingRemaining, 0.0f, 1.0f);
    // Anticipation pulls the hilt back; the slash sweeps through depth and across
    // the centre, then settles. Endpoints exactly match the idle pose.
    const std::array<PoseKey, 5> keys{{
        {0.00f, {0,0,0}, {-10,-25,-18}},
        {0.19f, {.035f,.035f,.025f}, {-18,-38,-35}},
        {0.49f, {-.19f,.025f,-.16f}, {20,35,72}},
        {0.72f, {-.10f,-.065f,-.03f}, {10,5,35}},
        {1.00f, {0,0,0}, {-10,-25,-18}}
    }};
    glm::vec3 position{}, rotation{};
    for (std::size_t i = 1; i < keys.size(); ++i) {
        if (t <= keys[i].time) {
            const float f = ease((t - keys[i-1].time) / (keys[i].time - keys[i-1].time));
            position = glm::mix(keys[i-1].translation, keys[i].translation, f);
            rotation = glm::mix(keys[i-1].rotation, keys[i].rotation, f);
            break;
        }
    }
    position += glm::vec3{.27f * aspect, -.40f, -1.08f};
    auto model = glm::translate(glm::mat4(1), position);
    model = glm::rotate(model, glm::radians(rotation.z), {0,0,1});
    model = glm::rotate(model, glm::radians(rotation.y), {0,1,0});
    model = glm::rotate(model, glm::radians(rotation.x), {1,0,0});
    return glm::scale(model, glm::vec3(.85f));
}

} // namespace voxel
