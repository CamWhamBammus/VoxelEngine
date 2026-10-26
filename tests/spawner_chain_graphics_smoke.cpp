#include "spawner_chain_graphics_smoke.h"
#include "graphics/renderer.h"
#include "graphics/hud.h"
#include "game.h"
#include "input.h"
#include "spawner_chains.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {
using Pixels=std::vector<unsigned char>;
void require(bool value,const char* message)
{
    if (!value) throw std::runtime_error(message);
}
Pixels readPixels(int width,int height)
{
    Pixels result(static_cast<std::size_t>(width)*height*4);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
    require(glGetError()==GL_NO_ERROR,"Spawner feedback generated an OpenGL error");
    return result;
}
std::size_t difference(const Pixels& a,const Pixels& b)
{
    std::size_t result=0;
    for (std::size_t i=0; i<a.size(); i+=4)
        result+=std::abs(int(a[i])-int(b[i]))+std::abs(int(a[i+1])-int(b[i+1]))
               +std::abs(int(a[i+2])-int(b[i+2]))>20;
    return result;
}
void aim(Camera& camera,glm::vec3 point)
{
    const auto d=glm::normalize(point-camera.position()),old=camera.forward();
    camera.rotate(glm::degrees(std::atan2(d.z,d.x)-std::atan2(old.z,old.x)),
                  glm::degrees(std::asin(d.y)-std::asin(old.y)));
}
struct OverlayState {
    std::array<GLint,5> bindings{};
    std::array<GLboolean,4> flags{};
    bool operator==(const OverlayState&) const=default;
};
OverlayState overlayState()
{
    OverlayState state;
    glGetIntegerv(GL_CURRENT_PROGRAM,&state.bindings[0]);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&state.bindings[1]);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&state.bindings[2]);
    glGetIntegerv(GL_POLYGON_MODE,&state.bindings[3]);
    state.flags={glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),glIsEnabled(GL_BLEND),GL_FALSE};
    glGetBooleanv(GL_DEPTH_WRITEMASK,&state.flags[3]);
    return state;
}
void verifyBuilder(int width,int height)
{
    const auto directory=std::filesystem::temp_directory_path()/
        ("voxel-chain-graphics-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove_all(path,error); } } cleanup{directory};
    World world; buildBlankSection(world);
    std::vector<HubAsset> assets{
        {HubAssetKind::TrialSpawner,{-3.5f,9,18.5f}},
        {HubAssetKind::TrialSpawner,{3.5f,9,18.5f}},
    };
    require(linkSpawners(assets,0,1).empty(),"Chain graphics fixture could not link its spawners");
    saveSectionDraft(world,{},assets,directory);
    Options options; options.dev=true; options.smokeTest=true; options.sectionLibrary=directory.string();
    Game game(options);
    const auto act=[&](int id) { InputActions a;a.active=true;a.devClick=id;game.update(a); };
    const auto press=[&](bool InputActions::*key) { InputActions a;a.active=true;a.*key=true;game.update(a); };
    act(SectionUiWorldBase); act(SectionUiLibraryDraftBase);
    Hud hud;
    Overlay overlay;
    game.camera.setPosition({0,13,27}); aim(game.camera,{0,10,18.5f});
    const auto draw=[&](bool links) {
        glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,width,height);
        glDepthMask(GL_TRUE); glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        glClearColor(.03f,.04f,.05f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        // Overlay must restore deliberately unusual caller state as well.
        glDisable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glDepthMask(GL_FALSE);
        const auto before=overlayState();
        if (links) overlay.drawSpawnerLinks(game.sectionEditor(),game.camera,width,height,.3f);
        require(overlayState()==before,"Spawner link overlay leaked OpenGL state");
        return readPixels(width,height);
    };
    const auto blank=draw(false);
    require(difference(blank,draw(true))>100,"Builder chain lacks readable numbered arrows");
    game.camera.setPosition(assets[0].position+glm::vec3(0,2,3));
    aim(game.camera,assets[0].position+glm::vec3(0,.6f,0));
    press(&InputActions::devPalette);
    require(game.sectionEditor().screen()==SectionEditorScreen::SpawnerSettings,"Chain fixture did not open spawner settings");
    for (const auto size : {glm::ivec2(width,height),glm::ivec2(640,360)}) {
        const float dpi=size.x==640 ? 2 : 1;
        const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
        const float x=(size.x/scale-610)*.5f,y=(size.y/scale-496)*.5f;
        const auto hit=[&](float px,float py) {
            return Hud::sectionClick(game,size.x,size.y,dpi,(x+px)*scale,(y+py)*scale);
        };
        require(hit(220,403)==SectionUiSpawnerLink,"Link next button has an incorrect scaled pointer target");
        require(hit(530,403)==SectionUiSpawnerUnlink,"Unlink button has an incorrect scaled pointer target");
        glViewport(0,0,size.x,size.y); hud.drawSection(game,size.x,size.y,dpi);
        require(glGetError()==GL_NO_ERROR,"Chain settings HUD generated an OpenGL error");
    }
    act(SectionUiSpawnerLink);
    require(game.sectionEditor().linkingSpawner()==0,"Link button did not enter source selection");
    require(difference(draw(false),draw(true))>20,"Source spawner is not highlighted during linking");
    InputActions cancel; cancel.active=true;cancel.closeMenu=true;game.update(cancel);
    act(SectionUiSpawnerUnlink); act(SectionUiSpawnerApply);
    require(game.sectionEditor().screen()==SectionEditorScreen::Build,"Saving unlink did not return to building");
    require(difference(draw(false),draw(true))==0,"Deleted link leaves stale arrows in the builder");
    press(&InputActions::devPalette); act(SectionUiSpawnerLink);
    game.camera.setPosition(assets[1].position+glm::vec3(0,2,3));
    aim(game.camera,assets[1].position+glm::vec3(0,.6f,0));
    press(&InputActions::removePressed); act(SectionUiSpawnerApply);
    require(game.sectionEditor().assets()[0].spawner.next!=0,"Picker did not restore the next-spawner link");
    require(difference(draw(false),draw(true))>20,"Restored link failed to refresh the builder overlay");
    press(&InputActions::devTest);
    require(game.sectionEditor().screen()==SectionEditorScreen::Test,"Chain fixture could not enter playtest");
    require(difference(draw(false),draw(true))==0,"Builder spawner annotations leaked into gameplay");
    glDepthMask(GL_TRUE);
}
} // namespace

void verifySpawnerChainGraphics(int width,int height)
{
    width=std::min(width,1280); height=std::min(height,720);
    verifyBuilder(width,height);
    constexpr int W=640,H=360;
    World world;
    for (int z=-7; z<=8; ++z) for (int x=-8; x<=8; ++x) world.set({x,38,z},Block::Stone);
    Camera camera; camera.setPosition({.5f,41,6.5f}); aim(camera,{.5f,40,.5f});
    WorldRenderer renderer;
    const auto draw=[&](std::span<const EncounterActivation> activations) {
        glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,W,H);
        glDepthMask(GL_TRUE); glDepthRange(0,1); glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        renderer.render(world,camera,W,H,false,true,1,false,false,{},{},{},{},{},{},{},{},true,{},activations);
        return readPixels(W,H);
    };
    const auto empty=draw({});
    const auto rebuilds=renderer.sceneRebuildCount();
    EncounterActivation pulse{{.5f,39,.5f},.08f,true};
    const auto first=draw(std::span(&pulse,1));
    require(difference(empty,first)>50,"Spawner start lacks a visible 3D pulse in a dark cave");
    pulse.age=.55f;
    require(difference(first,draw(std::span(&pulse,1)))>50,"Spawner pulse does not expand and rise over time");
    for (const float invalid : {EncounterActivation::Lifetime,-1.0f,std::numeric_limits<float>::quiet_NaN()}) {
        pulse.age=invalid;
        require(difference(empty,draw(std::span(&pulse,1)))==0,"Expired or invalid activation still renders");
    }
    require(renderer.sceneRebuildCount()==rebuilds,"Activation pulse rebuilds the static world");
    for (int y=39; y<=46; ++y) for (int x=-8; x<=8; ++x) world.set({x,y,3},Block::Stone);
    const auto wall=draw({});pulse.age=.12f;
    require(difference(wall,draw(std::span(&pulse,1)))==0,"Spawner pulse shines through solid walls");
    std::cout<<"Spawner chains passed: menu targets, numbered builder arrows, unlink, playtest isolation, pulses, expiry, depth and cache reuse\n";
}
} // namespace voxel
