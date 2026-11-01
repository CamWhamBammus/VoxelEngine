#include "skill_journal_hud_smoke.h"
#include "graphics/hud.h"
#include "graphics/journal_layout.h"
#include "game.h"
#include "heartwood.h"
#include "input.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {
void require(bool pass,const char* message)
{
    if(!pass)throw std::runtime_error(message);
}
struct GlState {
    std::array<GLint,9> bindings{};
    std::array<GLint,4> viewport{};
    std::array<GLboolean,5> enabled{};
    bool operator==(const GlState&) const=default;
};
GlState state()
{
    GlState result;
    constexpr std::array<GLenum,9> names{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,
        GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,
        GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA};
    for(std::size_t i=0;i<names.size();++i)glGetIntegerv(names[i],&result.bindings[i]);
    glGetIntegerv(GL_VIEWPORT,result.viewport.data());
    result.enabled={glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),glIsEnabled(GL_BLEND),
                    glIsEnabled(GL_SCISSOR_TEST),GL_FALSE};
    glGetBooleanv(GL_DEPTH_WRITEMASK,&result.enabled[4]);
    return result;
}
using Pixels=std::vector<unsigned char>;
std::size_t difference(const Pixels& a,const Pixels& b)
{
    require(a.size()==b.size(),"Skill journal capture dimensions changed");
    std::size_t changed=0;
    for(std::size_t i=0;i<a.size();i+=4)
        changed+=std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                          std::abs(int(a[i+2])-int(b[i+2]))})>20;
    return changed;
}
}

void verifySkillJournalHud(int width,int height)
{
    width=std::min(width,1280);height=std::min(height,720);
    Options options;options.smokeTest=true;
    Game game(options);
    Hud hud;
    const auto cleanup=[] {
        glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glBlendEquation(GL_FUNC_ADD);
    };
    try {
        const auto draw=[&](int w,int h,float dpi=1.f) {
            glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);
            glDisable(GL_SCISSOR_TEST);glClearColor(.13f,.17f,.15f,1);glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glEnable(GL_BLEND);glDepthMask(GL_TRUE);
            glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
            glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
            glEnable(GL_SCISSOR_TEST);glScissor(3,5,1,1);
            const auto before=state();
            hud.drawSkills(game,w,h,dpi);
            require(state()==before,"Skill journal changed caller OpenGL state");
            Pixels pixels(static_cast<std::size_t>(w)*h*4);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Skill journal generated an OpenGL error");
            return pixels;
        };
        const auto closed=draw(width,height);
        require(!Hud::skillClick(game,width,height,1,400,200),"Closed journal accepts pointer input");
        require(game.player.setFeetPosition(game.world,heartwoodLayout().approach),
                "Skill journal fixture needs an accessible tree approach");
        game.camera.setPosition(game.player.eyePosition());
        require(game.nearSkillTree(),"Heartwood seed is not reachable for skill interaction");
        InputActions open;open.active=open.interact=true;game.update(open);
        require(game.skillsOpen(),"E did not open the skill journal");
        const auto first=draw(width,height);
        require(difference(first,closed)>10000,"Skill journal is not visible");
        const auto equipment=game.equipment;
        const auto position=game.player.feetPosition();
        for(int tab=0;tab<4;++tab)for(int selected=0;selected<6;++selected) {
            InputActions choose;choose.active=true;choose.skillTab=tab;choose.skillSelection=selected;
            game.update(choose);
            require(game.skillJournal().tab==tab&&game.skillJournal().selected==selected,
                    "Skill journal cannot select every displayed entry");
            const auto selectedImage=draw(width,height);
            if(tab!=0||selected!=0)
                require(difference(first,selectedImage)>400,"Changing skills did not change the detail page");
        }
        require(game.equipment==equipment&&game.player.feetPosition()==position,
                "Browsing placeholder skills changed gear or player position");
        const auto verifyHits=[&](int w,int h,float dpi) {
            const auto layout=journalLayout(w,h,dpi);
            const auto click=[&](JournalRect rect) {
                return Hud::skillClick(game,w,h,dpi,(rect.x+rect.width*.5f)*layout.scale,
                                                  (rect.y+rect.height*.5f)*layout.scale);
            };
            for(int tab=0;tab<4;++tab)
                require(click(layout.tab(tab))==SkillUiTabBase+tab,"Journal tab target does not follow scaled layout");
            for(int selected=0;selected<6;++selected)
                require(click(layout.card(selected))==SkillUiEntryBase+selected,"Journal card target does not follow scaled layout");
            require(click(layout.close())==SkillUiClose,"Journal close target does not follow scaled layout");
            require(!Hud::skillClick(game,w,h,dpi,0,0),"Journal accepts a click outside the book");
            draw(w,h,dpi);
        };
        verifyHits(width,height,1);verifyHits(960,540,2);verifyHits(640,360,2);
        require(!Hud::skillClick(game,width,height,1,std::numeric_limits<float>::quiet_NaN(),0),
                "Journal accepts a non-finite pointer coordinate");
        require(!Hud::skillClick(game,0,height,1,0,0),"Journal accepts an empty framebuffer");
        InputActions close;close.active=close.closeMenu=true;game.update(close);
        require(!game.skillsOpen()&&difference(closed,draw(width,height))==0,"Escape did not remove the journal");
        cleanup();
        std::cout<<"Skill journal HUD: parchment spread, 24 selectable entries, DPI pointer targets and GL state passed.\n";
    } catch(...) {cleanup();throw;}
}

} // namespace voxel
