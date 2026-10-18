#include "hud_play_smoke.h"
#include "graphics/hud.h"
#include "game.h"
#include "input.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {

void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

struct State {
    std::array<GLint, 13> bindings{};
    std::array<GLint, 4> viewport{}, scissor{};
    std::array<GLint, 2> polygon{};
    std::array<GLboolean, 5> enabled{};
    bool operator==(const State&) const = default;
};

State state()
{
    State result;
    constexpr std::array<GLenum, 13> names{
        GL_CURRENT_PROGRAM, GL_VERTEX_ARRAY_BINDING, GL_ARRAY_BUFFER_BINDING,
        GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA,
        GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA, GL_ACTIVE_TEXTURE, GL_DEPTH_FUNC,
        GL_DRAW_FRAMEBUFFER_BINDING, GL_READ_FRAMEBUFFER_BINDING};
    for (std::size_t i=0; i<names.size(); ++i) glGetIntegerv(names[i],&result.bindings[i]);
    glGetIntegerv(GL_VIEWPORT,result.viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX,result.scissor.data());
    glGetIntegerv(GL_POLYGON_MODE,result.polygon.data());
    result.enabled = {glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),
                      glIsEnabled(GL_BLEND),glIsEnabled(GL_SCISSOR_TEST),GL_FALSE};
    glGetBooleanv(GL_DEPTH_WRITEMASK,&result.enabled[4]);
    return result;
}

using Pixels = std::vector<unsigned char>;
std::size_t difference(const Pixels& a, const Pixels& b)
{
    require(a.size()==b.size(),"HUD test image dimensions differ");
    std::size_t changed=0;
    for (std::size_t i=0; i<a.size(); i+=4)
        changed += std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                            std::abs(int(a[i+2])-int(b[i+2]))}) > 20;
    return changed;
}

} // namespace

void verifyPlayHud(int width, int height)
{
    require(width>=640 && height>=360,"Framebuffer is too small for the play HUD smoke test");
    // This fixture only draws UI; limit readbacks without losing full-size and
    // high-DPI coverage. A second viewport exercises the smaller layout.
    width=std::min(width,1280); height=std::min(height,720);
    Options options;
    options.smokeTest=true;
    Game game(options); // Empty progressPath prevents profile reads and writes.
    Hud hud;
    Shader caller("#version 410 core\nvoid main(){gl_Position=vec4(0);}",
                  "#version 410 core\nout vec4 color;void main(){color=vec4(1);}");
    GLuint vao=0, buffer=0;
    glGenVertexArrays(1,&vao); glGenBuffers(1,&buffer);
    const auto cleanup=[&] {
        glUseProgram(0); glBindVertexArray(0); glBindBuffer(GL_ARRAY_BUFFER,0);
        glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&buffer);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glCullFace(GL_BACK);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); glBlendEquation(GL_FUNC_ADD);
        glActiveTexture(GL_TEXTURE0);
    };
    try {
        const auto draw=[&](int w, int h, float scale=1.0f) {
            glBindFramebuffer(GL_FRAMEBUFFER,0);
            glViewport(0,0,w,h);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0.18f,0.24f,0.28f,1);
            glClear(GL_COLOR_BUFFER_BIT);
            caller.use(); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,buffer);
            glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);
            glEnable(GL_BLEND); glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
            glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
            glEnable(GL_SCISSOR_TEST); glScissor(3,5,1,1);
            glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            glActiveTexture(GL_TEXTURE3);
            const auto before=state();
            hud.drawPlay(game,w,h,scale);
            require(state()==before,"Play HUD changed caller OpenGL state");
            Pixels pixels(static_cast<std::size_t>(w)*h*4);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Play HUD generated an OpenGL error");
            return pixels;
        };
        game.equipment.selectedSlot=0;
        const auto pickaxe=draw(width,height);
        game.equipment.selectedSlot=1;
        require(difference(pickaxe,draw(width,height))>100,"Hotbar selection did not visibly move to the lantern");
        game.equipment.selectedSlot=2;
        const auto compass=draw(width,height);
        game.camera.rotate(70,0);
        require(difference(compass,draw(width,height))>30,"Compass did not respond to the camera bearing");
        const auto normalScale=draw(width,height);
        const auto highScale=draw(width,height,2);
        if (width>460 && height>440)
            require(difference(normalScale,highScale)>100,"Play HUD did not adapt to content scale");
        const auto small=draw(640,360,1);
        draw(640,360,2);
        require(difference(small,draw(640,360,std::numeric_limits<float>::quiet_NaN()))==0,
                "Invalid HUD scale did not fall back to the normal layout");

        require(game.player.setFeetPosition(game.world,sceneBlacksmithApproach()),"HUD fixture cannot reach the blacksmith");
        game.camera.setPosition(game.player.eyePosition());
        InputActions open; open.active=true; open.interact=true;
        game.update(open);
        require(game.smithOpen(),"HUD fixture could not open the blacksmith");
        game.equipment.selectedSlot=0;
        game.equipment.pickaxeTier=1; game.equipment.copper=12; game.equipment.iron=0;
        const auto affordable=draw(width,height);
        game.equipment.copper=0;
        require(difference(affordable,draw(width,height))>400,"Unaffordable forge upgrade did not show a disabled state");
        game.equipment.pickaxeTier=3;
        require(difference(affordable,draw(width,height))>400,"Maximum pickaxe tier did not change the forge panel");
        const auto capped=draw(width,height);
        game.status="Upgrade not saved: "+std::string(300,'X');
        require(difference(capped,draw(width,height))>30,"Forge panel did not show bounded save-error feedback");
        draw(640,360,2);
        const auto before=state();
        hud.drawPlay(game,0,0);
        require(state()==before,"Minimized play HUD changed OpenGL state");
        hud.draw("EDITOR - F5 SAVE",width,height);
        require(state()==before,"Text HUD no longer restores OpenGL state");
        require(glGetError()==GL_NO_ERROR,"HUD compatibility check generated an OpenGL error");
    } catch (...) {
        cleanup(); throw;
    }
    cleanup();
    std::cout << "Play HUD passed: tool selection, compass, forge states, feedback, DPI and GL state\n";
}

} // namespace voxel
