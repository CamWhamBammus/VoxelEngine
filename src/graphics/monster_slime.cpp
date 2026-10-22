#include "graphics/monster_builder.h"

#include <array>

namespace voxel {
namespace {

enum Joint {
    Root, Body, Face, LeftEye, RightEye, LeftPupil, RightPupil,
    Mouth, Sprout, LeftLeaf, RightLeaf, JointCount
};

constexpr float Tau = 6.28318530718f;
constexpr glm::vec3 DarkJade{0.025f, 0.31f, 0.19f};
constexpr glm::vec3 Jade{0.06f, 0.69f, 0.35f};
constexpr glm::vec3 Mint{0.39f, 0.92f, 0.55f};
constexpr glm::vec3 Ink{0.025f, 0.105f, 0.085f};
constexpr glm::vec3 EyeWhite{0.86f, 0.98f, 0.67f};

// Eight corners give the body broad, readable planes rather than a sphere's
// many little facets. The smaller crown and tucked-in base soften its outline.
std::array<glm::vec3, 8> ring(float y, float x, float z, float bevel)
{
    return {{{-x + bevel, y, -z}, {x - bevel, y, -z},
             {x, y, -z + bevel}, {x, y, z - bevel},
             {x - bevel, y, z}, {-x + bevel, y, z},
             {-x, y, z - bevel}, {-x, y, -z + bevel}}};
}

void slimeBody(AssetBuilder& b)
{
    const std::array rings{
        ring(0.0f, 0.55f, 0.46f, 0.16f),
        ring(0.13f, 0.72f, 0.62f, 0.18f),
        ring(0.65f, 0.68f, 0.59f, 0.16f),
        ring(0.87f, 0.53f, 0.45f, 0.13f),
        ring(0.96f, 0.34f, 0.29f, 0.09f),
    };
    const std::array colors{DarkJade, Jade, glm::vec3{0.16f, 0.78f, 0.39f}, Mint};
    for (std::size_t band = 0; band + 1 < rings.size(); ++band) {
        for (std::size_t side = 0; side < 8; ++side) {
            const auto next = (side + 1) % 8;
            const float tint = side == 5 || side == 6 ? 1.06f : side < 3 ? 0.91f : 1.0f;
            b.quad(rings[band][side], rings[band + 1][side],
                   rings[band + 1][next], rings[band][next],
                   Block::Stone, colors[band] * tint);
        }
    }
    for (std::size_t side = 0; side < 8; ++side) {
        const auto next = (side + 1) % 8;
        b.triangle({0, 0, 0}, rings.front()[side], rings.front()[next], Block::Stone, DarkJade * 0.8f);
        b.triangle({0, 0.96f, 0}, rings.back()[next], rings.back()[side], Block::Stone, Mint);
    }

    // Broad painted glints remain part of the silhouette and stay readable in
    // the cave's colored lighting. They are deliberately matte, not emissive.
    b.quad({-0.49f, 0.45f, 0.604f}, {-0.36f, 0.45f, 0.604f},
           {-0.36f, 0.64f, 0.593f}, {-0.49f, 0.64f, 0.593f}, Block::Stone, Mint);
    b.quad({-0.46f, 0.66f, 0.587f}, {-0.31f, 0.66f, 0.587f},
           {-0.31f, 0.79f, 0.504f}, {-0.46f, 0.79f, 0.504f}, Block::Stone, {0.67f, 0.98f, 0.71f});
    b.quad({0.32f, 0.19f, 0.619f}, {0.48f, 0.19f, 0.619f},
           {0.48f, 0.27f, 0.614f}, {0.32f, 0.27f, 0.614f}, Block::Stone, {0.08f, 0.52f, 0.28f});
}

void eye(AssetBuilder& b)
{
    b.box({-0.135f, -0.15f, -0.018f}, {0.135f, 0.15f, 0.005f}, Block::Stone, DarkJade);
    b.box({-0.105f, -0.12f, 0.004f}, {0.105f, 0.125f, 0.024f}, Block::Stone, EyeWhite);
}

void pupil(AssetBuilder& b)
{
    b.box({-0.058f, -0.086f, 0}, {0.058f, 0.088f, 0.016f}, Block::Stone, Ink);
    b.box({-0.045f, 0.032f, 0.016f}, {-0.008f, 0.072f, 0.021f}, Block::Stone, {0.93f, 1.0f, 0.88f});
}

void leaf(AssetBuilder& b, float direction)
{
    const glm::vec3 root{0, 0, 0};
    const glm::vec3 tip{direction * 0.27f, 0.13f, -0.015f};
    const glm::vec3 middle{direction * 0.14f, 0.08f, 0};
    const glm::vec3 front{direction * 0.11f, 0.055f, 0.09f};
    const glm::vec3 back{direction * 0.12f, 0.055f, -0.09f};
    b.triangle(root, middle, front, Block::Stone, {0.30f, 0.61f, 0.12f}, true);
    b.triangle(front, middle, tip, Block::Stone, {0.52f, 0.83f, 0.19f}, true);
    b.triangle(root, back, middle, Block::Stone, {0.33f, 0.69f, 0.14f}, true);
    b.triangle(back, tip, middle, Block::Stone, {0.64f, 0.90f, 0.25f}, true);
}

float mix(float a, float b, float t, float start, float end)
{
    return a + (b - a) * ramp(t, start, end);
}

struct Hop {
    float height = 0;
    float stretch = 1;
    float forward = 0;
    float impact = 0;
    float flight = 0;
};

Hop walkingHop(float t)
{
    Hop hop;
    if (t < 0.12f) return hop;
    if (t < 0.22f) {
        hop.stretch = mix(1, 0.64f, t, 0.12f, 0.22f);
    } else if (t < 0.70f) {
        const float air = (t - 0.22f) / 0.48f;
        hop.height = 0.74f * 4 * air * (1 - air);
        hop.flight = std::sin(air * Tau * 0.5f);
        hop.forward = 0.08f * std::sin(Tau * air);
        if (t < 0.29f) hop.stretch = mix(0.64f, 1.27f, t, 0.22f, 0.29f);
        else if (t < 0.48f) hop.stretch = mix(1.27f, 0.98f, t, 0.29f, 0.48f);
        else if (t < 0.66f) hop.stretch = mix(0.98f, 1.20f, t, 0.48f, 0.66f);
        else hop.stretch = mix(1.20f, 1.08f, t, 0.66f, 0.70f);
    } else if (t < 0.76f) {
        hop.stretch = mix(1.08f, 0.58f, t, 0.70f, 0.76f);
    } else if (t < 0.86f) {
        hop.stretch = mix(0.58f, 1.10f, t, 0.76f, 0.86f);
    } else if (t < 0.94f) {
        hop.stretch = mix(1.10f, 0.97f, t, 0.86f, 0.94f);
    } else {
        hop.stretch = mix(0.97f, 1, t, 0.94f, 1);
    }
    hop.impact = pulse(t, 0.70f, 0.76f, 0.89f);
    return hop;
}

Hop pounce(float t)
{
    Hop hop;
    if (t < 0.13f) {
        hop.stretch = mix(1, 0.53f, t, 0.015f, 0.13f);
    } else if (t < 0.40f) {
        const float air = (t - 0.13f) / 0.27f;
        hop.height = 1.03f * 4 * air * (1 - air);
        hop.forward = 1.45f * ease(air);
        hop.flight = std::sin(air * Tau * 0.5f);
        if (t < 0.18f) hop.stretch = mix(0.53f, 1.37f, t, 0.13f, 0.18f);
        else if (t < 0.29f) hop.stretch = mix(1.37f, 1.02f, t, 0.18f, 0.29f);
        else hop.stretch = mix(1.02f, 1.18f, t, 0.29f, 0.40f);
    } else if (t < 0.48f) {
        hop.forward = 1.45f;
        hop.stretch = mix(1.18f, 0.49f, t, 0.40f, 0.45f);
        if (t >= 0.45f) hop.stretch = mix(0.49f, 0.60f, t, 0.45f, 0.48f);
    } else if (t < 0.60f) {
        hop.forward = 1.45f;
        hop.stretch = t < 0.55f ? mix(0.60f, 1.08f, t, 0.48f, 0.55f)
                               : mix(1.08f, 1, t, 0.55f, 0.60f);
    } else if (t < 0.67f) {
        hop.forward = 1.45f;
        hop.stretch = mix(1, 0.70f, t, 0.60f, 0.67f);
    } else if (t < 0.90f) {
        const float air = (t - 0.67f) / 0.23f;
        hop.height = 0.45f * 4 * air * (1 - air);
        hop.forward = 1.45f * (1 - ease(air));
        hop.flight = 0.6f * std::sin(air * Tau * 0.5f);
        hop.stretch = t < 0.74f ? mix(0.70f, 1.17f, t, 0.67f, 0.74f)
                               : mix(1.17f, 1.02f, t, 0.74f, 0.90f);
    } else if (t < 0.94f) {
        hop.stretch = mix(1.02f, 0.72f, t, 0.90f, 0.94f);
    } else {
        hop.stretch = mix(0.72f, 1, t, 0.94f, 1);
    }
    hop.impact = pulse(t, 0.40f, 0.45f, 0.55f) + 0.5f * pulse(t, 0.90f, 0.94f, 1);
    return hop;
}

} // namespace

MonsterRig buildSlimeRig()
{
    MonsterRig rig;
    monsterPart(rig, "root", -1, {0, 0, 0}, [](AssetBuilder&) {});
    monsterPart(rig, "body", Root, {0, 0, 0}, slimeBody);
    monsterPart(rig, "face", Body, {0, 0.48f, 0.615f}, [](AssetBuilder&) {});
    monsterPart(rig, "left eye", Face, {-0.245f, 0.03f, 0}, eye);
    monsterPart(rig, "right eye", Face, {0.245f, 0.03f, 0}, eye);
    monsterPart(rig, "left pupil", LeftEye, {0, 0, 0.025f}, pupil);
    monsterPart(rig, "right pupil", RightEye, {0, 0, 0.025f}, pupil);
    monsterPart(rig, "mouth", Face, {0, -0.18f, 0.004f}, [](AssetBuilder& b) {
        b.box({-0.095f, -0.025f, 0}, {0.095f, 0.014f, 0.018f}, Block::Stone, Ink);
        b.box({-0.125f, 0.008f, 0}, {-0.085f, 0.052f, 0.018f}, Block::Stone, Ink);
        b.box({0.085f, 0.008f, 0}, {0.125f, 0.052f, 0.018f}, Block::Stone, Ink);
    });
    monsterPart(rig, "sprout", Body, {-0.17f, 0.945f, -0.025f}, [](AssetBuilder& b) {
        b.beam({0, 0, 0}, {0.015f, 0.16f, -0.025f}, 0.035f, Block::Stone, {0.37f, 0.68f, 0.15f});
    });
    monsterPart(rig, "left leaf", Sprout, {0, 0.07f, 0}, [](AssetBuilder& b) { leaf(b, -1); });
    monsterPart(rig, "right leaf", Sprout, {0.012f, 0.13f, -0.015f}, [](AssetBuilder& b) { leaf(b, 1); });
    return rig;
}

MonsterPose poseSlime(MonsterAnimation animation, float seconds)
{
    MonsterPose pose(JointCount);
    const float duration = animation == MonsterAnimation::Idle ? 3.4f
        : monsterCycleDuration(MonsterKind::Slime,animation);
    const float t = cycle(seconds, duration);
    Hop hop;
    float blink = 0;
    float glance = 0;
    if (animation == MonsterAnimation::Walk) {
        hop = walkingHop(t);
        blink = 0.64f * hop.impact;
    } else if (animation == MonsterAnimation::Attack) {
        hop = pounce(t);
        blink = 0.30f * pulse(t, 0.01f, 0.10f, 0.19f) + 0.60f * hop.impact;
    } else {
        hop.stretch = 1 + 0.035f * std::sin(Tau * t);
        glance = 0.024f * (pulse(t, 0.12f, 0.22f, 0.39f) - pulse(t, 0.68f, 0.78f, 0.94f));
        blink = pulse(t, 0.53f, 0.555f, 0.59f);
        pose[Body].rotation.y = 6.0f * std::sin(Tau * t);
    }

    // Scaling about the contact plane leaves the feet on the ground. Equal
    // horizontal expansion preserves volume through anticipation and impacts.
    const float width = 1 / std::sqrt(hop.stretch);
    pose[Root].scale = {width, hop.stretch, width};
    pose[Root].translation = {0, hop.height, hop.forward};
    pose[Face].translation.y = -0.015f * hop.impact;
    pose[LeftEye].scale.y = pose[RightEye].scale.y = 1 - 0.92f * blink;
    pose[LeftPupil].translation.x = pose[RightPupil].translation.x = glance;
    pose[LeftPupil].translation.y = pose[RightPupil].translation.y = 0.013f * hop.flight;
    pose[Mouth].scale.y = 1 + 0.8f * hop.flight;
    pose[Mouth].translation.y = -0.018f * hop.flight;

    // The little sprout trails behind each impulse and keeps moving after the
    // heavy body settles. Its motion is small enough to read at gameplay scale.
    const float sway = std::sin(Tau * t);
    const float flutter = std::sin(Tau * (2 * t - 0.12f));
    const float energy = animation == MonsterAnimation::Idle ? 0.23f : 1;
    pose[Sprout].rotation.x = energy * (12 * sway - 22 * hop.impact);
    pose[Sprout].rotation.z = 5 * sway;
    pose[LeftLeaf].rotation.z = energy * (11 * flutter + 14 * hop.impact);
    pose[RightLeaf].rotation.z = -energy * (8 * flutter + 19 * hop.impact);
    return pose;
}

} // namespace voxel
