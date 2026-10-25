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
#include <set>
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
    std::array<GLint,4> textures{};
    std::array<GLint,4> samplers{};
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
    GLint active=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    for (int unit=0;unit<4;++unit) {
        glActiveTexture(GL_TEXTURE0+unit);glGetIntegerv(GL_TEXTURE_BINDING_2D,&result.textures[unit]);
        glGetIntegerv(GL_SAMPLER_BINDING,&result.samplers[unit]);
    }
    glActiveTexture(active);
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

void requireHelpHint(const Pixels& pixels,int width)
{
    // H HELP must fit as six readable characters even when font snapping keeps
    // small high-DPI text at one physical pixel. H... is not a useful hint.
    int letters=0;
    for (int y=0;y<40;++y) for (int x=width-90;x<width;++x) {
        const auto i=(static_cast<std::size_t>(y)*width+x)*4;
        letters+=pixels[i]>145 && pixels[i+1]>120 && pixels[i+2]>90;
    }
    require(letters>60,"Small high-DPI HUD clipped the H HELP hint");
}

void verifyChestAuthoring(int width,int height,const std::filesystem::path& library,Hud& hud)
{
    World room; buildBlankSection(room);
    const std::vector<HubAsset> chests{
        {HubAssetKind::LootChest,{-6.5f,9,18.5f},0,0,{{ItemId::CopperOre,8}}},
        {HubAssetKind::CrystalChest,{.5f,9,18.5f},0,0,{{ItemId::Amethyst,5}}},
        {HubAssetKind::IronboundChest,{7.5f,9,18.5f},0,0,{{ItemId::MithrilOre,2}}},
    };
    saveSectionDraft(room,{},chests,library);
    Options options;options.dev=true;options.smokeTest=true;options.sectionLibrary=library.string();
    Game game(options);
    const auto act=[&](int id) {InputActions a;a.active=true;a.devClick=id;game.update(a);};
    const auto press=[&](bool InputActions::*key) {InputActions a;a.active=true;a.*key=true;game.update(a);};
    const auto draw=[&](int w,int h,float dpi=1) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);
        glClearColor(.13f,.17f,.15f,1);glClear(GL_COLOR_BUFFER_BIT);
        const auto before=state();hud.drawSection(game,w,h,dpi);
        require(state()==before,"Chest contents editor changed OpenGL state");
        Pixels pixels(static_cast<std::size_t>(w)*h*4);
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        require(glGetError()==GL_NO_ERROR,"Chest contents editor generated an OpenGL error");
        return pixels;
    };
    act(SectionUiWorldBase);act(SectionUiLibraryDraftBase);
    for (std::size_t style=0;style<chests.size();++style) {
        game.camera.setPosition(chests[style].position+glm::vec3(0,2,3));
        const auto d=glm::normalize(chests[style].position+glm::vec3(0,.6f,0)-game.camera.position());
        const auto old=game.camera.forward();
        game.camera.rotate(glm::degrees(std::atan2(d.z,d.x)-std::atan2(old.z,old.x)),
                           glm::degrees(std::asin(d.y)-std::asin(old.y)));
        press(&InputActions::devPalette);
        require(game.sectionEditor().screen()==SectionEditorScreen::ChestContents
                && game.sectionEditor().editingChestKind()==chests[style].kind,
                "A placed chest style did not open its own contents editor");
        const auto before=draw(width,height);
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2 : 1;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            const auto hit=[&](float px,float py) {
                return Hud::sectionClick(game,size.x,size.y,dpi,px*scale,py*scale);
            };
            for (int slot=0;slot<ChestSlotCount;++slot)
                require(hit(x+354+(slot%5)*66,y+125+(slot/5)*66)==SectionUiChestSlotBase+slot,
                        "Chest editor slot hit area disagrees with its DPI-scaled drawing");
            for (int item=0;item<ItemCount;++item)
                require(hit(x+92+(item%2)*146,y+109+(item/2)*30)==SectionUiChestItemBase+item,
                        "Every catalog item, including the key, must be visible and clickable");
            const std::array<std::pair<glm::vec2,int>,7> controls{{
                {{341,397},SectionUiChestMinus},{{400,397},SectionUiChestQuantity},
                {{458,397},SectionUiChestPlus},{{565,397},SectionUiChestRemove},
                {{92,454},SectionUiChestClear},{{390,454},SectionUiChestCancel},
                {{572,454},SectionUiChestApply}
            }};
            for (const auto& [point,id]:controls)
                require(hit(x+point.x,y+point.y)==id,"Chest contents control disagrees with the scaled drawing");
            draw(size.x,size.y,dpi);
        }
        for (int slot=0;slot<ChestSlotCount;++slot) {
            act(SectionUiChestSlotBase+slot);act(SectionUiChestItemBase+slot%ItemCount);
        }
        act(SectionUiChestQuantity);
        InputActions quantity;quantity.active=true;quantity.devText="99";game.update(quantity);
        require(game.sectionEditor().chestSlots().back().count==99,"Typed quantity did not update the selected stack");
        require(game.sectionEditor().assets()[style].chestItems==chests[style].chestItems,
                "Chest HUD edits must remain staged until Save");
        require(difference(before,draw(width,height))>500,"Chest editor did not display the selected contents and quantity");
        draw(640,360,2);
        if (style==0) {
            act(SectionUiChestApply);
            require(game.sectionEditor().assets()[style].chestItems.size()==ChestSlotCount,
                    "Save did not preserve all twenty authored stacks");
        } else {
            act(SectionUiChestCancel);
            require(game.sectionEditor().assets()[style].chestItems==chests[style].chestItems,
                    "Cancel must leave the placed chest unchanged");
        }
    }
}
void verifySpawnerAuthoring(int width,int height,const std::filesystem::path& library,Hud& hud)
{
    World room;buildBlankSection(room);
    const HubAsset spawner{HubAssetKind::TrialSpawner,{.5f,9,18.5f}};
    saveSectionDraft(room,{},std::span(&spawner,1),library);
    Options options;options.dev=true;options.smokeTest=true;options.sectionLibrary=library.string();
    Game game(options);
    const auto act=[&](int id) {InputActions a;a.active=true;a.devClick=id;game.update(a);};
    act(SectionUiWorldBase);act(SectionUiLibraryDraftBase);
    game.camera.setPosition(spawner.position+glm::vec3(0,2,3));
    const auto direction=glm::normalize(spawner.position+glm::vec3(0,.7f,0)-game.camera.position());
    const auto previous=game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(previous.z,previous.x)),
                       glm::degrees(std::asin(direction.y)-std::asin(previous.y)));
    InputActions open;open.active=true;open.devPalette=true;game.update(open);
    require(game.sectionEditor().screen()==SectionEditorScreen::SpawnerSettings,
            "Placed spawner did not open its encounter settings");
    const auto draw=[&](int w,int h,float dpi=1) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);
        glClearColor(.13f,.17f,.15f,1);glClear(GL_COLOR_BUFFER_BIT);
        const auto before=state();hud.drawSection(game,w,h,dpi);
        require(state()==before,"Spawner editor changed OpenGL state");
        Pixels pixels(static_cast<std::size_t>(w)*h*4);
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        require(glGetError()==GL_NO_ERROR,"Spawner editor generated an OpenGL error");
        return pixels;
    };
    const auto original=draw(width,height);
    for(const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
        const float dpi=size.x==640 ? 2 : 1;
        const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
        const float x=(size.x/scale-610)*.5f,y=(size.y/scale-440)*.5f;
        const auto hit=[&](float px,float py) {
            return Hud::sectionClick(game,size.x,size.y,dpi,(x+px)*scale,(y+py)*scale);
        };
        for(int kind=0;kind<3;++kind) {
            require(hit(446,109+kind*66)==SectionUiSpawnerMinusBase+kind,
                    "Spawner minus target disagrees with its scaled display");
            require(hit(551,109+kind*66)==SectionUiSpawnerPlusBase+kind,
                    "Spawner plus target disagrees with its scaled display");
        }
        require(hit(305,335)==SectionUiSpawnerRewards && hit(100,418)==SectionUiSpawnerCancel
                && hit(460,418)==SectionUiSpawnerApply,"Spawner settings targets moved at high DPI");
        draw(size.x,size.y,dpi);
    }
    act(SectionUiSpawnerPlusBase+1);
    require(difference(original,draw(width,height))>20,"Spawner count did not visibly update");
    act(SectionUiSpawnerRewards);
    require(game.sectionEditor().screen()==SectionEditorScreen::ChestContents
            && game.sectionEditor().editingChestKind()==HubAssetKind::TrialSpawner,
            "Reward editing did not reuse the item contents screen");
    draw(width,height);draw(640,360,2);
    act(SectionUiChestItemBase+static_cast<int>(ItemId::Ruby));
    act(SectionUiChestApply);
    require(game.sectionEditor().screen()==SectionEditorScreen::SpawnerSettings,
            "Reward editing did not return to staged spawner settings");
    act(SectionUiSpawnerApply);
    require(game.sectionEditor().assets()[0].spawner.counts[1]==2
            && game.sectionEditor().assets()[0].chestItems.size()==1,
            "Spawner settings did not save enemies and rewards together");
}

void verifyCaveAuthoring(int width,int height,const std::filesystem::path& library,Hud& hud)
{
    for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
        const float dpi=size.x==640 ? 2.f : 1.f;
        const float scale=std::min({dpi,size.x/736.f,size.y/516.f});
        const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
        Options options;options.dev=true;options.smokeTest=true;
        options.sectionLibrary=(library/std::to_string(size.x)).string();
        Game game(options);
        const auto act=[&](int id) {InputActions a;a.active=true;a.devClick=id;game.update(a);};
        const auto press=[&](bool InputActions::*key) {InputActions a;a.active=true;a.*key=true;game.update(a);};
        const auto hit=[&](float px,float py) {
            return Hud::sectionClick(game,size.x,size.y,dpi,(x+px)*scale,(y+py)*scale);
        };
        const auto click=[&](float px,float py,int id) {
            const auto target=hit(px,py);
            require(target==id,"Cave authoring target disagrees with its scaled display");
            act(*target);
        };
        const auto draw=[&] {
            glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,size.x,size.y);glDisable(GL_SCISSOR_TEST);
            glClearColor(.13f,.17f,.15f,1);glClear(GL_COLOR_BUFFER_BIT);
            const auto before=state();hud.drawSection(game,size.x,size.y,dpi);
            require(state()==before,"Cave authoring HUD changed OpenGL state");
            Pixels pixels(static_cast<std::size_t>(size.x)*size.y*4);
            glReadPixels(0,0,size.x,size.y,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Cave authoring HUD generated an OpenGL error");
            return pixels;
        };
        act(SectionUiWorldBase);
        draw();
        require(hit(181,83)==SectionUiLibraryNew,"Open-room action lost its scaled library target");
        require(hit(519,83)==SectionUiLibraryCave,"New-cave action is missing from the world library");
        require(!hit(350,83),"New room and cave actions overlap in the world library");
        click(519,83,SectionUiLibraryCave);
        auto& editor=game.sectionEditor();
        require(editor.screen()==SectionEditorScreen::Build && editor.meta().lighting==SectionLighting::Dark
                && editor.meta().category==SectionCategory::Exploration && editor.builderLight(),
                "Clicking the displayed New Cave action did not open the dark Exploration template");
        draw();
        const auto originalSlot=editor.hotbar()[editor.selectedSlot()];
        press(&InputActions::devPalette);
        click(514,75,SectionUiPaletteTabBase+4);
        draw();
        constexpr std::array modes{SectionBuildMode::CarveRound,SectionBuildMode::CarveBox,
            SectionBuildMode::Select,SectionBuildMode::Brush,SectionBuildMode::Single};
        require(editor.paletteItems().size()==modes.size(),"Creative Tools palette is missing a build tool");
        for (std::size_t item=0;item<modes.size();++item) {
            require(editor.paletteItems()[item].tool==modes[item],"Tools card label points to the wrong build mode");
            require(hit(74+float(item)*110,124)==SectionUiPaletteItemBase+static_cast<int>(item),
                    "Tools card lost its original palette cell target");
        }
        click(184,124,SectionUiPaletteItemBase+1);
        require(editor.screen()==SectionEditorScreen::Build && editor.buildMode()==SectionBuildMode::CarveBox
                && editor.hotbar()[editor.selectedSlot()].name==originalSlot.name
                && !editor.hotbar()[editor.selectedSlot()].tool,
                "Choosing the Box Carve card did not activate it without replacing the hotbar item");
        draw();
        press(&InputActions::devTools);
        const std::array<std::pair<glm::vec2,int>,4> controls{{
            {{99,388},SectionUiCarveRound},{{261,388},SectionUiCarveBox},
            {{427,388},SectionUiLighting},{{597,388},SectionUiBuilderLight}
        }};
        for (const auto& [point,id]:controls)
            require(hit(point.x,point.y)==id,"Cave tool or lighting switch has an incorrect DPI-scaled target");
        const auto dark=draw();
        click(427,388,SectionUiLighting);
        require(editor.meta().lighting==SectionLighting::Ambient && !editor.meta().tested,
                "Section lighting control did not update the authored scene");
        require(difference(dark,draw())>15,"Section lighting switch did not display its changed state");
        press(&InputActions::undo);
        require(editor.meta().lighting==SectionLighting::Dark,"Undo did not restore the cave's authored darkness");
        const auto revision=editor.revision();
        const auto builderOn=draw();
        click(597,388,SectionUiBuilderLight);
        require(!editor.builderLight() && editor.revision()==revision,
                "Builder light changed the authored scene instead of only the working view");
        require(difference(builderOn,draw())>15,"Builder light switch did not display its changed state");
        click(597,388,SectionUiBuilderLight);
        click(99,388,SectionUiCarveRound);
        require(editor.screen()==SectionEditorScreen::Build && editor.buildMode()==SectionBuildMode::CarveRound,
                "Round Carve control did not return directly to the build view");
        draw();
        press(&InputActions::devTools);
        click(368,116,SectionUiBrushSizeBase+3);
        require(editor.brushSize()==9 && editor.buildMode()==SectionBuildMode::CarveRound,
                "Displayed size control replaced the selected carving tool");
        draw();
        click(261,388,SectionUiCarveBox);
        require(editor.buildMode()==SectionBuildMode::CarveBox,"Box Carve control did not retain the chosen size");
        press(&InputActions::devCancel);
        require(editor.buildMode()==SectionBuildMode::Single && !editor.toolPreview(),
                "Cancelling a carve tool left its outline or mode active");
        draw();
    }
}

void verifySectionNaming(int width,int height,const std::filesystem::path& library,Hud& hud)
{
    for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
        const float dpi=size.x==640 ? 2.f : 1.f;
        const float scale=std::min({dpi,size.x/736.f,size.y/516.f});
        Options options;options.dev=true;options.smokeTest=true;
        options.sectionLibrary=(library/std::to_string(size.x)).string();
        Game game(options);
        const auto act=[&](int id) {InputActions a;a.active=true;a.devClick=id;game.update(a);};
        const auto press=[&](bool InputActions::*key) {InputActions a;a.active=true;a.*key=true;game.update(a);};
        const auto type=[&](const std::string& text) {InputActions a;a.active=true;a.devText=text;game.update(a);};
        const auto hit=[&](float px,float py,bool rename) {
            const float x=(size.x/scale-(rename ? 620 : 700))*.5f;
            const float y=(size.y/scale-(rename ? 270 : 478))*.5f;
            return Hud::sectionClick(game,size.x,size.y,dpi,(x+px)*scale,(y+py)*scale);
        };
        const auto click=[&](float px,float py,bool rename,int id) {
            const auto target=hit(px,py,rename);
            require(target==id,"Section name control disagrees with its scaled display");
            act(*target);
        };
        const auto draw=[&] {
            glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,size.x,size.y);glDisable(GL_SCISSOR_TEST);
            glClearColor(.13f,.17f,.15f,1);glClear(GL_COLOR_BUFFER_BIT);
            const auto before=state();hud.drawSection(game,size.x,size.y,dpi);
            require(state()==before,"Section naming HUD changed OpenGL state");
            Pixels pixels(static_cast<std::size_t>(size.x)*size.y*4);
            glReadPixels(0,0,size.x,size.y,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Section naming HUD generated an OpenGL error");
            return pixels;
        };
        act(SectionUiWorldBase);act(SectionUiLibraryNew);
        auto& editor=game.sectionEditor();
        require(!editor.meta().tested,"Name editor fixture must not have completed a walk test");
        const auto originalName=editor.meta().name;
        press(&InputActions::devTools);
        const auto tools=draw();
        click(350,450,false,SectionUiRename);
        require(editor.screen()==SectionEditorScreen::Rename && editor.typing()
                && editor.renameText()==originalName && editor.nameSelected(),
                "Rename button did not open the preselected section name before a walk test");
        require(difference(tools,draw())>2000,"Name dialog did not replace the building tools panel");
        require(hit(310,106,true)==SectionUiNameField && hit(109,234,true)==SectionUiBack
                && hit(471,234,true)==SectionUiRenameApply,
                "Name dialog field, Cancel or Save targets disagree with their scaled display");
        type("   ");
        require(!hit(471,234,true),"Blank names leave the Save button clickable");
        draw();
        click(109,234,true,SectionUiBack);
        require(editor.screen()==SectionEditorScreen::Tools && editor.meta().name==originalName
                && !editor.dirty(),"Cancelling a name edit changed the document or lost the prior menu");
        click(350,450,false,SectionUiRename);
        type("Temporary");
        const auto selectedName=draw();
        click(310,106,true,SectionUiNameField);
        require(editor.nameSelected(),"Clicking the name field did not select its existing text");
        type("  Mossy Hall  ");
        require(editor.renameText()=="  Mossy Hall  " && editor.meta().name==originalName,
                "Typing in the name dialog changed the saved document before applying");
        require(difference(selectedName,draw())>15,"Name dialog did not display the newly typed name");
        click(471,234,true,SectionUiRenameApply);
        require(editor.screen()==SectionEditorScreen::Tools && editor.meta().name=="Mossy Hall"
                && !editor.dirty() && !editor.meta().tested,
                "Saving an untested section name did not trim and persist it before returning to Tools");
        const auto drafts=listSectionDrafts(options.sectionLibrary);
        require(drafts.size()==1 && drafts.front().meta.name=="Mossy Hall" && !drafts.front().meta.tested,
                "Name Save did not persist the same untested draft");
        draw();
        act(SectionUiBack);
        require(editor.screen()==SectionEditorScreen::Build,"Rename fixture could not return to building");
        press(&InputActions::devRename);
        require(editor.screen()==SectionEditorScreen::Rename && editor.renameText()=="Mossy Hall",
                "F2 naming action did not work directly from the build view");
        draw();
        type("Discard this name");
        click(109,234,true,SectionUiBack);
        require(editor.screen()==SectionEditorScreen::Build && editor.meta().name=="Mossy Hall",
                "Cancelling an F2 name edit failed to restore the build view and original name");
        draw();
    }
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
    GLuint vao=0,buffer=0,sampler=0;
    std::array<GLuint,2> textures{};
    glGenVertexArrays(1,&vao); glGenBuffers(1,&buffer);
    glGenTextures(2,textures.data());glGenSamplers(1,&sampler);
    glSamplerParameteri(sampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_LINEAR);
    const auto cleanup=[&] {
        glUseProgram(0); glBindVertexArray(0); glBindBuffer(GL_ARRAY_BUFFER,0);
        glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&buffer);
        glBindSampler(0,0);glBindSampler(3,0);glDeleteSamplers(1,&sampler);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,0);glDeleteTextures(2,textures.data());
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
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[0]);glBindSampler(0,sampler);
            glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,textures[1]);glBindSampler(3,sampler);
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
                {{128,345},SectionUiCopy},{{350,345},SectionUiPaste},{{572,345},SectionUiPasteSettings},
                {{546,226},SectionUiHeightDown},{{624,226},SectionUiHeightUp},{{128,450},SectionUiClearSelection}}};
            for (const auto& [point,id]:targets)
                require(logicalHit(x+point.x,y+point.y,size.x,size.y,dpi)==id,
                        "Building tool hit area disagrees with the scaled drawing");
            draw(size.x,size.y,dpi);
        }
        const auto tools=draw(width,height);
        act(SectionUiPasteSettings);
        require(game.sectionEditor().screen()==SectionEditorScreen::PasteSettings,"Paste settings did not open");
        require(difference(tools,draw(width,height))>2000,"Paste settings did not replace the tools panel");
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            const std::array<std::pair<glm::vec2,int>,5> targets{{
                {{183,83},SectionUiPasteModeBase},{{517,83},SectionUiPasteModeBase+1},
                {{131,219},SectionUiPasteAir},{{128,450},SectionUiBack},{{559,450},SectionUiPasteResume}}};
            for (const auto& [point,id]:targets)
                require(logicalHit(x+point.x,y+point.y,size.x,size.y,dpi)==id,
                        "Paste settings hit area disagrees with the scaled drawing");
            draw(size.x,size.y,dpi);
        }
        act(SectionUiPasteAir);
        require(game.sectionEditor().pasteIncludesAir(),"Paste-air option did not enable");
        act(SectionUiPasteAir); act(SectionUiPasteModeBase);
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
        press(&InputActions::devCancel);
        // A one-layer copy overlaps a solid floor with two holes. Its ghost
        // must draw exactly those holes, then disappear after one stamp.
        game.camera.setPosition({-3.5f,10.5f,4.5f}); press(&InputActions::devCornerA);
        game.camera.setPosition({-1.5f,10.5f,5.5f}); press(&InputActions::devCornerB);
        press(&InputActions::devTools); act(SectionUiCopy); act(SectionUiPasteSettings);
        act(SectionUiPasteModeBase+1);
        require(game.sectionEditor().pasteFillGaps() && !game.sectionEditor().pasteIncludesAir(),
                "Fill Gaps did not disable copied air");
        require(!game.sectionEditor().toolPreview(),"Fill Gaps ghost appeared before choosing a floor");
        const auto waiting=draw(width,height);
        for (int x=8;x<=10;++x) for (int z=8;z<=9;++z) game.world.set({x,9,z},Block::Clay);
        game.world.set({9,9,8},Block::Air); game.world.set({10,9,9},Block::Air);
        game.camera.setPosition({8.5f,17,14.5f}); aim({8.5f,9.99f,8.5f});
        press(&InputActions::devPasteLevel);
        require(game.sectionEditor().pasteFloorHeight()==9 && game.sectionEditor().toolPreviewValid(),
                "Fill Gaps did not lock to the floor block height");
        require(game.sectionEditor().previewBlockChanges().size()==2,"Fill Gaps preview includes existing floor blocks");
        require(difference(waiting,draw(width,height))>50,"Build HUD did not show locked floor and block count");
        press(&InputActions::devPastePin);
        require(game.sectionEditor().pastePinned(),"Paste pin did not lock the preview");
        const auto gaps=drawToolsOverlay();
        press(&InputActions::placePressed);
        require(game.sectionEditor().previewBlockChanges().empty(),"Filled holes remain in the additions preview");
        require(difference(gaps,drawToolsOverlay())>50,"Filled terrain is still drawn as a ghost addition");
        press(&InputActions::undo);
        require(game.sectionEditor().previewBlockChanges().size()==2,"Undo did not restore the hole preview");
        require(difference(gaps,drawToolsOverlay())==0,"Undo reused a stale Fill Gaps overlay mesh");
        press(&InputActions::devTools); act(SectionUiPasteSettings);
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            require(!logicalHit(x+131,y+219,size.x,size.y,dpi),"Copied-air toggle is clickable in Fill Gaps mode");
            require(logicalHit(x+559,y+450,size.x,size.y,dpi)==SectionUiPasteResume,"Resume preview target moved in Fill Gaps mode");
            draw(size.x,size.y,dpi);
        }
        act(SectionUiPasteResume);
        require(game.sectionEditor().pastePinned() && game.sectionEditor().pasteFloorHeight()==9,
                "Resuming settings discarded the pinned floor preview");
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
        require(game.sectionEditor().hotbar()[8].block==Block::AgedPlankStairs,"Last stair material cannot be equipped");
        draw(width,height); draw(640,360,2);
        act(SectionUiPaletteTabBase+1);
        const auto catalog=sectionAssetCatalog();
        const auto modelCount=std::count_if(catalog.begin(),catalog.end(),[](const auto& asset){return isCreativeAsset(asset.kind);});
        require(game.sectionEditor().paletteItems().size()==static_cast<std::size_t>(modelCount),
                "Asset palette must contain creative models without pickups or obsolete stair props");
        require(std::none_of(game.sectionEditor().paletteItems().begin(),game.sectionEditor().paletteItems().end(),
                             [](const auto& item){return item.pickup || (item.asset && !isCreativeAsset(*item.asset));}),
                "Ground pickups and obsolete stair props must stay out of Assets");
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
        for (const auto kind:{HubAssetKind::LootChest,HubAssetKind::CrystalChest,HubAssetKind::IronboundChest}) {
            act(SectionUiPaletteItemBase+static_cast<int>(kind));
            require(game.sectionEditor().hotbar()[8].asset==kind,"Each chest style must be equipable");
            draw(width,height);draw(640,360,2);
        }
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            require(logicalHit(x+22+5*110+52,y+97+27,size.x,size.y,dpi)==SectionUiPaletteItemBase+53,
                    "Third-page lush asset target disagrees with scaled drawing");
            draw(size.x,size.y,dpi);
        }
        // Every real block/model preview must remain visible inside its original
        // clickable cell, including the compact Retina layout and later pages.
        for (const int tab:{0,1}) {
            act(SectionUiPaletteTabBase+tab);
            const int count=static_cast<int>(game.sectionEditor().paletteItems().size());
            for (int page=0;page<(count+23)/24;++page) {
                act(SectionUiPaletteItemBase+page*24);
                for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
                    const float dpi=size.x==640 ? 2.0f : 1.0f;
                    const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
                    const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
                    const bool compact=scale<.9f;
                    const auto pixels=draw(size.x,size.y,dpi);
                    for (int cell=0;cell<24 && page*24+cell<count;++cell) {
                        const float left=x+22+(cell%6)*110,top=y+97+(cell/6)*60;
                        require(logicalHit(left+52,top+27,size.x,size.y,dpi)==SectionUiPaletteItemBase+page*24+cell,
                                "A textured palette entry lost its original clickable cell");
                        const float centerX=(left+(compact ? 52 : 24))*scale;
                        const float centerY=(top+(compact ? 14 : 27))*scale;
                        const float half=(compact ? 25 : 40)*scale*.44f;
                        std::set<std::array<unsigned char,3>> colors;
                        for (int py=static_cast<int>(std::ceil(centerY-half));py<centerY+half;++py)
                            for (int px=static_cast<int>(std::ceil(centerX-half));px<centerX+half;++px) {
                                const auto offset=(static_cast<std::size_t>(size.y-1-py)*size.x+px)*4;
                                colors.insert({pixels[offset],pixels[offset+1],pixels[offset+2]});
                            }
                        if (colors.size()<=3)
                            throw std::runtime_error("Blank palette thumbnail: tab "+std::to_string(tab)
                                +", entry "+std::to_string(page*24+cell)+", width "+std::to_string(size.x));
                    }
                }
            }
        }
        act(SectionUiPaletteTabBase+2); draw(width,height);
        require(game.sectionEditor().paletteItems().size()==3,"Section markers are missing from inventory");
        act(SectionUiPaletteTabBase+3);
        require(game.sectionEditor().paletteItems().size()==ItemCount,"Items palette omitted an ore, gem, or cave key");
        for (const auto& item:itemCatalog()) {
            act(SectionUiPaletteItemBase+static_cast<int>(item.id));
            const auto& equipped=game.sectionEditor().hotbar()[8];
            require(equipped.pickup==item.id && equipped.asset==HubAssetKind::ItemPickup,
                    "Every item must equip its own generic ground pickup");
        }
        act(SectionUiPickupPlus);
        require(game.sectionEditor().pickupQuantity()==2,"Pickup quantity + did not change the placed stack size");
        InputActions more;more.active=true;more.devClick=SectionUiPickupPlus;more.sprint=true;game.update(more);
        require(game.sectionEditor().pickupQuantity()==12,"Shift + did not change pickup quantity by ten");
        act(SectionUiPickupMinus);
        require(game.sectionEditor().pickupQuantity()==11,"Pickup quantity - did not change the placed stack size");
        for (const auto size:{glm::ivec2(width,height),glm::ivec2(640,360)}) {
            const float dpi=size.x==640 ? 2.0f : 1.0f;
            const float scale=std::min({dpi,size.x/736.0f,size.y/516.0f});
            const float x=(size.x/scale-700)*.5f,y=(size.y/scale-478)*.5f;
            constexpr std::array tabIds{0,1,3,2,4};
            for (int tab=0;tab<5;++tab)
                require(logicalHit(x+74+tab*110,y+75,size.x,size.y,dpi)==SectionUiPaletteTabBase+tabIds[tab],
                        "One of the five creative tabs has an incorrect DPI-scaled click target");
            require(logicalHit(x+630,y+75,size.x,size.y,dpi)==SectionUiBack,
                    "Creative Items tab overlaps the Close target");
            for (int item=0;item<ItemCount;++item)
                require(logicalHit(x+74+(item%6)*110,y+124+(item/6)*60,size.x,size.y,dpi)==SectionUiPaletteItemBase+item,
                        "Every ground pickup must have a visible, correctly scaled click target");
            require(logicalHit(x+508,y+354,size.x,size.y,dpi)==SectionUiPickupMinus
                    && logicalHit(x+662,y+354,size.x,size.y,dpi)==SectionUiPickupPlus,
                    "Pickup stack controls disagree with their DPI-scaled drawing");
            draw(size.x,size.y,dpi);
        }
        for (int step=0;step<9;++step) game.update(more);
        require(game.sectionEditor().pickupQuantity()==ChestStackLimit,"Pickup quantity exceeds the 99-item stack limit");
        draw(width,height);draw(640,360,2);
        act(SectionUiBack); act(SectionUiTest);
        require(game.sectionEditor().screen()==SectionEditorScreen::Test,"Section playtest did not start");
        const auto test=draw(width,height);
        const float testScale=std::min({1.0f,width/736.0f,height/516.0f});
        require(occupied(test)<22000*testScale*testScale,
                "F8 HUD is covering too much of the section");
        require(difference(build,test)>1000,"Playtest did not replace creative HUD");
        requireHelpHint(draw(640,360,2),640);
        const auto dryFeet=game.player.feetPosition();
        std::vector<std::pair<glm::ivec3,Block>> poolCells;
        for (int z=19;z<=21;++z) for (int y=20;y<=23;++y) for (int x=19;x<=21;++x) {
            const glm::ivec3 cell{x,y,z};
            poolCells.emplace_back(cell,game.world.get(cell));
            game.world.set(cell,Block::Water);
        }
        require(game.player.setFeetPosition(game.world,{20.5f,20.5f,20.5f}) && game.player.inWater(),
                "Section HUD fixture cannot enter water");
        require(difference(test,draw(width,height))==0,"Wet playtest added a persistent instruction panel");
        draw(640,360,2);
        for (const auto& [cell,block]:poolCells) game.world.set(cell,block);
        require(game.player.setFeetPosition(game.world,dryFeet) && !game.player.inWater(),
                "Section HUD fixture cannot leave water");
        require(difference(test,draw(width,height))==0,"Leaving water changed the quiet playtest HUD");
        require(game.player.setFeetPosition(game.world,game.sectionEditor().meta().finish),"Section HUD fixture cannot reach exit");
        InputActions active; active.active=true; game.update(active,.1f);
        require(game.sectionEditor().testComplete(),"Section HUD fixture did not finish playtest");
        require(difference(test,draw(width,height))>100,"Completed playtest did not show its completion notice");
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
        verifyChestAuthoring(width,height,temporary/"chest-workshop",hud);
        verifySpawnerAuthoring(width,height,temporary/"spawner-workshop",hud);
        verifyCaveAuthoring(width,height,temporary/"cave-workshop",hud);
        verifySectionNaming(width,height,temporary/"section-naming",hud);
    } catch (...) { cleanup(); throw; }
    cleanup();
    std::cout << "Section HUD passed: world library, cave creation, carving and lighting controls, five palette tabs, section naming before testing, build tools, volume, rotated and exact Fill Gaps previews, paste settings, all asset icons, palette pages, scaled pointer targets, playtest, acceptance and GL state\n";
}
}
