#include "graphics_menu_smoke.h"
#include "graphics/graphics_menu_layout.h"
#include "graphics/hud.h"
#include "pause_menu.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {
void require(bool value,const char* message)
{
    if(!value) throw std::runtime_error(message);
}

struct State {
    std::array<GLint,14> bindings{};
    std::array<GLint,4> viewport{},scissor{};
    std::array<GLint,2> polygon{};
    std::array<GLboolean,5> enabled{};
    bool operator==(const State&) const=default;
};
State state()
{
    State result;
    constexpr std::array<GLenum,14> names{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,
        GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,
        GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_ACTIVE_TEXTURE,GL_DEPTH_FUNC,
        GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_TEXTURE_BINDING_2D};
    for(std::size_t i=0;i<names.size();++i) glGetIntegerv(names[i],&result.bindings[i]);
    glGetIntegerv(GL_VIEWPORT,result.viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX,result.scissor.data());
    glGetIntegerv(GL_POLYGON_MODE,result.polygon.data());
    result.enabled={glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),glIsEnabled(GL_BLEND),
                    glIsEnabled(GL_SCISSOR_TEST),GL_FALSE};
    glGetBooleanv(GL_DEPTH_WRITEMASK,&result.enabled[4]);
    return result;
}

using Pixels=std::vector<unsigned char>;
struct Target {
    GLuint framebuffer=0,color=0;
    int width,height;
    Target(int w,int h):width(w),height(h)
    {
        glGenFramebuffers(1,&framebuffer);glGenTextures(1,&color);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glBindTexture(GL_TEXTURE_2D,color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Menu test framebuffer is incomplete");
    }
    ~Target()
    {
        glBindFramebuffer(GL_FRAMEBUFFER,0);glBindTexture(GL_TEXTURE_2D,0);
        glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&color);
        glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glBlendEquation(GL_FUNC_ADD);
    }
    Pixels draw(Hud& hud,const PauseMenu& menu,float dpi)
    {
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(0,0,width,height);
        glDisable(GL_SCISSOR_TEST);glClearColor(.13f,.17f,.15f,1);glClear(GL_COLOR_BUFFER_BIT);
        // Nondefault caller state catches a menu accidentally leaking its blend,
        // fill, scissor, or depth settings into the first resumed world frame.
        glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glEnable(GL_BLEND);glDepthMask(GL_TRUE);
        glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
        glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
        glEnable(GL_SCISSOR_TEST);glScissor(3,5,1,1);glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
        const auto before=state();
        hud.drawPause(menu,width,height,dpi);
        require(state()==before,"Graphics menu changed caller OpenGL state");
        Pixels pixels(static_cast<std::size_t>(width)*height*4);
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        require(glGetError()==GL_NO_ERROR,"Graphics menu generated an OpenGL error");
        return pixels;
    }
};

std::size_t difference(const Pixels& a,const Pixels& b)
{
    require(a.size()==b.size(),"Menu captures have different dimensions");
    std::size_t count=0;
    for(std::size_t i=0;i<a.size();i+=4)
        count+=std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                         std::abs(int(a[i+2])-int(b[i+2]))})>20;
    return count;
}

void verifyVisibleControls(const Pixels& pixels,int width,int height,const GraphicsMenuLayout& layout)
{
    for(const auto& control:layout.controls) {
        const auto rect=control.rect;
        std::size_t bright=0;
        for(int py=std::max(0,int(std::ceil(rect.y*layout.scale)));
            py<std::min(height,int((rect.y+rect.height)*layout.scale));++py)
            for(int px=std::max(0,int(std::ceil(rect.x*layout.scale)));
                px<std::min(width,int((rect.x+rect.width)*layout.scale));++px) {
                const auto index=(static_cast<std::size_t>(height-py-1)*width+px)*4;
                bright+=pixels[index]>100 && pixels[index+1]>100 && pixels[index+2]>70;
            }
        require(bright>5,"A clickable menu control has no visible label or arrow");
    }
}
}

void verifyGraphicsMenu()
{
    Hud hud;
    struct View {int width,height;float dpi;};
    constexpr View views[]={{1280,720,1},{2560,1440,2},{640,360,2},{320,240,1}};
    for(const auto view:views) {
        Target target(view.width,view.height);
        PauseMenu menu;
        const auto closed=target.draw(hud,menu,view.dpi);
        require(!Hud::pauseClick(menu,view.width,view.height,view.dpi,view.width*.5f,view.height*.5f),
                "Closed pause menu consumed a click");
        for(const auto screen:{PauseScreen::Home,PauseScreen::Graphics}) {
            if(screen==PauseScreen::Home) menu.open();else menu.openGraphics();
            const auto layout=pauseMenuLayout(screen,view.width,view.height,view.dpi);
            const auto pixels=target.draw(hud,menu,view.dpi);
            require(difference(closed,pixels)>1000,"Open menu did not draw over the world");
            verifyVisibleControls(pixels,view.width,view.height,layout);
            for(const auto& control:layout.controls) {
                const auto r=control.rect;
                require(Hud::pauseClick(menu,view.width,view.height,view.dpi,
                            (r.x+r.width*.5f)*layout.scale,(r.y+r.height*.5f)*layout.scale)==control.action,
                        "Menu pointer target differs from its drawn control");
            }
            menu.moveSelection(1);
            require(difference(pixels,target.draw(hud,menu,view.dpi))>100,
                    "Moving keyboard selection did not visibly highlight another control");
        }
        const auto before=target.draw(hud,menu,view.dpi);
        menu.dispatch(PauseUiDecreaseBase);
        require(menu.dirty(),"Changing a preset failed to create a draft");
        const auto dirty=target.draw(hud,menu,view.dpi);
        require(difference(before,dirty)>100,"Graphics values did not change after editing");
        menu.completeApply(menu.draft(),"Settings saved.");
        require(difference(dirty,target.draw(hud,menu,view.dpi))>20,"Apply feedback did not appear");
        menu.close();
        require(difference(closed,target.draw(hud,menu,view.dpi))==0,"Closing Pause left an overlay behind");
    }
    std::cout<<"Graphics menu: visible controls, keyboard selection, Retina/small pointer targets and GL state passed.\n";
}

} // namespace voxel
