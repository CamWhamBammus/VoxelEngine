#include "root_crossing_hud_smoke.h"
#include "graphics/hud.h"
#include "game.h"
#include "input.h"
#include "root_crossings.h"

#include <algorithm>
#include <array>
#include <iostream>
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
    result.enabled={glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),
                    glIsEnabled(GL_BLEND),glIsEnabled(GL_SCISSOR_TEST),GL_FALSE};
    glGetBooleanv(GL_DEPTH_WRITEMASK,&result.enabled[4]);
    return result;
}

using Pixels=std::vector<unsigned char>;
std::size_t difference(const Pixels& a, const Pixels& b)
{
    require(a.size()==b.size(),"Root Crossing HUD image sizes differ");
    std::size_t changed=0;
    for (std::size_t i=0; i<a.size(); i+=4)
        changed+=std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                           std::abs(int(a[i+2])-int(b[i+2]))})>20;
    return changed;
}

} // namespace

void verifyRootCrossingHud(int width, int height)
{
    require(width>=640 && height>=360,"Framebuffer is too small for Root Crossing HUD tests");
    width=std::min(width,1280); height=std::min(height,720);
    Options options;
    options.smokeTest=true;
    options.rootCrossing=0;
    Game game(options);
    // The layout is deterministic; capture its landing positions for real
    // checkpoint/finish transitions without forcing private Game state.
    const auto course=buildRootCrossing(game.world,0);
    Hud hud;
    Shader caller("#version 410 core\nvoid main(){gl_Position=vec4(0);}",
                  "#version 410 core\nout vec4 color;void main(){color=vec4(1);}");
    GLuint vao=0,buffer=0;
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
        const auto draw=[&](int w,int h,float scale=1.0f) {
            glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,w,h);
            glDisable(GL_SCISSOR_TEST); glClearColor(0.18f,0.24f,0.28f,1);
            glClear(GL_COLOR_BUFFER_BIT);
            caller.use(); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,buffer);
            glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);
            glEnable(GL_BLEND); glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
            glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
            glEnable(GL_SCISSOR_TEST); glScissor(3,5,1,1);
            glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); glActiveTexture(GL_TEXTURE3);
            const auto before=state();
            hud.drawPlay(game,w,h,scale);
            require(state()==before,"Root Crossing HUD changed caller OpenGL state");
            Pixels pixels(static_cast<std::size_t>(w)*h*4);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Root Crossing HUD generated an OpenGL error");
            return pixels;
        };
        const auto courseStart=draw(width,height);
        InputActions open; open.active=true; open.crossingMenu=true;
        game.update(open);
        require(game.crossingMenuOpen(),"HUD fixture could not open the crossing picker");
        const auto first=draw(width,height);
        require(difference(courseStart,first)>10000,"Root Crossing picker did not cover the play HUD");
        InputActions select; select.active=true; select.selectedCrossing=9;
        game.update(select);
        require(game.crossingMenuSelection()==9,"HUD fixture did not select the tenth course");
        require(difference(first,draw(width,height))>500,"Picker did not visibly select the tenth course");
        const auto tenthSmall=draw(640,360,2);
        select.selectedCrossing=0;
        game.update(select);
        require(difference(tenthSmall,draw(640,360,2))>200,
                "Small high-DPI picker did not show the changed course selection");
        draw(width,height,2);

        InputActions close; close.closeMenu=true;
        game.update(close);
        require(!game.crossingMenuOpen(),"HUD fixture could not close the picker");
        game.equipment.selectedSlot=2;
        const auto compass=draw(width,height);
        game.camera.rotate(90,0);
        require(difference(compass,draw(width,height))>30,"Course compass did not turn toward the exit");
        game.equipment.selectedSlot=0;
        InputActions active; active.active=true;
        require(game.player.setFeetPosition(game.world,course.checkpoint),"HUD fixture cannot reach checkpoint");
        game.update(active,0.1f);
        require(game.crossingCheckpointReached(),"HUD fixture did not activate checkpoint");
        const auto checkpoint=draw(width,height);
        require(difference(courseStart,checkpoint)>200,"Checkpoint did not visibly update the course HUD");
        require(game.player.setFeetPosition(game.world,course.finish),"HUD fixture cannot reach the exit");
        game.update(active,0.1f);
        require(game.crossingCompleted() && game.crossingBestTime()>0,"HUD fixture did not complete the course");
        const auto complete=draw(width,height);
        require(difference(checkpoint,complete)>100,"Completed crossing did not show its completion notice");
        draw(640,360,2);
        InputActions restart; restart.active=true; restart.restartCrossing=true;
        game.update(restart);
        require(!game.crossingCompleted() && !game.crossingCheckpointReached(),"HUD fixture did not restart");
        require(difference(complete,draw(width,height))>100,"Restarted crossing retained its completion notice");
        const auto before=state();
        hud.drawPlay(game,0,0);
        require(state()==before,"Minimized crossing HUD changed OpenGL state");
    } catch (...) {
        cleanup(); throw;
    }
    cleanup();
    std::cout << "Root Crossing HUD passed: course picker, small/DPI layout, compass, checkpoint, completion and GL state\n";
}

} // namespace voxel
