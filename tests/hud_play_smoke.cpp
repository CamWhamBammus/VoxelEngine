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

void verifyCombatBars(int width,int height,Hud& hud)
{
    Options options;options.smokeTest=true;
    Game game(options);
    buildBlankSection(game.world);
    require(game.player.setFeetPosition(game.world,{.5f,9,6.5f}),"Combat HUD fixture needs a safe floor");
    game.camera.setPosition(game.player.eyePosition());
    HubAsset spawner{HubAssetKind::TrialSpawner,{.5f,9,.5f}};
    spawner.spawner.counts={1,0,0};
    game.beginSectionLoot(std::span(&spawner,1),true);
    InputActions tick;tick.active=true;
    for(int frame=0;frame<10 && game.monsters().empty();++frame) game.updateCombat(tick,.1f);
    require(game.monsters().size()==1,"Combat HUD fixture did not spawn its skeleton");
    const auto feet=game.monsters().front().position;
    require(game.player.setFeetPosition(game.world,feet+glm::vec3(0,0,2.6f)),"Combat HUD fixture needs sword reach");
    game.camera.setPosition(game.player.eyePosition());
    const auto direction=glm::normalize(feet+glm::vec3(0,1.1f,0)-game.camera.position());
    const auto previous=game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(previous.z,previous.x)),
                       glm::degrees(std::asin(direction.y)-std::asin(previous.y)));
    const auto draw=[&](int w,int h,float scale=1) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);
        glClearColor(.18f,.24f,.28f,1);glClear(GL_COLOR_BUFFER_BIT);
        const auto before=state();hud.drawCombat(game,w,h,scale);
        require(state()==before,"Mob bars changed caller OpenGL state");
        Pixels pixels(static_cast<std::size_t>(w)*h*4);
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        require(glGetError()==GL_NO_ERROR,"Mob bars produced an OpenGL error");
        return pixels;
    };
    const auto unhurt=draw(width,height);
    game.equipment.selectedSlot=3;
    InputActions attack;attack.active=true;attack.attack=true;
    game.updateCombat(attack,.01f);
    require(game.encounters().healthBars().front().visibleSeconds>0
            && game.encounters().healthBars().front().current<game.encounters().healthBars().front().maximum,
            "A sword hit must reveal a damaged mob health bar");
    game.equipment.selectedSlot=0;
    const auto hurt=draw(width,height);
    require(difference(unhurt,hurt)>150,"Damaged mob bar did not appear above its head");
    draw(640,360,2);
    const auto midpoint=glm::ivec3(glm::floor((game.camera.position()+game.encounters().healthBars().front().position)*.5f));
    for(int z=-1;z<=1;++z) for(int y=-2;y<=2;++y) for(int x=-1;x<=1;++x)
        game.world.set(midpoint+glm::ivec3(x,y,z),Block::Stone);
    require(difference(unhurt,draw(width,height))==0,"Mob health bars leaked through a cave wall");
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
        const auto draw=[&](int w, int h, float scale=1.0f, bool healthOnly=false) {
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
            if (!healthOnly) {
                hud.drawCombat(game,w,h,scale);
                require(state()==before,"Combat HUD changed caller OpenGL state");
                hud.drawPlay(game,w,h,scale);
            }
            require(state()==before,"Play HUD changed caller OpenGL state");
            hud.drawHealth(game,w,h,scale);
            require(state()==before,"Health HUD changed caller OpenGL state");
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
        game.equipment.selectedSlot=3;
        const auto sword=draw(width,height);
        require(difference(compass,sword)>1500,"Sword selection did not display its fourth pocket and held blade");
        InputActions swing;swing.active=true;swing.attack=true;
        game.updateCombat(swing,.1f);
        InputActions rest;rest.active=true;game.updateCombat(rest,.1f);
        require(game.encounters().swordSwing()>0 && difference(sword,draw(width,height))>1000,
                "Sword swing did not animate the held weapon");
        for(int frame=0;frame<6;++frame) game.updateCombat(rest,.1f);
        game.equipment.selectedSlot=2;
        const auto normalScale=draw(width,height);
        const auto highScale=draw(width,height,2);
        if (width>460 && height>440)
            require(difference(normalScale,highScale)>100,"Play HUD did not adapt to content scale");
        const auto small=draw(640,360,1);
        draw(640,360,2);
        require(difference(small,draw(640,360,std::numeric_limits<float>::quiet_NaN()))==0,
                "Invalid HUD scale did not fall back to the normal layout");

        require(game.healthVisible(),"Walking player has no health HUD");
        const auto healthy=draw(width,height,1,true);
        require(game.damagePlayer(35),"HUD fixture cannot damage the player");
        const auto hurt=draw(width,height,1,true);
        require(difference(healthy,hurt)>350,"Health damage did not visibly change the meter and feedback");
        // Read inside the bar, away from the flash and frame: losing health must
        // shorten the red fill, not merely update the number or screen tint.
        const float healthScale=std::min({1.0f,width/780.0f,height/440.0f});
        const auto redFill=[&](const Pixels& pixels) {
            std::size_t count=0;
            const int left=static_cast<int>(31*healthScale),right=static_cast<int>(185*healthScale);
            const int bottom=static_cast<int>(30*healthScale),top=static_cast<int>(34*healthScale);
            for (int y=bottom;y<top;++y) for (int x=left;x<right;++x) {
                const auto index=(static_cast<std::size_t>(y)*width+x)*4;
                count+=pixels[index]>100 && pixels[index]>pixels[index+1]*2;
            }
            return count;
        };
        require(redFill(hurt)<redFill(healthy)*.8f,"Damaged health bar did not shorten its red fill");
        draw(640,360,2,true);
        game.restoreHealth();
        require(difference(healthy,draw(width,height,1,true))==0,"Restoring health did not restore the full bar");
        require(game.damagePlayer(Health::Maximum) && game.health().dead(),"HUD fixture cannot enter death state");
        const auto dead=draw(width,height,1,true);
        require(difference(healthy,dead)>static_cast<std::size_t>(width)*height/2,
                "Death overlay did not cover the scene and announce respawn");
        InputActions waiting; waiting.active=true;
        for (int frame=0;frame<4;++frame) game.update(waiting,.1f);
        require(game.health().dead() && game.health().respawnProgress()>0,
                "HUD fixture did not advance the automatic respawn timer");
        require(difference(dead,draw(width,height,1,true))>100,
                "Automatic respawn progress did not visibly advance");
        draw(640,360,2,true);
        game.restoreHealth();
        require(difference(healthy,draw(width,height,1,true))==0,"Death overlay remains after health restoration");

        const auto dryFeet=game.player.feetPosition();
        const auto dryHud=draw(width,height);
        for (int z=80;z<=83;++z) for (int y=40;y<=44;++y) for (int x=80;x<=83;++x)
            game.world.set({x,y,z},Block::Water);
        require(game.player.setFeetPosition(game.world,{81.5f,41.0f,81.5f}) && game.player.inWater(),
                "HUD fixture cannot enter water");
        require(difference(dryHud,draw(width,height))>150,"Entering water did not show swim controls");
        draw(640,360,2);
        require(game.player.setFeetPosition(game.world,dryFeet) && !game.player.inWater(),
                "HUD fixture cannot leave water");
        require(difference(dryHud,draw(width,height))==0,"Swim controls remain after leaving water");

        require(game.player.setFeetPosition(game.world,sceneBlacksmithApproach()),"HUD fixture cannot reach the blacksmith");
        game.camera.setPosition(game.player.eyePosition());
        InputActions open; open.active=true; open.interact=true;
        game.update(open);
        require(game.smithOpen(),"HUD fixture could not open the blacksmith");
        game.equipment.selectedSlot=0;
        game.equipment.pickaxeTier=1; game.equipment.inventory={};
        game.equipment.inventory.add(ItemId::CopperOre,12);
        const auto affordable=draw(width,height);
        game.equipment.inventory.remove(ItemId::CopperOre,12);
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
        hud.drawCombat(game,0,0);
        require(state()==before,"Minimized combat HUD changed OpenGL state");
        hud.drawHealth(game,0,0);
        require(state()==before,"Minimized health HUD changed OpenGL state");
        hud.draw("EDITOR - F5 SAVE",width,height);
        require(state()==before,"Text HUD no longer restores OpenGL state");
        require(glGetError()==GL_NO_ERROR,"HUD compatibility check generated an OpenGL error");
        verifyCombatBars(width,height,hud);
    } catch (...) {
        cleanup(); throw;
    }
    cleanup();
    std::cout << "Play HUD passed: health and death, sword swings, hit-revealed occluded mob bars, tool selection, compass, swim controls and exit, forge states, feedback, DPI and GL state\n";
}

} // namespace voxel
