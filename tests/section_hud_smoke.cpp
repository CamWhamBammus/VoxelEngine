#include "section_hud_smoke.h"
#include "graphics/hud.h"
#include "graphics/renderer.h"
#include "game.h"
#include "input.h"
#include "section_editor.h"

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
    require(a.size()==b.size(),"Section HUD image dimensions differ");
    std::size_t changed=0;
    for (std::size_t i=0;i<a.size();i+=4)
        changed+=std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                           std::abs(int(a[i+2])-int(b[i+2]))})>20;
    return changed;
}
}

void verifySectionHud(int width,int height)
{
    require(width>=640 && height>=360,"Framebuffer is too small for section HUD tests");
    width=std::min(width,1280); height=std::min(height,720);
    const auto temporary=std::filesystem::temp_directory_path()/
        ("voxel-section-hud-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Options options; options.smokeTest=true; options.dev=true; options.sectionLibrary=temporary.string();
    Game game(options);
    Hud hud;
    Overlay overlay;
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
            const auto before=state(); hud.drawSection(game,w,h,scale);
            require(state()==before,"Section HUD changed caller OpenGL state");
            Pixels pixels(static_cast<std::size_t>(w)*h*4);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Section HUD generated an OpenGL error");
            return pixels;
        };
        const auto act=[&](int id) {
            InputActions input; input.active=true; input.devClick=id; game.update(input);
        };
        const auto press=[&](bool InputActions::* key) {
            InputActions input; input.active=true; input.*key=true; game.update(input);
        };
        const auto aim=[&](glm::vec3 target) {
            const auto direction=glm::normalize(target-game.camera.position()),old=game.camera.forward();
            game.camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                               glm::degrees(std::asin(direction.y)-std::asin(old.y)));
        };
        const auto drawToolsOverlay=[&] {
            glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,width,height);
            glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDepthMask(GL_TRUE);
            glClearColor(.015f,.025f,.035f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            caller.use(); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,buffer);
            glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);
            glEnable(GL_SCISSOR_TEST); glScissor(0,0,width,height); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            const auto before=state(); overlay.drawBuildTools(game.sectionEditor(),game.camera,width,height);
            require(state()==before,"Build volume overlay changed caller OpenGL state");
            overlay.drawBuildTools(game.sectionEditor(),game.camera,0,0);
            require(state()==before,"Minimized build overlay changed caller OpenGL state");
            Pixels pixels(static_cast<std::size_t>(width)*height*4);
            glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Build volume overlay generated an OpenGL error");
            return pixels;
        };
        const auto logicalHit=[&](float x,float y,int w,int h,float dpi) {
            const float scale=std::min({std::clamp(dpi,.5f,4.0f),w/736.0f,h/516.0f});
            return Hud::sectionClick(game,w,h,dpi,x*scale,y*scale);
        };
        require(game.devMode(),"Section HUD fixture did not enter dev mode");
        const auto worlds=draw(width,height);
        // Locate the same card at normal size and in a small Retina framebuffer.
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-620)*.5f,y=(size.y/scale-410)*.5f;
            require(logicalHit(x+100,y+100,size.x,size.y,dpi)==SectionUiWorldBase,
                    "World card hit area disagrees with the scaled drawing");
            draw(size.x,size.y,dpi);
        }
        require(!Hud::sectionClick(game,width,height,1,-1,-1),"Section UI accepts outside clicks");
        require(!Hud::sectionClick(game,width,height,1,std::numeric_limits<float>::quiet_NaN(),0),
                "Section UI accepts invalid pointer coordinates");
        act(SectionUiWorldBase);
        require(game.sectionEditor().screen()==SectionEditorScreen::Library,"World click did not open its library");
        const auto library=draw(width,height);
        require(difference(worlds,library)>2000,"Section library did not replace world cards");
        act(SectionUiLibraryNew);
        require(game.sectionEditor().screen()==SectionEditorScreen::Build,"New section did not open creative mode");
        const auto build=draw(width,height);
        press(&InputActions::devTools);
        require(game.sectionEditor().screen()==SectionEditorScreen::Tools,"Building tools menu did not open");
        require(difference(build,draw(width,height))>10000,"Building tools did not replace the creative HUD");
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            require(logicalHit(x+350,y+78,size.x,size.y,dpi)==SectionUiModeBase+1,
                    "Select-area mode target disagrees with the scaled drawing");
            require(logicalHit(x+368,y+116,size.x,size.y,dpi)==SectionUiBrushSizeBase+3,
                    "Brush size target disagrees with the scaled drawing");
            require(!logicalHit(x+128,y+283,size.x,size.y,dpi),"Fill is clickable without a selection");
            require(!logicalHit(x+350,y+345,size.x,size.y,dpi),"Paste is clickable with an empty clipboard");
            draw(size.x,size.y,dpi);
        }
        act(SectionUiBrushSizeBase+3);
        require(game.sectionEditor().brushSize()==9,"Brush-size button did not change the brush");
        act(SectionUiModeBase+1);
        require(game.sectionEditor().buildMode()==SectionBuildMode::Select && game.devFlying(),
                "Select mode did not return to flying");
        game.camera.setPosition({-3.5f,10.5f,4.5f}); press(&InputActions::devCornerA);
        game.camera.setPosition({-1.5f,11.5f,5.5f}); press(&InputActions::devCornerB);
        game.camera.setPosition({2.5f,16,12.5f}); aim({-2.5f,10.5f,4.5f});
        press(&InputActions::devTools);
        require(game.sectionEditor().buildSelection().has_value(),"Two air corners did not create a selection");
        act(SectionUiHeightUp);
        require(game.sectionEditor().buildSelection()->max.y==12,"Selection height did not grow");
        act(SectionUiHeightDown);
        require(game.sectionEditor().buildSelection()->max.y==11,"Selection height did not shrink");
        act(SectionUiFill); act(SectionUiCopy);
        require(!game.sectionEditor().clipboard().empty(),"Copy selection did not populate the clipboard");
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            const std::array<std::pair<glm::vec2,int>,9> targets{{
                {{128,283},SectionUiFill},{{350,283},SectionUiShell},{{572,283},SectionUiErase},
                {{128,345},SectionUiCopy},{{350,345},SectionUiPaste},{{572,345},SectionUiPasteAir},
                {{546,226},SectionUiHeightDown},{{624,226},SectionUiHeightUp},{{128,450},SectionUiClearSelection}}};
            for (const auto& [point,id]:targets)
                require(logicalHit(x+point.x,y+point.y,size.x,size.y,dpi)==id,
                        "Building tool hit area disagrees with the scaled drawing");
            draw(size.x,size.y,dpi);
        }
        act(SectionUiPasteAir);
        require(game.sectionEditor().pasteIncludesAir(),"Paste-air option did not enable");
        act(SectionUiPasteAir); act(SectionUiPaste);
        game.camera.setPosition({8.5f,16,14.5f}); aim({8.5f,8.5f,8.5f});
        InputActions refresh; refresh.active=true; game.update(refresh);
        require(game.sectionEditor().toolPreview() && game.sectionEditor().toolPreviewValid(),
                "Paste did not show a valid volume preview");
        const auto paste=drawToolsOverlay();
        std::size_t greenPixels=0;
        for (std::size_t i=0;i<paste.size();i+=4) greenPixels+=paste[i+1]>150 && paste[i+1]>paste[i]+40;
        require(greenPixels>50,"Valid paste volume and ghost are not visible");
        press(&InputActions::devRotate);
        require(difference(paste,drawToolsOverlay())>40,"Rotating a paste did not update its visible geometry");
        game.camera.setPosition({27.5f,15,27.5f}); aim({31.5f,8.5f,31.5f}); game.update(refresh);
        require(game.sectionEditor().toolPreview() && !game.sectionEditor().toolPreviewValid(),
                "Out-of-room paste did not become invalid");
        const auto invalid=drawToolsOverlay();
        std::size_t redPixels=0;
        for (std::size_t i=0;i<invalid.size();i+=4) redPixels+=invalid[i]>150 && invalid[i]>invalid[i+1]+50;
        require(redPixels>50,"Invalid paste preview is not visibly red");
        press(&InputActions::devCancel); press(&InputActions::devTools);
        act(SectionUiClearSelection); act(SectionUiBack);
        require(!game.sectionEditor().buildSelection(),"Clear selection left an active volume");
        InputActions inventory; inventory.active=true; inventory.devPalette=true; game.update(inventory);
        require(game.sectionEditor().screen()==SectionEditorScreen::Palette,"Creative inventory did not open");
        const auto blocks=draw(width,height);
        require(difference(build,blocks)>10000,"Creative palette did not cover the build HUD");
        act(SectionUiHotbarBase+8); act(SectionUiPaletteItemBase+18);
        require(game.sectionEditor().selectedSlot()==8 && game.sectionEditor().hotbar()[8].block==Block::Path,
                "Creative inventory did not equip the selected hotbar slot");
        act(SectionUiPaletteNext);
        require(game.sectionEditor().paletteSelection()/24==1,"New building materials cannot be reached on page two");
        act(SectionUiPaletteItemBase+static_cast<int>(PlaceableBlocks.size())-1);
        require(game.sectionEditor().hotbar()[8].block==Block::GlowMoss,"Last lush material cannot be equipped");
        draw(width,height); draw(640,360,2);
        act(SectionUiPaletteTabBase+1);
        require(game.sectionEditor().paletteItems().size()==sectionAssetCatalog().size(),"Asset palette omitted a model");
        const auto assetsFirst=draw(width,height);
        require(difference(blocks,assetsFirst)>1200,"Asset tab did not show model icons");
        // Selecting late in page one must still navigate to page two.
        act(SectionUiPaletteItemBase+17); act(SectionUiPaletteNext);
        require(game.sectionEditor().paletteSelection()/24==1,"Next inventory page stayed on page one");
        const auto assetsLast=draw(width,height);
        require(difference(assetsFirst,assetsLast)>1000,"Second asset page did not replace visible models");
        act(SectionUiPaletteItemBase+37);
        require(game.sectionEditor().hotbar()[8].asset==HubAssetKind::AnvilSign,"Existing workshop asset cannot be equipped");
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            require(logicalHit(x+22+110+52,y+97+120+27,size.x,size.y,dpi)==SectionUiPaletteItemBase+37,
                    "Second-page model hit area disagrees with the scaled drawing");
            require(logicalHit(size.x/scale*.5f+212,y+408,size.x,size.y,dpi)==SectionUiHotbarBase+8,
                    "Creative hotbar hit area disagrees with the scaled drawing");
            draw(size.x,size.y,dpi);
        }
        act(SectionUiPaletteNext);
        require(game.sectionEditor().paletteSelection()/24==2,"Lush assets cannot be reached on page three");
        require(difference(assetsLast,draw(width,height))>1000,"New lush model icons did not replace page two");
        act(SectionUiPaletteItemBase+static_cast<int>(HubAssetCount)-1);
        require(game.sectionEditor().hotbar()[8].asset==HubAssetKind::HangingPlanter,"Last lush asset cannot be equipped");
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            require(logicalHit(x+22+5*110+52,y+97+27,size.x,size.y,dpi)==SectionUiPaletteItemBase+53,
                    "Third-page lush asset target disagrees with scaled drawing");
            draw(size.x,size.y,dpi);
        }
        act(SectionUiPaletteTabBase+2); draw(width,height);
        require(game.sectionEditor().paletteItems().size()==3,"Section markers are missing from inventory");
        act(SectionUiBack); act(SectionUiTest);
        require(game.sectionEditor().screen()==SectionEditorScreen::Test,"Section playtest did not start");
        const auto test=draw(width,height);
        require(difference(build,test)>1000,"Playtest did not replace creative HUD");
        require(game.player.setFeetPosition(game.world,game.sectionEditor().meta().finish),"Section HUD fixture cannot reach exit");
        InputActions active; active.active=true; game.update(active,.1f);
        require(game.sectionEditor().testComplete(),"Section HUD fixture did not finish playtest");
        require(difference(test,draw(width,height))>1000,"Completed playtest did not show completion banner");
        act(SectionUiAccept);
        require(game.sectionEditor().screen()==SectionEditorScreen::Accept,"Acceptance dialog did not open");
        const auto accept=draw(width,height);
        act(SectionUiCategoryBase+1);
        InputActions name; name.active=true; name.devText=" HUD"; game.update(name);
        require(difference(accept,draw(width,height))>100,"Acceptance dialog did not update category and name");
        draw(640,360,2);
        act(SectionUiConfirm);
        require(game.sectionEditor().screen()==SectionEditorScreen::Library && !game.sectionEditor().entries().empty(),
                "Accepted section did not appear in the world library");
        require(difference(library,draw(width,height))>400,"Library did not show its accepted section");
        const auto before=state(); hud.drawSection(game,0,0);
        require(state()==before,"Minimized section HUD changed OpenGL state");
    } catch (...) { cleanup(); throw; }
    cleanup();
    std::cout << "Section HUD passed: world library, build tools, volume and rotated paste previews, all asset icons, palette pages, scaled pointer targets, playtest, acceptance and GL state\n";
}
}
