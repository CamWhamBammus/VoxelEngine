#include "graphics/monster_builder.h"

#include <array>
#include <initializer_list>

namespace voxel {
namespace {

constexpr float Pi = 3.14159265359f;
constexpr float HipHeight = 1.0f;
constexpr float ThighLength = 0.43f;
constexpr float ShinLength = 0.44f;
constexpr float AnkleHeight = 0.145f;

enum Joint {
    Hips, Chest, Head, Jaw, Scarf, ScarfTail,
    LeftUpperArm, LeftForearm, LeftHand,
    RightUpperArm, RightForearm, RightHand, Sword,
    LeftThigh, LeftShin, LeftFoot, RightThigh, RightShin, RightFoot,
    JointCount
};

const glm::vec3 Bone{0.78f, 0.70f, 0.52f};
const glm::vec3 BoneLight{0.92f, 0.84f, 0.64f};
const glm::vec3 BoneShade{0.48f, 0.42f, 0.30f};
const glm::vec3 Socket{0.075f, 0.085f, 0.075f};
const glm::vec3 Iron{0.22f, 0.34f, 0.35f};
const glm::vec3 IronLight{0.36f, 0.49f, 0.48f};
const glm::vec3 Rust{0.45f, 0.25f, 0.13f};
const glm::vec3 Cloth{0.47f, 0.12f, 0.095f};

// An octagonal cross section gives the large bones and skull broad painted
// facets. The silhouette stays chunky without requiring a rounded mesh.
void bevelBox(AssetBuilder& b, glm::vec3 lo, glm::vec3 hi, float cut,
              glm::vec3 color)
{
    const std::array<glm::vec2, 8> ring{{
        {lo.x + cut, lo.z}, {hi.x - cut, lo.z},
        {hi.x, lo.z + cut}, {hi.x, hi.z - cut},
        {hi.x - cut, hi.z}, {lo.x + cut, hi.z},
        {lo.x, hi.z - cut}, {lo.x, lo.z + cut}
    }};
    const glm::vec3 bottom{(lo.x + hi.x) * 0.5f, lo.y, (lo.z + hi.z) * 0.5f};
    const glm::vec3 top{bottom.x, hi.y, bottom.z};
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const auto a = ring[i];
        const auto c = ring[(i + 1) % ring.size()];
        const glm::vec3 al{a.x, lo.y, a.y}, ah{a.x, hi.y, a.y};
        const glm::vec3 cl{c.x, lo.y, c.y}, ch{c.x, hi.y, c.y};
        const float facet = (i & 1) ? 0.88f : 1.0f;
        b.quad(al, ah, ch, cl, Block::Stone, color * facet);
        b.triangle(bottom, al, cl, Block::Stone, color * 0.72f);
        b.triangle(top, ch, ah, Block::Stone, color * 1.05f);
    }
}

void limb(AssetBuilder& b, float length, float width)
{
    bevelBox(b, {-width * 0.5f, -length + 0.035f, -width * 0.44f},
                {width * 0.5f, -0.035f, width * 0.44f}, width * 0.18f, Bone);
    b.box({-width * 0.7f, -0.06f, -width * 0.58f},
          {width * 0.7f, 0.035f, width * 0.58f}, Block::Stone, BoneLight);
    b.box({-width * 0.66f, -length - 0.022f, -width * 0.55f},
          {width * 0.66f, -length + 0.048f, width * 0.55f}, Block::Stone, Bone);
}

struct Key { float time; float value; };
float keyed(float time, std::initializer_list<Key> keys)
{
    auto previous = keys.begin();
    for (auto next = previous + 1; next != keys.end(); ++next) {
        if (time <= next->time) {
            const float t = ease((time - previous->time) / (next->time - previous->time));
            return previous->value + (next->value - previous->value) * t;
        }
        previous = next;
    }
    return previous->value;
}

// Sagittal two-bone IK keeps the ankle on the floor during stance. The foot
// counter-rotates at the ankle, rather than tracing the shin's arc through it.
void legPose(MonsterPose& pose, Joint thigh, Joint shin, Joint foot,
             float footZ, float lift, float footPitch = 0.0f)
{
    const float down = HipHeight + pose[Hips].translation.y - AnkleHeight - lift;
    const float forward = footZ - pose[Hips].translation.z;
    const float reach = std::clamp(std::sqrt(down * down + forward * forward),
                                  0.10f, ThighLength + ShinLength - 0.001f);
    const float hipBend = std::acos(std::clamp(
        (ThighLength * ThighLength + reach * reach - ShinLength * ShinLength)
        / (2.0f * ThighLength * reach), -1.0f, 1.0f));
    const float kneeBend = Pi - std::acos(std::clamp(
        (ThighLength * ThighLength + ShinLength * ShinLength - reach * reach)
        / (2.0f * ThighLength * ShinLength), -1.0f, 1.0f));
    pose[thigh].rotation.x = glm::degrees(std::atan2(-forward, down) - hipBend);
    pose[shin].rotation.x = glm::degrees(kneeBend);
    pose[foot].rotation.x = -pose[thigh].rotation.x - pose[shin].rotation.x + footPitch;
}

void walkLeg(MonsterPose& pose, Joint thigh, Joint shin, Joint foot, float phase)
{
    phase -= std::floor(phase);
    constexpr float Stance = 0.60f;
    constexpr float Stride = 0.50f;
    float z = Stride * 0.5f - Stride * phase / Stance;
    float lift = 0.0f;
    float pitch = 0.0f;
    if (phase > Stance) {
        const float swing = (phase - Stance) / (1.0f - Stance);
        // Match the planted foot's velocity at takeoff and touchdown. A
        // smoothstep alone visibly stalls the foot at both ends of the stride.
        const float tangent = -Stride * (1.0f - Stance) / Stance;
        z = -Stride * 0.5f + Stride * ease(swing)
            + tangent * swing * (1.0f - swing) * (1.0f - 2.0f * swing);
        const float arch = std::sin(Pi * swing);
        lift = 0.19f * arch * arch;
        pitch = -12.0f * arch * arch;
    }
    legPose(pose, thigh, shin, foot, z, lift, pitch);
}

} // namespace

MonsterRig buildSkeletonRig()
{
    MonsterRig rig;
    rig.parts.reserve(JointCount);
    monsterPart(rig, "hips", -1, {0, HipHeight, 0}, [](AssetBuilder& b) {
        bevelBox(b, {-0.22f,-0.03f,-0.105f}, {0.22f,0.105f,0.105f}, 0.045f, Bone);
        b.box({-0.235f,0.06f,-0.125f}, {0.235f,0.125f,0.12f}, Block::Stone, {0.23f,0.16f,0.105f});
        b.box({-0.07f,0.065f,0.121f}, {0.055f,0.13f,0.15f}, Block::Stone, Rust);
        b.box({-0.035f,0.082f,0.151f}, {0.025f,0.115f,0.153f}, Block::Stone, Socket);
        // Torn tabard sits between the legs; it does not hide their movement.
        b.quad({-0.115f,0.055f,0.13f},{0.105f,0.055f,0.13f},
               {0.075f,-0.18f,0.15f},{-0.07f,-0.235f,0.14f},Block::Stone,Cloth,true);
    });
    monsterPart(rig, "rib cage", Hips, {0,0.11f,0}, [](AssetBuilder& b) {
        for (int i=0; i<6; ++i) {
            const float y = 0.06f + i * 0.073f;
            b.box({-0.065f,y,-0.105f},{0.065f,y+0.046f,-0.015f},Block::Stone,
                  i % 2 ? Bone : BoneShade);
        }
        for (int i=0; i<3; ++i) {
            const float y = 0.18f + i * 0.105f;
            const float width = 0.235f + i * 0.015f;
            for (const float side : {-1.0f, 1.0f}) {
                b.beam({side*0.04f,y,-0.07f},{side*width,y+0.025f,0.01f},0.055f,Block::Stone,BoneShade);
                b.beam({side*width,y+0.025f,0.01f},{side*(width-0.025f),y-0.035f,0.135f},0.06f,Block::Stone,Bone);
                b.beam({side*(width-0.025f),y-0.035f,0.135f},{side*0.045f,y-0.055f,0.16f},0.055f,Block::Stone,BoneLight);
            }
        }
        b.box({-0.042f,0.12f,0.125f},{0.042f,0.44f,0.18f},Block::Stone,Bone);
        b.beam({-0.34f,0.465f,0.0f},{-0.035f,0.50f,0.10f},0.065f,Block::Stone,BoneLight);
        b.beam({0.035f,0.50f,0.10f},{0.34f,0.465f,0.0f},0.065f,Block::Stone,BoneLight);
        b.box({-0.055f,0.46f,-0.045f},{0.055f,0.67f,0.045f},Block::Stone,BoneShade);
        // A diagonal leather strap breaks the symmetry of the exposed ribs.
        b.beam({-0.25f,0.46f,0.19f},{0.16f,0.09f,0.18f},0.065f,Block::Stone,{0.26f,0.17f,0.10f});
    });
    monsterPart(rig, "skull", Chest, {0,0.72f,0.015f}, [](AssetBuilder& b) {
        bevelBox(b, {-0.275f,0.01f,-0.215f},{0.275f,0.405f,0.22f},0.065f,Bone);
        b.box({-0.255f,0.085f,0.221f},{0.255f,0.29f,0.235f},Block::Stone,Socket);
        // Heavy, uneven brows and cheekbones frame recessed sockets.
        b.beam({-0.255f,0.29f,0.25f},{-0.055f,0.26f,0.26f},0.065f,Block::Stone,BoneLight);
        b.beam({0.045f,0.27f,0.26f},{0.255f,0.31f,0.245f},0.07f,Block::Stone,BoneLight);
        b.box({-0.27f,0.055f,0.225f},{-0.18f,0.16f,0.255f},Block::Stone,BoneLight);
        b.box({0.18f,0.055f,0.225f},{0.27f,0.16f,0.255f},Block::Stone,Bone);
        b.box({-0.035f,0.17f,0.236f},{0.035f,0.29f,0.265f},Block::Stone,BoneLight);
        b.triangle({-0.045f,0.08f,0.257f},{0.045f,0.08f,0.257f},{0.0f,0.165f,0.257f},Block::Stone,Socket);
        b.box({-0.168f,0.172f,0.238f},{-0.105f,0.215f,0.25f},Block::Lantern,{0.25f,0.90f,0.72f});
        b.box({0.10f,0.173f,0.238f},{0.163f,0.216f,0.25f},Block::Lantern,{0.25f,0.90f,0.72f});
        b.box({-0.17f,0.02f,0.225f},{0.17f,0.065f,0.275f},Block::Stone,Bone);
        for (int i=0;i<4;++i) {
            const float x=-0.15f + i*0.085f;
            b.box({x,-0.025f,0.228f},{x+0.06f,0.035f,0.278f},Block::Stone,BoneLight);
        }
        // A broad chip and a short forked crack are visible at gameplay scale.
        b.triangle({-0.245f,0.397f,0.221f},{-0.21f,0.34f,0.221f},{-0.155f,0.397f,0.221f},Block::Stone,BoneShade);
        b.beam({0.13f,0.399f,0.221f},{0.115f,0.34f,0.224f},0.012f,Block::Stone,BoneShade);
        b.beam({0.115f,0.34f,0.224f},{0.16f,0.32f,0.226f},0.012f,Block::Stone,BoneShade);
    });
    monsterPart(rig, "jaw", Head, {0,0.015f,0.10f}, [](AssetBuilder& b) {
        bevelBox(b, {-0.225f,-0.14f,-0.04f},{0.225f,-0.07f,0.17f},0.045f,Bone);
        b.box({-0.225f,-0.10f,-0.07f},{-0.17f,0.075f,0.015f},Block::Stone,BoneShade);
        b.box({0.17f,-0.10f,-0.07f},{0.225f,0.075f,0.015f},Block::Stone,BoneShade);
        for (int i=0;i<4;++i) {
            if (i==2) continue;
            const float x=-0.15f+i*0.085f;
            b.box({x,-0.08f,0.115f},{x+0.06f,-0.035f,0.168f},Block::Stone,BoneLight);
        }
    });
    monsterPart(rig, "scarf", Chest, {0,0.62f,0}, [](AssetBuilder& b) {
        bevelBox(b, {-0.165f,-0.065f,-0.105f},{0.165f,0.04f,0.155f},0.03f,Cloth);
        b.box({0.11f,-0.06f,0.11f},{0.205f,0.04f,0.21f},Block::Stone,{0.63f,0.19f,0.12f});
        b.beam({-0.14f,0.005f,0.157f},{0.11f,-0.035f,0.164f},0.018f,Block::Stone,{0.72f,0.28f,0.16f});
    });
    monsterPart(rig, "scarf tail", Scarf, {0.15f,-0.025f,-0.08f}, [](AssetBuilder& b) {
        b.quad({-0.055f,0,0},{0.065f,0,0},{0.09f,-0.23f,-0.20f},
               {-0.055f,-0.27f,-0.18f},Block::Stone,Cloth,true);
        b.triangle({-0.055f,-0.27f,-0.18f},{0.09f,-0.23f,-0.20f},
                   {0.0f,-0.38f,-0.23f},Block::Stone,{0.35f,0.075f,0.065f},true);
    });
    monsterPart(rig, "left upper arm", Chest, {-0.355f,0.46f,0}, [](AssetBuilder& b) {
        limb(b,0.34f,0.095f);
        // One battered pauldron, with an uneven bottom edge and brass rivets.
        bevelBox(b, {-0.17f,-0.14f,-0.15f},{0.11f,0.09f,0.14f},0.045f,Iron);
        b.box({-0.18f,-0.09f,-0.12f},{-0.13f,0.04f,0.11f},Block::Stone,IronLight);
        b.triangle({-0.14f,-0.14f,0.141f},{0.10f,-0.14f,0.141f},
                   {-0.09f,-0.23f,0.13f},Block::Stone,Iron,true);
        for (const float x : {-0.11f,0.055f})
            b.box({x,-0.035f,0.14f},{x+0.025f,-0.01f,0.16f},Block::Stone,Rust);
        b.quad({-0.08f,0.02f,0.142f},{0.04f,-0.055f,0.142f},
               {0.025f,-0.07f,0.143f},{-0.095f,0.005f,0.143f},Block::Stone,IronLight);
    });
    monsterPart(rig, "left forearm", LeftUpperArm, {0,-0.34f,0}, [](AssetBuilder& b) {
        b.beam({-0.035f,0,0},{-0.025f,-0.30f,0.015f},0.055f,Block::Stone,Bone);
        b.beam({0.035f,0,0},{0.025f,-0.30f,0.015f},0.043f,Block::Stone,BoneShade);
        b.box({-0.075f,-0.035f,-0.06f},{0.075f,0.025f,0.065f},Block::Stone,BoneLight);
        b.box({-0.065f,-0.29f,-0.05f},{0.065f,-0.245f,0.07f},Block::Stone,{0.26f,0.18f,0.12f});
    });
    monsterPart(rig, "left hand", LeftForearm, {0,-0.30f,0.015f}, [](AssetBuilder& b) {
        b.box({-0.065f,-0.09f,-0.03f},{0.065f,0.01f,0.045f},Block::Stone,Bone);
        for (int i=0;i<3;++i) {
            const float x=-0.057f+i*0.042f;
            b.beam({x,-0.07f,0.01f},{x,-0.17f,0.055f},0.029f,Block::Stone,BoneLight);
            b.beam({x,-0.17f,0.055f},{x,-0.18f,0.11f},0.027f,Block::Stone,Bone);
        }
        b.beam({-0.055f,-0.035f,0.025f},{-0.10f,-0.085f,0.08f},0.035f,Block::Stone,Bone);
    });
    monsterPart(rig, "right upper arm", Chest, {0.355f,0.46f,0}, [](AssetBuilder& b) {
        limb(b,0.34f,0.095f);
        b.box({-0.067f,-0.18f,-0.06f},{0.067f,-0.12f,0.065f},Block::Stone,Cloth);
    });
    monsterPart(rig, "right forearm", RightUpperArm, {0,-0.34f,0}, [](AssetBuilder& b) {
        b.beam({-0.035f,0,0},{-0.025f,-0.30f,0.015f},0.055f,Block::Stone,Bone);
        b.beam({0.035f,0,0},{0.025f,-0.30f,0.015f},0.043f,Block::Stone,BoneShade);
        b.box({-0.075f,-0.035f,-0.06f},{0.075f,0.025f,0.065f},Block::Stone,BoneLight);
        bevelBox(b, {-0.085f,-0.29f,-0.065f},{0.085f,-0.14f,0.08f},0.025f,Iron);
        b.box({-0.085f,-0.245f,0.08f},{0.085f,-0.21f,0.09f},Block::Stone,Rust);
    });
    monsterPart(rig, "right hand", RightForearm, {0,-0.30f,0.015f}, [](AssetBuilder& b) {
        b.box({-0.065f,-0.095f,-0.03f},{0.065f,0.01f,0.035f},Block::Stone,Bone);
        for (int i=0;i<3;++i) {
            const float z=-0.01f+i*0.037f;
            b.box({-0.068f,-0.09f,z},{0.045f,-0.055f,z+0.026f},Block::Stone,BoneLight);
        }
        b.box({0.035f,-0.05f,0.02f},{0.075f,-0.005f,0.09f},Block::Stone,Bone);
    });
    monsterPart(rig, "notched sword", RightHand, {0,-0.06f,0.015f}, [](AssetBuilder& b) {
        b.box({-0.035f,-0.035f,-0.17f},{0.035f,0.035f,0.14f},Block::Stone,{0.22f,0.14f,0.09f});
        for (int i=0;i<4;++i) {
            const float z=-0.14f+i*0.06f;
            b.box({-0.041f,-0.041f,z},{0.041f,0.041f,z+0.025f},Block::Stone,Rust);
        }
        b.box({-0.06f,-0.06f,-0.21f},{0.06f,0.06f,-0.16f},Block::Stone,IronLight);
        b.beam({-0.19f,0.0f,0.17f},{0.19f,0.0f,0.12f},0.065f,Block::Stone,Iron);
        const glm::vec3 edge{0.66f,0.74f,0.71f}, steel{0.37f,0.48f,0.49f};
        // A diamond section catches a long highlight as the blade turns.
        b.quad({-0.085f,0,0.92f},{0,0.026f,0.92f},{0,0.026f,0.17f},{-0.085f,0,0.17f},Block::Stone,edge);
        b.quad({0,0.026f,0.92f},{0.085f,0,0.92f},{0.085f,0,0.17f},{0,0.026f,0.17f},Block::Stone,steel);
        b.quad({0,-0.026f,0.92f},{-0.085f,0,0.92f},{-0.085f,0,0.17f},{0,-0.026f,0.17f},Block::Stone,steel);
        b.quad({0.085f,0,0.92f},{0,-0.026f,0.92f},{0,-0.026f,0.17f},{0.085f,0,0.17f},Block::Stone,edge);
        b.triangle({-0.085f,0,0.92f},{0,0,1.10f},{0,0.026f,0.92f},Block::Stone,edge);
        b.triangle({0,0.026f,0.92f},{0,0,1.10f},{0.085f,0,0.92f},Block::Stone,steel);
        b.triangle({0,-0.026f,0.92f},{0,0,1.10f},{-0.085f,0,0.92f},Block::Stone,steel);
        b.triangle({0.085f,0,0.92f},{0,0,1.10f},{0,-0.026f,0.92f},Block::Stone,edge);
        b.quad({-0.074f,0.005f,0.34f},{-0.025f,0.02f,0.31f},
               {-0.023f,0.021f,0.33f},{-0.074f,0.006f,0.365f},Block::Stone,Rust,true);
    });
    monsterPart(rig, "left thigh", Hips, {-0.145f,0,0}, [](AssetBuilder& b) { limb(b,ThighLength,0.10f); });
    monsterPart(rig, "left shin", LeftThigh, {0,-ThighLength,0}, [](AssetBuilder& b) {
        limb(b,ShinLength,0.082f);
        bevelBox(b, {-0.085f,-0.23f,0.03f},{0.085f,-0.045f,0.095f},0.025f,Iron);
        b.box({-0.085f,-0.15f,0.096f},{0.085f,-0.12f,0.11f},Block::Stone,Rust);
    });
    const auto foot = [](AssetBuilder& b) {
        bevelBox(b, {-0.085f,-AnkleHeight,-0.09f},{0.085f,-0.045f,0.145f},0.025f,Bone);
        for (int i=0;i<3;++i) {
            const float x=-0.08f+i*0.055f;
            b.box({x,-AnkleHeight,0.12f},{x+0.045f,-0.07f,0.225f},Block::Stone,BoneLight);
        }
    };
    monsterPart(rig, "left foot", LeftShin, {0,-ShinLength,0}, foot);
    monsterPart(rig, "right thigh", Hips, {0.145f,0,0}, [](AssetBuilder& b) { limb(b,ThighLength,0.10f); });
    monsterPart(rig, "right shin", RightThigh, {0,-ThighLength,0}, [](AssetBuilder& b) { limb(b,ShinLength,0.082f); });
    monsterPart(rig, "right foot", RightShin, {0,-ShinLength,0}, foot);
    return rig;
}

MonsterPose poseSkeleton(MonsterAnimation animation, float seconds)
{
    MonsterPose pose(JointCount);
    pose[LeftUpperArm].rotation = {-7,0,-9};
    pose[LeftForearm].rotation.x = -16;
    pose[RightUpperArm].rotation = {-9,0,8};
    pose[RightForearm].rotation.x = -20;
    pose[RightHand].rotation.y = -5;
    if (animation == MonsterAnimation::Idle) {
        const float t = cycle(seconds,2.8f)*2.0f*Pi;
        pose[Hips].translation.y = -0.012f + 0.006f*std::sin(t);
        pose[Chest].rotation = {2.0f+1.2f*std::sin(t), 3.0f*std::sin(t-0.3f), -1.4f};
        pose[Head].rotation = {-3.0f+1.5f*std::sin(t-0.5f), 7.0f*std::sin(t-0.8f), 2.0f};
        pose[Jaw].rotation.x = 3.0f+2.0f*std::sin(t-0.3f);
        pose[LeftUpperArm].rotation.x += 2.0f*std::sin(t-0.3f);
        pose[RightUpperArm].rotation.x += 1.5f*std::sin(t-0.4f);
        pose[ScarfTail].rotation.x = 5.0f*std::sin(t-0.9f);
        pose[ScarfTail].rotation.z = 4.0f*std::sin(t-0.2f);
        legPose(pose,LeftThigh,LeftShin,LeftFoot,-0.035f,0);
        legPose(pose,RightThigh,RightShin,RightFoot,0.045f,0);
    } else if (animation == MonsterAnimation::Walk) {
        const float phase = cycle(seconds,monsterCycleDuration(MonsterKind::Skeleton,animation));
        const float t = phase*2.0f*Pi;
        pose[Hips].translation.y = -0.045f + 0.012f*std::cos(t*2.0f);
        pose[Chest].rotation = {5.0f, 5.5f*std::sin(t), 2.5f*std::sin(t)};
        pose[Head].rotation = {-4.0f+1.5f*std::sin(t*2.0f-0.4f), -3.5f*std::sin(t-0.15f), -1.5f*std::sin(t)};
        pose[Jaw].rotation.x = 4.0f+3.0f*std::sin(t*2.0f-0.5f);
        pose[LeftUpperArm].rotation.x = -8.0f+29.0f*std::sin(t);
        pose[RightUpperArm].rotation.x = -14.0f-19.0f*std::sin(t);
        pose[LeftForearm].rotation.x = -20.0f-12.0f*std::max(0.0f,-std::sin(t));
        pose[RightForearm].rotation.x = -26.0f+7.0f*std::sin(t-0.3f);
        pose[LeftHand].rotation.x = 6.0f*std::sin(t-0.5f);
        pose[ScarfTail].rotation = {16.0f+8.0f*std::sin(t*2.0f-0.8f),0,7.0f*std::sin(t-0.6f)};
        walkLeg(pose,LeftThigh,LeftShin,LeftFoot,phase);
        walkLeg(pose,RightThigh,RightShin,RightFoot,phase+0.5f);
    } else {
        const float t = cycle(seconds,monsterCycleDuration(MonsterKind::Skeleton,animation));
        const float prepare = pulse(t,0.04f,0.25f,0.41f);
        const float strike = pulse(t,0.28f,0.44f,0.84f);
        pose[Hips].translation.y = -0.018f-0.045f*strike;
        pose[Hips].translation.z = 0.13f*strike-0.025f*prepare;
        pose[Chest].rotation = {-9.0f*prepare+16.0f*strike, -18.0f*prepare+12.0f*strike, -3.0f*strike};
        pose[Head].rotation = {4.0f*prepare-12.0f*strike, 12.0f*prepare-8.0f*strike, 3.0f*strike};
        pose[Jaw].rotation.x = 4.0f+16.0f*pulse(t,0.18f,0.37f,0.64f);
        pose[RightUpperArm].rotation.x = keyed(t,{{0,-9},{0.07f,-9},{0.25f,-130},{0.30f,-135},{0.44f,-5},{0.54f,0},{0.91f,-9},{1,-9}});
        pose[RightUpperArm].rotation.z = 8.0f+16.0f*prepare;
        pose[RightUpperArm].rotation.y = -13.0f*prepare+8.0f*strike;
        pose[RightForearm].rotation.x = keyed(t,{{0,-20},{0.25f,-40},{0.31f,-42},{0.43f,-13},{0.60f,-19},{0.91f,-20},{1,-20}});
        pose[RightHand].rotation.z = -8.0f*prepare+6.0f*strike;
        pose[LeftUpperArm].rotation.x = -7.0f-46.0f*prepare-26.0f*strike;
        pose[LeftUpperArm].rotation.z = -9.0f-16.0f*prepare-9.0f*strike;
        pose[LeftForearm].rotation.x = -16.0f-63.0f*prepare-30.0f*strike;
        pose[LeftHand].rotation.x = -10.0f*prepare;
        pose[ScarfTail].rotation.x = -10.0f*prepare+28.0f*pulse(t,0.36f,0.51f,0.89f);
        pose[ScarfTail].rotation.z = -13.0f*prepare+16.0f*pulse(t,0.34f,0.55f,0.92f);
        const float step = pulse(t,0.20f,0.41f,0.90f);
        const float lift = 0.075f*pulse(t,0.20f,0.29f,0.40f);
        legPose(pose,LeftThigh,LeftShin,LeftFoot,-0.035f-0.045f*strike,0);
        legPose(pose,RightThigh,RightShin,RightFoot,0.045f+0.24f*step,lift);
    }
    return pose;
}

} // namespace voxel
