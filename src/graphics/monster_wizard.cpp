#include "graphics/monster_builder.h"

#include <array>
#include <numbers>

namespace voxel {
namespace {

// The robe and hat use broad, bevelled panels so their shape reads in a dark cave.
constexpr glm::vec3 Indigo{0.18f, 0.20f, 0.42f};
constexpr glm::vec3 HatBlue{0.25f, 0.27f, 0.50f};
constexpr glm::vec3 Teal{0.17f, 0.39f, 0.40f};
constexpr glm::vec3 PaleTeal{0.30f, 0.56f, 0.51f};
constexpr glm::vec3 Brass{0.75f, 0.49f, 0.20f};
constexpr glm::vec3 Leather{0.23f, 0.13f, 0.12f};
constexpr glm::vec3 Skin{0.48f, 0.56f, 0.55f};
constexpr float Pi = std::numbers::pi_v<float>;

enum Joint {
    Root, Body, LeftThigh, LeftShin, LeftBoot, RightThigh, RightShin, RightBoot,
    LeftSkirt, RightSkirt, BackSkirt, Head, Hat, HatTip,
    LeftShoulder, LeftElbow, LeftHand, RightShoulder, RightElbow, RightHand,
    Staff, Pendant, Spell, MoteOne, MoteTwo, MoteThree, JointCount
};

void panel(AssetBuilder& b, glm::vec3 lower, glm::vec3 upper,
           glm::vec2 bottom, glm::vec2 top, glm::vec3 color,
           Block material = Block::Stone)
{
    constexpr std::array<glm::vec2, 8> ring{{
        {-0.65f, 1}, {0.65f, 1}, {1, 0.65f}, {1, -0.65f},
        {0.65f, -1}, {-0.65f, -1}, {-1, -0.65f}, {-1, 0.65f}}};
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const auto next = (i + 1) % ring.size();
        const auto lo = lower + glm::vec3(ring[i].x * bottom.x, 0, ring[i].y * bottom.y);
        const auto ln = lower + glm::vec3(ring[next].x * bottom.x, 0, ring[next].y * bottom.y);
        const auto hi = upper + glm::vec3(ring[i].x * top.x, 0, ring[i].y * top.y);
        const auto hn = upper + glm::vec3(ring[next].x * top.x, 0, ring[next].y * top.y);
        const float shade = i == 0 ? 1.08f : (i == 4 ? 0.85f : 0.96f);
        b.quad(lo, ln, hn, hi, material, color * shade);
        b.triangle(upper, hi, hn, material, color * 1.04f);
        b.triangle(lower, ln, lo, material, color * 0.8f);
    }
}

void gem(AssetBuilder& b, glm::vec3 at, float radius, float height,
         glm::vec3 color, Block material = Block::Crystal)
{
    const glm::vec3 tip = at + glm::vec3(radius * 0.2f, height, 0);
    const glm::vec3 foot = at - glm::vec3(0, height * 0.65f, 0);
    const glm::vec3 ring[4]{{-radius,0,radius}, {radius,0,radius},
                          {radius,0,-radius}, {-radius,0,-radius}};
    for (int i = 0; i < 4; ++i) {
        const auto a = at + ring[i], c = at + ring[(i + 1) % 4];
        b.triangle(a, c, tip, material, color * (i == 0 ? 1.0f : 0.82f));
        b.triangle(c, a, foot, material, color * 0.74f);
    }
}

void boot(AssetBuilder& b)
{
    panel(b, {0,-0.18f,0.04f}, {0,0.10f,0}, {0.14f,0.23f}, {0.12f,0.12f}, Leather);
    b.box({-0.14f,-0.22f,-0.16f},{0.14f,-0.17f,0.27f},Block::Stone,Leather * 0.6f);
    b.box({-0.127f,0.01f,0.11f},{0.127f,0.07f,0.15f},Block::Stone,Brass * 0.72f);
    b.box({-0.045f,0.017f,0.145f},{0.045f,0.064f,0.165f},Block::Stone,Brass);
}

void sleeve(AssetBuilder& b, bool forearm)
{
    const float length = forearm ? 0.26f : 0.31f;
    panel(b, {0,-length,0}, {0,0.06f,0},
          forearm ? glm::vec2(0.19f,0.17f) : glm::vec2(0.14f,0.14f),
          forearm ? glm::vec2(0.12f,0.12f) : glm::vec2(0.17f,0.16f), Indigo);
    if (forearm) {
        panel(b, {0,-length - 0.005f,0}, {0,-length + 0.06f,0},
              {0.195f,0.175f},{0.18f,0.165f},Teal);
        b.box({-0.12f,-length+0.017f,0.174f},{0.12f,-length+0.035f,0.183f},Block::Stone,Brass);
    } else {
        b.box({-0.075f,-0.09f,0.155f},{0.075f,0.035f,0.174f},Block::Stone,Teal);
    }
}

// Solve a bent knee from the foot position. During the stance half of a step,
// the sole stays level and moves back at a steady pace relative to the body.
void walkLeg(MonsterPose& pose, Joint thigh, Joint shin, Joint foot, float phase, float bob)
{
    const float u = cycle(phase, 1.0f);
    float forward, lift;
    if (u < 0.58f) {
        forward = 0.20f - 0.40f * u / 0.58f;
        lift = 0.0f;
    } else {
        const float swing = (u - 0.58f) / 0.42f;
        forward = -0.20f + 0.40f * ease(swing);
        lift = 0.16f * std::sin(Pi * swing);
    }
    constexpr float upper = 0.35f, lower = 0.35f;
    const float down = 0.665f + bob - lift;
    const float length = std::min(std::sqrt(down * down + forward * forward), upper + lower - 0.001f);
    const float knee = std::acos(std::clamp((length * length - upper * upper - lower * lower)
                                          / (2.0f * upper * lower), -1.0f, 1.0f));
    const float hip = std::atan2(-forward, down) - std::atan2(lower * std::sin(knee), upper + lower * std::cos(knee));
    pose[thigh].rotation.x = glm::degrees(hip);
    pose[shin].rotation.x = glm::degrees(knee);
    pose[foot].rotation.x = glm::degrees(-hip - knee);
}

} // namespace

MonsterRig buildWizardRig()
{
    MonsterRig rig;
    monsterPart(rig, "hips", -1, {0,0.92f,0}, [](AssetBuilder& b) {
        b.box({-0.24f,-0.10f,-0.14f},{0.24f,0.08f,0.14f},Block::Stone,Indigo);
    });
    monsterPart(rig, "robe body", Root, {0,0.09f,0}, [](AssetBuilder& b) {
        panel(b, {0,-0.07f,0}, {0,0.48f,0}, {0.27f,0.18f},{0.34f,0.21f},Indigo);
        // Broad crossed lapels and a worn belt give the front a deliberate design.
        b.beam({-0.26f,0.45f,0.20f},{0.12f,0.08f,0.21f},0.11f,Block::Stone,Teal);
        b.beam({0.26f,0.45f,0.20f},{-0.10f,0.08f,0.215f},0.11f,Block::Stone,PaleTeal * 0.8f);
        panel(b, {0,-0.01f,0}, {0,0.085f,0}, {0.282f,0.19f},{0.285f,0.20f},Leather);
        b.box({-0.09f,0.006f,0.195f},{0.09f,0.079f,0.231f},Block::Stone,Brass);
        b.box({-0.055f,0.024f,0.229f},{0.055f,0.062f,0.243f},Block::Stone,Leather);
        b.box({0.19f,-0.11f,0.13f},{0.33f,0.10f,0.285f},Block::Stone,Leather * 1.2f);
        b.box({0.21f,0.04f,0.284f},{0.31f,0.10f,0.30f},Block::Stone,Brass * 0.8f);
        panel(b, {0,0.43f,0}, {0,0.56f,0}, {0.26f,0.20f},{0.23f,0.18f},Teal);
    });
    for (int side : {-1, 1}) {
        const bool left = side < 0;
        monsterPart(rig, left ? "left thigh" : "right thigh", Root, {side * 0.16f,0,0}, [](AssetBuilder& b) {
            b.box({-0.10f,-0.35f,-0.10f},{0.10f,0.02f,0.10f},Block::Stone,Indigo * 0.72f);
        });
        const int thigh = static_cast<int>(rig.parts.size()) - 1;
        monsterPart(rig, left ? "left knee" : "right knee", thigh, {0,-0.35f,0}, [](AssetBuilder& b) {
            b.box({-0.09f,-0.35f,-0.09f},{0.09f,0.025f,0.09f},Block::Stone,Teal * 0.64f);
        });
        const int shin = static_cast<int>(rig.parts.size()) - 1;
        monsterPart(rig, left ? "left boot" : "right boot", shin, {0,-0.35f,0}, boot);
    }
    for (int side : {-1, 1}) {
        monsterPart(rig, side < 0 ? "left robe hem" : "right robe hem", Root, {side * 0.14f,0.05f,0.075f}, [side](AssetBuilder& b) {
            panel(b, {side * 0.055f,-0.53f,0.04f}, {0,0,0},
                  {0.225f,0.165f},{0.14f,0.145f}, side < 0 ? Teal : Indigo * 1.14f);
            b.box({-0.14f,-0.50f,0.204f},{0.14f,-0.456f,0.221f},Block::Stone,Brass * 0.78f);
            // A few broad stitches, not noisy texture, make the hem feel repaired.
            for (int i = 0; i < 3; ++i)
                b.box({side * 0.095f,-0.32f + i * 0.065f,0.178f},
                      {side * 0.095f + 0.05f,-0.302f + i * 0.065f,0.196f},Block::Stone,PaleTeal);
        });
    }
    monsterPart(rig, "back robe tail", Root, {0,0.08f,-0.14f}, [](AssetBuilder& b) {
        panel(b, {0,-0.57f,-0.06f}, {0,0,0}, {0.34f,0.08f},{0.24f,0.075f},Indigo * 0.88f);
        b.box({-0.25f,-0.55f,-0.147f},{0.25f,-0.51f,-0.135f},Block::Stone,Brass * 0.65f);
    });
    monsterPart(rig, "head", Body, {0,0.71f,0}, [](AssetBuilder& b) {
        panel(b, {0,-0.17f,0}, {0,0.22f,0}, {0.22f,0.17f},{0.26f,0.21f},Indigo * 0.66f);
        // The face stays dark under the brim, with pale cheek planes and amber eyes.
        b.box({-0.185f,-0.105f,0.175f},{0.185f,0.13f,0.221f},Block::Stone,{0.105f,0.145f,0.17f});
        b.box({-0.173f,-0.105f,0.22f},{-0.112f,-0.055f,0.241f},Block::Stone,Skin * 0.63f);
        b.box({0.112f,-0.105f,0.22f},{0.173f,-0.055f,0.241f},Block::Stone,Skin * 0.63f);
        b.box({-0.148f,0.005f,0.225f},{-0.057f,0.045f,0.244f},Block::Lantern,{1,0.60f,0.20f});
        b.box({0.057f,0.005f,0.225f},{0.148f,0.045f,0.244f},Block::Lantern,{1,0.60f,0.20f});
        b.box({-0.161f,0.053f,0.23f},{-0.045f,0.075f,0.253f},Block::Stone,Skin * 0.54f);
        b.box({0.045f,0.053f,0.23f},{0.161f,0.075f,0.253f},Block::Stone,Skin * 0.54f);
        b.box({-0.042f,-0.10f,0.22f},{0.042f,0.037f,0.305f},Block::Stone,Skin * 0.83f);
        b.box({-0.10f,-0.143f,0.198f},{0.10f,-0.106f,0.235f},Block::Stone,Teal);
    });
    monsterPart(rig, "hat", Head, {0,0.19f,0}, [](AssetBuilder& b) {
        panel(b, {0,-0.025f,0}, {0,0.025f,0}, {0.56f,0.42f},{0.54f,0.41f},HatBlue);
        panel(b, {0,0.025f,0}, {-0.055f,0.29f,-0.025f}, {0.32f,0.26f},{0.23f,0.19f},HatBlue);
        panel(b, {0,0.025f,0}, {-0.01f,0.095f,0}, {0.327f,0.267f},{0.305f,0.25f},Teal);
        b.box({-0.07f,0.035f,0.255f},{0.07f,0.09f,0.28f},Block::Stone,Brass);
        // A copper clasp and one broad patch keep the hat asymmetrical.
        b.box({0.17f,0.125f,0.174f},{0.25f,0.22f,0.192f},Block::Stone,Teal * 0.8f);
    });
    monsterPart(rig, "crooked hat tip", Hat, {-0.055f,0.29f,-0.025f}, [](AssetBuilder& b) {
        panel(b, {0,-0.085f,0}, {-0.11f,0.24f,-0.015f}, {0.23f,0.19f},{0.125f,0.12f},HatBlue * 0.96f);
        panel(b, {-0.11f,0.24f,-0.015f}, {-0.29f,0.34f,0.015f}, {0.125f,0.12f},{0.04f,0.055f},HatBlue * 1.06f);
        b.box({-0.325f,0.297f,-0.03f},{-0.255f,0.355f,0.06f},Block::Stone,Teal);
    });
    for (int side : {-1, 1}) {
        const bool left = side < 0;
        monsterPart(rig, left ? "left shoulder" : "right shoulder", Body, {side * 0.35f,0.45f,0}, [](AssetBuilder& b) { sleeve(b, false); });
        const int shoulder = static_cast<int>(rig.parts.size()) - 1;
        monsterPart(rig, left ? "left elbow" : "right elbow", shoulder, {0,-0.31f,0}, [](AssetBuilder& b) { sleeve(b, true); });
        const int elbow = static_cast<int>(rig.parts.size()) - 1;
        monsterPart(rig, left ? "left hand" : "right hand", elbow, {0,-0.29f,0}, [side](AssetBuilder& b) {
            b.box({-0.075f,-0.10f,-0.063f},{0.075f,0.055f,0.07f},Block::Stone,Skin);
            b.box({side < 0 ? 0.045f : -0.095f,-0.064f,0.04f},
                  {side < 0 ? 0.095f : -0.045f,0.013f,0.10f},Block::Stone,Skin * 0.9f);
            b.box({-0.06f,-0.103f,0.035f},{0.06f,-0.076f,0.082f},Block::Stone,Skin * 0.82f);
        });
    }
    monsterPart(rig, "crystal staff", RightHand, {0,0,0.055f}, [](AssetBuilder& b) {
        b.beam({0,-0.82f,0},{0,0.67f,0},0.075f,Block::Stone,{0.36f,0.22f,0.16f});
        b.beam({-0.012f,-0.40f,0.036f},{0.005f,0.32f,0.036f},0.019f,Block::Stone,{0.48f,0.32f,0.20f});
        b.box({-0.058f,-0.79f,-0.052f},{0.058f,-0.67f,0.052f},Block::Stone,Brass * 0.75f);
        for (int i = 0; i < 3; ++i)
            b.box({-0.045f,-0.02f + i * 0.055f,-0.045f},{0.045f,0.004f + i * 0.055f,0.045f},Block::Stone,Leather);
        b.beam({0,0.54f,0},{-0.18f,0.76f,0},0.09f,Block::Stone,{0.40f,0.25f,0.16f});
        b.beam({-0.18f,0.76f,0},{-0.13f,0.92f,0},0.08f,Block::Stone,{0.44f,0.29f,0.19f});
        b.beam({0,0.54f,0},{0.16f,0.79f,0},0.08f,Block::Stone,{0.40f,0.25f,0.16f});
        b.box({-0.067f,0.55f,-0.064f},{0.067f,0.61f,0.064f},Block::Stone,Brass);
        gem(b, {0,0.84f,0}, 0.105f,0.24f,{0.54f,0.96f,0.86f});
    });
    monsterPart(rig, "staff charm", Staff, {-0.16f,0.75f,0}, [](AssetBuilder& b) {
        b.beam({0,0,0},{0,-0.23f,0},0.018f,Block::Stone,Brass);
        gem(b, {0,-0.28f,0},0.045f,0.08f,Brass,Block::Stone);
    });
    monsterPart(rig, "cast crystal", LeftHand, {0,-0.25f,0.02f}, [](AssetBuilder& b) {
        gem(b, {0,0,0},0.15f,0.22f,{0.67f,1.0f,0.87f},Block::Lantern);
    });
    for (int i = 0; i < 3; ++i) {
        constexpr std::array<std::string_view,3> names{"first spell mote","second spell mote","third spell mote"};
        monsterPart(rig, names[static_cast<std::size_t>(i)], Spell, {0,0,0}, [](AssetBuilder& b) {
            gem(b, {0,0,0},0.045f,0.07f,{0.32f,0.85f,0.72f});
        });
    }
    return rig;
}

MonsterPose poseWizard(MonsterAnimation animation, float seconds)
{
    MonsterPose pose(JointCount);
    const float breath = std::sin(seconds * 1.9f);
    pose[Body].rotation = {1.0f + breath * 0.7f, 0, -0.8f};
    pose[Body].translation.y = breath * 0.009f;
    pose[Head].rotation = {-2.0f, std::sin(seconds * 0.65f) * 4.0f, 1.2f};
    pose[HatTip].rotation.z = std::sin(seconds * 1.9f - 0.8f) * 1.2f;
    pose[LeftShoulder].rotation = {-7.0f,0,-13.0f};
    pose[LeftElbow].rotation.x = -18.0f;
    pose[RightShoulder].rotation = {-8.0f,0,18.0f};
    pose[RightElbow].rotation = {-10.0f,0,-8.0f};
    pose[Staff].rotation = {18.0f,0,-10.0f};
    pose[Pendant].rotation = {std::sin(seconds * 2.3f) * 3.0f,0,std::sin(seconds * 1.7f) * 3.0f};
    pose[Spell].scale = glm::vec3(0.001f);
    pose[MoteOne].scale = pose[MoteTwo].scale = pose[MoteThree].scale = glm::vec3(0.001f);

    if (animation == MonsterAnimation::Walk) {
        const float step = cycle(seconds,monsterCycleDuration(MonsterKind::Wizard,animation));
        const float swing = std::sin(step * 2.0f * Pi);
        const float bob = -0.012f + 0.012f * std::cos(step * 4.0f * Pi);
        // The short bent-knee stride leaves room beneath the split robe hem.
        pose[Root].translation.y = bob - 0.035f;
        pose[Root].rotation.y = swing * 2.0f;
        walkLeg(pose, LeftThigh, LeftShin, LeftBoot, step, bob);
        walkLeg(pose, RightThigh, RightShin, RightBoot, step + 0.5f, bob);
        pose[Body].rotation = {3.0f, -swing * 4.5f, std::cos(step * 2.0f * Pi) * 1.5f};
        pose[Head].rotation = {-3.0f,-pose[Body].rotation.y * 0.7f,0};
        pose[LeftShoulder].rotation.x = -7.0f + swing * 15.0f;
        pose[LeftElbow].rotation.x = -18.0f - std::max(0.0f,-swing) * 10.0f;
        pose[RightShoulder].rotation.x = -11.0f - swing * 5.0f;
        pose[Staff].rotation.x = 23.0f + swing * 4.0f;
        pose[Staff].translation.y = 0.06f;
        pose[LeftSkirt].rotation.x = -4.0f + std::sin(step * 2.0f * Pi - 0.45f) * 8.0f;
        pose[RightSkirt].rotation.x = -4.0f - std::sin(step * 2.0f * Pi - 0.45f) * 8.0f;
        pose[BackSkirt].rotation.x = 5.0f + std::cos(step * 4.0f * Pi - 0.5f) * 2.0f;
        pose[HatTip].rotation = {std::cos(step * 4.0f * Pi - 0.7f) * 2.0f,0,
                               std::sin(step * 2.0f * Pi - 0.8f) * 4.0f};
        pose[Pendant].rotation.x = std::sin(step * 2.0f * Pi - 1.0f) * 12.0f;
    } else if (animation == MonsterAnimation::Attack) {
        const float t = cycle(seconds,monsterCycleDuration(MonsterKind::Wizard,animation));
        // Hold the gathered pose through the charge, then drive straight into the release.
        const float prepare = ramp(t,0.04f,0.25f) * (1.0f - ramp(t,0.44f,0.53f));
        const float charge = pulse(t,0.16f,0.40f,0.52f);
        const float cast = pulse(t,0.44f,0.51f,0.80f);
        const float recoil = pulse(t,0.51f,0.56f,0.71f);
        pose[Root].translation.y = -prepare * 0.035f;
        const float crouch = glm::degrees(std::acos(std::clamp((0.70f - prepare * 0.035f) / 0.70f, 0.0f, 1.0f)));
        for (const auto leg : {std::array<Joint,3>{LeftThigh,LeftShin,LeftBoot},
                               std::array<Joint,3>{RightThigh,RightShin,RightBoot}}) {
            pose[leg[0]].rotation.x = -crouch;
            pose[leg[1]].rotation.x = crouch * 2.0f;
            pose[leg[2]].rotation.x = -crouch;
        }
        pose[Body].rotation = {-prepare * 7.0f + cast * 14.0f - recoil * 4.0f,
                              prepare * 12.0f - cast * 15.0f,-prepare * 3.0f};
        pose[Head].rotation = {-2.0f - prepare * 3.0f - cast * 10.0f,
                              -pose[Body].rotation.y * 0.7f,prepare * 3.0f};
        pose[LeftShoulder].rotation = {-7.0f - prepare * 35.0f - cast * 74.0f,
                                      -prepare * 12.0f,-13.0f - prepare * 25.0f + cast * 18.0f};
        pose[LeftElbow].rotation.x = -18.0f - prepare * 72.0f + cast * 13.0f;
        pose[LeftHand].rotation = {prepare * 12.0f,0,prepare * 15.0f};
        pose[RightShoulder].rotation.x = -8.0f - prepare * 30.0f - cast * 10.0f;
        pose[RightShoulder].rotation.z = 18.0f + prepare * 9.0f;
        pose[RightElbow].rotation.x = -10.0f - prepare * 20.0f;
        pose[Staff].rotation = {18.0f + prepare * 42.0f + cast * 7.0f,0,-10.0f - prepare * 8.0f};
        pose[LeftSkirt].rotation = {-prepare * 4.0f - recoil * 7.0f,0,-prepare * 3.0f};
        pose[RightSkirt].rotation = {prepare * 3.0f - recoil * 6.0f,0,prepare * 3.0f};
        pose[BackSkirt].rotation.x = prepare * 5.0f - recoil * 10.0f;
        pose[HatTip].rotation = {pulse(t,0.51f,0.60f,0.85f) * 15.0f - prepare * 5.0f,0,
                                pulse(t,0.51f,0.64f,0.91f) * -9.0f};
        pose[Pendant].rotation.x = prepare * -12.0f + pulse(t,0.48f,0.60f,0.90f) * 27.0f;
        const float release = ramp(t,0.50f,0.65f);
        const float visibility = std::max(charge, pulse(t,0.46f,0.52f,0.66f));
        pose[Spell].scale = glm::vec3(0.001f + visibility * (0.7f + charge * 0.3f));
        // Keep the released spell travelling forward as the casting arm recoils.
        const glm::quat handRotation = glm::quat(glm::radians(pose[Root].rotation))
            * glm::quat(glm::radians(pose[Body].rotation))
            * glm::quat(glm::radians(pose[LeftShoulder].rotation))
            * glm::quat(glm::radians(pose[LeftElbow].rotation))
            * glm::quat(glm::radians(pose[LeftHand].rotation));
        if (visibility > 0.0f)
            pose[Spell].translation = glm::inverse(handRotation) * glm::vec3(0,0.04f * release,1.65f * release);
        pose[Spell].rotation.y = t * 720.0f;
        for (int i = 0; i < 3; ++i) {
            const float angle = seconds * 5.0f + static_cast<float>(i) * 2.0f * Pi / 3.0f;
            auto& mote = pose[MoteOne + i];
            mote.translation = {std::cos(angle) * 0.31f, std::sin(angle) * 0.31f, std::cos(angle * 0.7f) * 0.09f};
            mote.scale = glm::vec3(0.001f + visibility);
        }
    }
    return pose;
}

} // namespace voxel
