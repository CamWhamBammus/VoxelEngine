#include "level_hud_smoke.h"
#include "graphics/hud.h"
#include "graphics/renderer.h"
#include "game.h"
#include "input.h"
#include "level_editor.h"

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
void require(bool value,const char* message)
{
    if (!value) throw std::runtime_error(message);
}
struct State {
    std::array<GLint,13> bindings{};
    std::array<GLint,4> viewport{},scissor{};
    std::array<GLint,2> polygon{};
    std::array<GLboolean,5> enabled{};
    bool operator==(const State&) const = default;
};
State state()
{
    State result;
    constexpr std::array<GLenum,13> names{
        GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,
        GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,
        GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_ACTIVE_TEXTURE,GL_DEPTH_FUNC,
        GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING};
    for (std::size_t i=0;i<names.size();++i) glGetIntegerv(names[i],&result.bindings[i]);
    glGetIntegerv(GL_VIEWPORT,result.viewport.data()); glGetIntegerv(GL_SCISSOR_BOX,result.scissor.data());
    glGetIntegerv(GL_POLYGON_MODE,result.polygon.data());
    result.enabled={glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),glIsEnabled(GL_BLEND),glIsEnabled(GL_SCISSOR_TEST),GL_FALSE};
    glGetBooleanv(GL_DEPTH_WRITEMASK,&result.enabled[4]);
    return result;
}
using Pixels=std::vector<unsigned char>;
std::size_t difference(const Pixels& a,const Pixels& b)
{
    require(a.size()==b.size(),"Level HUD image dimensions differ");
    std::size_t changed=0;
    for (std::size_t i=0;i<a.size();i+=4)
        changed+=std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                           std::abs(int(a[i+2])-int(b[i+2]))})>20;
    return changed;
}
std::size_t occupied(const Pixels& pixels)
{
    // All fixtures clear to this color before drawing. Count only visible UI,
    // including its backing, so large idle panels cannot silently return.
    std::size_t count=0;
    for (std::size_t i=0;i<pixels.size();i+=4)
        count+=std::max({std::abs(int(pixels[i])-46),std::abs(int(pixels[i+1])-61),
                         std::abs(int(pixels[i+2])-71)})>20;
    return count;
}
}

void verifyLevelHud(int width,int height)
{
    require(width>=640 && height>=360,"Framebuffer is too small for level HUD tests");
    width=std::min(width,1280); height=std::min(height,720);
    const auto temporary=std::filesystem::temp_directory_path()/
        ("voxel-level-hud-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto sections=temporary/"sections",levels=temporary/"levels";
    World room; buildBlankSection(room);
    SectionMetadata meta; meta.tested=true;
    std::vector<std::filesystem::path> accepted;
    for (int index=0;index<8;++index) {
        meta.name="Garden route "+std::to_string(index+1);
        for (int y=9;y<13;++y) room.set({6,y,15},index%2 ? Block::Crystal : Block::Timber);
        const std::vector<HubAsset> assets=index%2
            ? std::vector<HubAsset>{{HubAssetKind::CrystalBeacon,{-3.5f,9,18.5f},0,0}}
            : std::vector<HubAsset>{{HubAssetKind::Forge,{3.5f,9,18.5f},2,0},
                                    {HubAssetKind::FernBed,{-3.5f,9,18.5f},0,1}};
        accepted.push_back(acceptSection(room,meta,assets,sections));
    }
    LevelDefinition saved; saved.name="The garden journey";
    saved.sections={accepted[0].lexically_relative(sections),accepted[1].lexically_relative(sections)};
    saveLevel(saved,sections,levels);
    saved.number=5; saved.name="The roots below"; saveLevel(saved,sections,levels);
    saved.number=3; saved.name="A winding garden";
    for (int index=2;index<9;++index) saved.sections.push_back(accepted[index%8].lexically_relative(sections));
    saveLevelDraft(saved,sections,levels);
    Options options; options.smokeTest=true; options.levelCreator=true;
    options.sectionLibrary=sections.string(); options.levelLibrary=levels.string();
    Game game(options);
    Hud hud;
    WorldRenderer renderer;
    Shader caller("#version 410 core\nvoid main(){gl_Position=vec4(0);}",
                  "#version 410 core\nout vec4 color;void main(){color=vec4(1);}");
    GLuint vao=0,buffer=0;
    glGenVertexArrays(1,&vao); glGenBuffers(1,&buffer);
    const auto cleanup=[&] {
        glUseProgram(0); glBindVertexArray(0); glBindBuffer(GL_ARRAY_BUFFER,0);
        glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&buffer);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glCullFace(GL_BACK);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); glBlendEquation(GL_FUNC_ADD); glActiveTexture(GL_TEXTURE0);
        std::error_code error; std::filesystem::remove_all(temporary,error);
    };
    try {
        const auto draw=[&](int w,int h,float scale=1.0f) {
            glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,w,h);
            glDisable(GL_SCISSOR_TEST); glClearColor(.18f,.24f,.28f,1); glClear(GL_COLOR_BUFFER_BIT);
            caller.use(); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,buffer);
            glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);
            glEnable(GL_BLEND); glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
            glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
            glEnable(GL_SCISSOR_TEST); glScissor(3,5,1,1); glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            glActiveTexture(GL_TEXTURE3);
            const auto before=state(); hud.drawLevels(game,w,h,scale);
            require(state()==before,"Level HUD changed caller OpenGL state");
            Pixels pixels(static_cast<std::size_t>(w)*h*4);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Level HUD generated an OpenGL error");
            return pixels;
        };
        const auto act=[&](int id) { InputActions input; input.active=true; input.devClick=id; game.update(input); };
        const auto scene=[&] {
            const int w=std::min(width,960),h=std::min(height,540);
            const auto read=[&] {
                Pixels result(static_cast<std::size_t>(w)*h*4);
                glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
                return result;
            };
            glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glCullFace(GL_BACK);
            glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); glActiveTexture(GL_TEXTURE0);
            const auto& assets=game.levelController().assets();
            renderer.render(game.world,game.camera,w,h,false,true,1,true,false,assets);
            const auto actualCount=renderer.triangleCount();
            const auto actual=read();
            renderer.render(game.world,game.camera,w,h,false,true,1,true,false);
            auto expected=renderer.triangleCount();
            for (const auto& asset:assets) expected+=buildHubAsset(asset).indices.size()/3;
            require(actualCount==expected,"Connected level retained assets from the previous section");
            // A renderer reused across section loads must match a fresh renderer,
            // including terrain, placed models and the local light candidates.
            WorldRenderer fresh;
            fresh.render(game.world,game.camera,w,h,false,true,1,true,false,assets);
            require(actual==read(),"Connected section render differs from a fresh world render");
            require(glGetError()==GL_NO_ERROR,"Connected section render generated an OpenGL error");
            return actual;
        };
        const auto markers=[&] {
            const int w=std::min(width,960),h=std::min(height,540);
            Overlay overlay;
            for (const auto feet:{game.levelController().meta().spawn,game.levelController().meta().finish}) {
                Camera camera;
                camera.setPosition(feet+glm::vec3(0,1.5f,6));
                const auto direction=glm::normalize(glm::vec3(0,-.3f,-6));
                const auto forward=camera.forward();
                camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(forward.z,forward.x)),
                              glm::degrees(std::asin(direction.y)-std::asin(forward.y)));
                renderer.render(game.world,camera,w,h,false,true,1,true,false,game.levelController().assets());
                Pixels before(static_cast<std::size_t>(w)*h*4),after(before.size());
                glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,before.data());
                overlay.drawSection(game.levelController().meta(),{},{},camera,w,h);
                glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,after.data());
                const auto transform=camera.projectionMatrix(float(w)/h)*camera.viewMatrix();
                const auto project=[&](glm::vec3 point) {
                    auto clip=transform*glm::vec4(point,1);
                    return glm::vec2((clip.x/clip.w*.5f+.5f)*w,(clip.y/clip.w*.5f+.5f)*h);
                };
                const auto low=project(feet+glm::vec3(-.85f,2.14f,0));
                const auto high=project(feet+glm::vec3(.85f,2.60f,0));
                std::size_t changed=0;
                for (int y=std::max(0,int(std::floor(low.y)));y<std::min(h,int(std::ceil(high.y)));++y)
                    for (int x=std::max(0,int(std::floor(low.x)));x<std::min(w,int(std::ceil(high.x)));++x) {
                        const auto p=(static_cast<std::size_t>(y)*w+x)*4;
                        changed+=std::max({std::abs(int(before[p])-int(after[p])),
                            std::abs(int(before[p+1])-int(after[p+1])),std::abs(int(before[p+2])-int(after[p+2]))})>20;
                    }
                require(changed>30,"Start or End label was not drawn above its portal");
                require(glGetError()==GL_NO_ERROR,"Start/End markers generated an OpenGL error");
            }
        };
        const auto hit=[&](float x,float y,int w,int h,float dpi) {
            const float scale=std::min({std::clamp(dpi,.5f,4.0f),w/776.0f,h/550.0f});
            return Hud::levelClick(game,w,h,dpi,x*scale,y*scale);
        };
        const auto worlds=draw(width,height);
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/776.0f,size.y/550.0f});
            const float x=(size.x/scale-620)*.5f,y=(size.y/scale-410)*.5f;
            require(hit(x+100,y+100,size.x,size.y,dpi)==LevelUiWorldBase,"Level world target disagrees with drawing");
            draw(size.x,size.y,dpi);
        }
        require(!Hud::levelClick(game,width,height,1,-1,-1),"Level menu accepts outside clicks");
        require(!Hud::levelClick(game,width,height,1,std::numeric_limits<float>::quiet_NaN(),0),"Level menu accepts invalid pointer coordinates");
        act(LevelUiWorldBase);
        require(game.levelController().screen()==LevelScreen::Library,"Level world click did not open library");
        const auto library=draw(width,height);
        require(difference(worlds,library)>2000,"Level library did not replace worlds");
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.f : 1.f;
            const float scale=std::min({dpi,size.x/776.f,size.y/550.f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            const std::array<std::pair<glm::vec2,int>,5> controls{{
                {{181,81},LevelUiNew},{{515,81},LevelUiResume},
                {{350,125},LevelUiEntryBase},{{101,453},LevelUiBack},{{558,453},LevelUiPlay}
            }};
            for (const auto& [point,id]:controls)
                require(hit(x+point.x,y+point.y,size.x,size.y,dpi)==id,
                        "Simplified level library lost a scaled action target");
            draw(size.x,size.y,dpi);
        }
        act(LevelUiResume);
        require(game.levelController().screen()==LevelScreen::Compose,"Saved composition did not resume");
        require(game.levelController().definition().sections.size()==9,"Saved composition lost repeated sections");
        const auto compose=draw(width,height);
        require(difference(library,compose)>2000,"Level composition did not replace library");
        act(LevelUiSectionNext); act(LevelUiQueueNext);
        require(game.levelController().sectionPage()==1 && game.levelController().queuePage()==1,"Composition pages did not advance");
        require(difference(compose,draw(width,height))>800,"Composition page buttons did not change rows");
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/776.0f,size.y/550.0f});
            const float x=(size.x/scale-740)*.5f,y=(size.y/scale-510)*.5f;
            require(hit(x+100,y+160,size.x,size.y,dpi)==LevelUiSectionBase+7,"Second-page accepted section target disagrees with drawing");
            require(hit(x+450,y+160,size.x,size.y,dpi)==LevelUiQueueBase+7,"Second-page queue target disagrees with drawing");
            require(hit(x+550,y+95,size.x,size.y,dpi)==LevelUiNumber,"Level number field target disagrees with drawing");
            require(hit(x+565,y+487,size.x,size.y,dpi)==LevelUiPublish,"Publish target disagrees with drawing");
            draw(size.x,size.y,dpi);
        }
        act(LevelUiSectionBase+7); act(LevelUiQueueBase+9); act(LevelUiMoveUp); act(LevelUiRemove);
        require(game.levelController().definition().sections.size()==9,"Adding, moving and removing a queued section failed");
        act(LevelUiName);
        InputActions type; type.active=true; type.devText="HUD garden route"; game.update(type);
        act(LevelUiNumber); type.devText="5"; game.update(type);
        require(game.levelController().numberText()=="5","Level number field did not accept typed input");
        draw(width,height); draw(640,360,2);
        act(LevelUiPublish);
        require(game.levelController().screen()==LevelScreen::Replace,"Existing level number did not show replacement confirmation");
        const auto replace=draw(width,height);
        require(difference(compose,replace)>2000,"Replacement confirmation was not visible");
        draw(640,360,2);
        act(LevelUiCancel); act(LevelUiPublish); act(LevelUiReplace);
        require(game.levelController().screen()==LevelScreen::Compose,"Replacement did not return to composition");
        act(LevelUiBack); act(LevelUiBack); act(LevelUiBack);
        InputActions picker; picker.active=true; picker.levelMenu=true; game.update(picker);
        act(LevelUiWorldBase);
        require(!game.levelController().creator(),"Player picker remained in creator mode");
        const auto playerLibrary=draw(width,height);
        require(difference(library,playerLibrary)>1000,"Player library did not replace creator controls");
        act(LevelUiEntryBase); // Level one is the short two-section fixture.
        require(game.levelController().screen()==LevelScreen::Play,"Published level did not launch");
        const auto firstScene=scene();
        markers();
        const auto play=draw(width,height);
        const float playScale=std::min({1.0f,width/776.0f,height/550.0f});
        require(occupied(play)<15000*playScale*playScale,
                "Playing level HUD is covering too much of the section");
        require(difference(playerLibrary,play)>2000,"Level HUD did not replace player picker");
        InputActions active; active.active=true;
        require(game.player.setFeetPosition(game.world,game.levelController().meta().finish),"Level HUD fixture cannot reach end");
        game.update(active,.1f);
        require(game.levelController().screen()==LevelScreen::Transition,"End marker did not begin transition");
        require(difference(play,draw(width,height))>2000,"Section transition did not show overlay");
        draw(640,360,2);
        // Remove the next accepted file after starting to exercise recoverable UI.
        const auto missing=accepted[1],hidden=temporary/"temporarily-hidden.vxs";
        std::filesystem::rename(missing,hidden);
        for (int step=0;step<4;++step) game.update(active,.1f);
        require(game.levelController().screen()==LevelScreen::Error,"Missing next section did not show recoverable error");
        draw(width,height); draw(640,360,2);
        std::filesystem::rename(hidden,missing); act(LevelUiRetry);
        require(game.levelController().screen()==LevelScreen::Play && game.levelController().sectionIndex()==1,"Retry did not load the next section");
        require(difference(firstScene,scene())>100,"Connected sections did not replace terrain and model lighting");
        require(game.player.setFeetPosition(game.world,game.levelController().meta().finish),"Level HUD fixture cannot reach final end");
        game.update(active,.1f);
        require(game.levelController().screen()==LevelScreen::Complete,"Final section did not complete level");
        require(difference(play,draw(width,height))>2000,"Completed level did not show completion dialog");
        draw(640,360,2);
        act(LevelUiReplay);
        require(game.levelController().sectionIndex()==0 && game.levelController().screen()==LevelScreen::Play,"Replay did not restart level");
        require(!Hud::levelClick(game,width,height,1,200,200),"Walking level HUD accepts menu clicks");
        const auto before=state(); hud.drawLevels(game,0,0);
        require(state()==before,"Minimized level HUD changed OpenGL state");
    } catch (...) { cleanup(); throw; }
    cleanup();
    std::cout << "Level HUD passed: world/level picker, composition pages, text fields, publish confirmation, visible Start/End labels, transitions, scene replacement, recovery, completion and scaled pointer targets\n";
}
}
