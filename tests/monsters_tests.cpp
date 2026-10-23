#include "game.h"
#include "input.h"
#include <glm/geometric.hpp>
#include <cmath>
#include <iostream>
#include <limits>

using namespace voxel;
namespace {
bool finite(glm::vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
}
int main()
{
    int failures = 0;
    const auto check = [&](bool pass, const char* message) {
        if (!pass) { std::cerr << message << '\n'; ++failures; }
    };
    for (const auto kind : {MonsterKind::Skeleton,MonsterKind::Wizard,MonsterKind::Slime}) {
        const auto& rig = monsterRig(kind);
        check(rig.parts.size() >= 10, "Creature needs articulated parts");
        bool meshValid = true, poseValid = true;
        std::size_t triangles = 0;
        for (std::size_t i=0; i<rig.parts.size(); ++i) {
            const auto& part = rig.parts[i];
            meshValid &= part.parent >= -1 && part.parent < static_cast<int>(i);
            meshValid &= part.mesh.indices.size()%3 == 0;
            triangles += part.mesh.indices.size()/3;
            for (auto index : part.mesh.indices) meshValid &= index < part.mesh.vertices.size();
            for (const auto& vertex : part.mesh.vertices)
                meshValid &= finite(vertex.position) && finite(vertex.normal) && finite(vertex.color)
                    && std::abs(glm::length(vertex.normal)-1) < .001f;
        }
        check(meshValid && triangles > 100, "Creature geometry must be finite with valid indices, normals and hierarchy");
        for (const auto animation : {MonsterAnimation::Idle,MonsterAnimation::Walk,MonsterAnimation::Attack}) {
            MonsterInstance instance;
            instance.kind = kind;
            instance.animation = animation;
            const auto initial = monsterMatrices(instance);
            bool moved = false;
            for (int frame=0; frame<400; ++frame) {
                instance.seconds = frame / 50.0f;
                const auto pose = monsterPose(kind,animation,instance.seconds);
                poseValid &= pose.size() == rig.parts.size();
                for (const auto& joint : pose)
                    poseValid &= finite(joint.translation) && finite(joint.rotation) && finite(joint.scale)
                        && joint.scale.x > 0 && joint.scale.y > 0 && joint.scale.z > 0;
                const auto matrices = monsterMatrices(instance);
                for (std::size_t i=0; i<matrices.size(); ++i) {
                    for (int c=0; c<4; ++c) for (int r=0; r<4; ++r) {
                        poseValid &= std::isfinite(matrices[i][c][r]);
                        moved |= std::abs(matrices[i][c][r]-initial[i][c][r]) > .02f;
                    }
                    for (const auto& v : rig.parts[i].mesh.vertices) {
                        const auto p = glm::vec3(matrices[i] * glm::vec4(v.position,1));
                        poseValid &= finite(p) && glm::length(p) < 8;
                    }
                }
            }
            check(moved,"Idle, move and attack must each animate the rig");
        }
        check(poseValid,"Animation must remain finite, bounded and invertible across several cycles");
        // Recoil must bend away from the strike, even when the creature faces
        // another way, and return exactly to its normal pose after the hit.
        MonsterInstance hurt;
        hurt.kind = kind;
        hurt.position = {3,2,-4};
        hurt.yaw = 90;
        hurt.seconds = .37f;
        const auto resting = monsterMatrices(hurt);
        bool recoilValid = true;
        for (const auto direction : {glm::vec3(1,0,0),glm::vec3(-1,0,0),glm::vec3(0,0,1),glm::vec3(0,0,-1)}) {
            hurt.hitDirection = direction;
            for (int frame=0; frame<=30; ++frame) {
                hurt.hitReaction = 1.0f-frame/30.0f;
                const auto matrices = monsterMatrices(hurt);
                for (std::size_t i=0; i<matrices.size(); ++i) {
                    recoilValid &= std::isfinite(glm::determinant(matrices[i]))
                        && std::abs(glm::determinant(matrices[i])) > std::abs(glm::determinant(resting[i]))*.5f;
                    for (const auto& v : rig.parts[i].mesh.vertices) {
                        const auto p = glm::vec3(matrices[i]*glm::vec4(v.position,1));
                        const auto rest = glm::vec3(resting[i]*glm::vec4(v.position,1));
                        recoilValid &= finite(p) && glm::length(p-rest) < .65f;
                        if (frame == 30) recoilValid &= glm::length(p-rest) < .00001f;
                    }
                }
                if (frame == 0) {
                    // Comparing a point above the root isolates the whole-body
                    // lean from the animated joint's existing local rotation.
                    const auto recoil = matrices.front()*glm::inverse(resting.front());
                    const auto top = hurt.position+glm::vec3(0,1,0);
                    const auto shifted = glm::vec3(recoil*glm::vec4(top,1))-top;
                    check(glm::dot(shifted,direction) > .05f,"Hit recoil must lean away from the attack direction");
                    if (kind == MonsterKind::Slime)
                        check(shifted.y < -.1f,"Slime hit reaction must visibly squash its body");
                }
            }
        }
        check(recoilValid,"Recoil must stay bounded, invertible and fully recover its resting pose");
        hurt.hitReaction = std::numeric_limits<float>::quiet_NaN();
        check(monsterMatrices(hurt) == resting,"Invalid reaction timers must not corrupt a creature pose");
        hurt.hitReaction = 1;
        hurt.hitDirection = glm::vec3(std::numeric_limits<float>::infinity());
        for (const auto& matrix : monsterMatrices(hurt))
            check(std::isfinite(glm::determinant(matrix)),"Invalid hit directions must fall back to a finite recoil");
    }
    Options options;
    options.monsters = true;
    Game game(options);
    const auto equipment = game.equipment;
    check(game.monsterPreviewActive() && game.monsters().size()==3,"Direct startup opens the three creature preview");
    check(game.player.grounded(),"Preview arrival must have solid ground");
    InputActions action;
    action.active = true;
    for (int i=0; i<25; ++i) game.update(action,.02f);
    const auto before = monsterMatrices(game.monsters()[0]);
    action.selectedTool = 2;
    game.update(action,0);
    const auto after = monsterMatrices(game.monsters()[0]);
    float difference=0;
    for (std::size_t i=0;i<before.size();++i)
        for (int c=0;c<4;++c) for(int r=0;r<4;++r) difference+=std::abs(before[i][c][r]-after[i][c][r]);
    check(difference < .001f,"Changing animation must begin at the current pose without a pop");
    const float time = game.monsters()[0].seconds;
    game.update({},1);
    check(game.monsters()[0].seconds==time,"Releasing the mouse pauses the preview");
    action = {}; action.active=true; action.inventory=true; action.upgrade=true; action.place=true;
    game.update(action,.02f);
    check(!game.inventoryOpen() && game.equipment==equipment,"Preview controls cannot alter inventory or upgrades");
    action={}; action.active=true; action.interact=true;
    game.update(action,0);
    check(game.monsterPreview().isAutomatic(),"E restores automatic cycling");
    action.interact=false;
    for (int i=0;i<90;++i)game.update(action,.1f);
    check(game.monsterPreview().animation()==MonsterAnimation::Walk,"Automatic playback advances to movement");
    bool sawSlimeFinish = false;
    for (int i=0; i<1000; ++i) {
        const auto oldAnimation = game.monsters()[2].animation;
        const auto oldPose = blendedMonsterPose(game.monsters()[2]);
        game.update(action,.025f);
        if (oldAnimation==MonsterAnimation::Attack && game.monsters()[2].animation!=oldAnimation) {
            sawSlimeFinish = true;
            check(std::abs(oldPose[0].translation.y)<.03f && std::abs(oldPose[0].translation.z)<.03f,
                  "Automatic playback must let the slime land and return before ending its pounce");
        }
    }
    check(sawSlimeFinish,"Automatic preview must finish an attack cycle");
    action.returnToHub=true;
    game.update(action,0);
    check(!game.monsterPreviewActive() && game.monsters().empty() && game.player.grounded(),"B returns safely to the village");
    check(game.equipment==equipment,"Leaving the preview preserves equipment");
    action={};action.active=true;action.monsterPreview=true;
    game.update(action,0);
    check(game.monsterPreviewActive(),"F3 reopens the preview from the hub");
    const std::array<std::string_view,1> args{"--monsters"};
    check(parseOptions(args).monsters,"Creature CLI option parses");
    for (const auto flag : {"--editor","--dev","--level-creator"}) {
        bool rejected=false;
        try { const std::array<std::string_view,2> conflict{"--monsters",flag}; parseOptions(conflict); }
        catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"Conflicting startup modes must be rejected");
    }
    return failures ? 1 : 0;
}
