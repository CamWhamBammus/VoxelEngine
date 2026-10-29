#include "mining_hud_smoke.h"
#include "game.h"
#include "ores.h"
#include "input.h"
#include "graphics/hud.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <unistd.h>

namespace voxel {
namespace {

void require(bool pass,const char* message)
{
    if (!pass) throw std::runtime_error(message);
}

struct Temporary {
    std::filesystem::path path;
    Temporary() {
        char name[]="/tmp/voxel-mining-hud-XXXXXX";
        require(mkdtemp(name)!=nullptr,"Cannot create mining HUD fixture directory");
        path=name;
    }
    ~Temporary() { std::error_code ignored;std::filesystem::remove_all(path,ignored); }
};

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
    glGetIntegerv(GL_VIEWPORT,result.viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX,result.scissor.data());
    glGetIntegerv(GL_POLYGON_MODE,result.polygon.data());
    result.enabled={glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),
                    glIsEnabled(GL_BLEND),glIsEnabled(GL_SCISSOR_TEST),GL_FALSE};
    glGetBooleanv(GL_DEPTH_WRITEMASK,&result.enabled[4]);
    return result;
}

using Pixels=std::vector<unsigned char>;
bool changed(const Pixels& a,const Pixels& b,std::size_t pixel)
{
    const auto i=pixel*4;
    return std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                     std::abs(int(a[i+2])-int(b[i+2]))})>20;
}
std::size_t difference(const Pixels& a,const Pixels& b)
{
    require(a.size()==b.size(),"Mining HUD readback sizes differ");
    std::size_t count=0;
    for (std::size_t i=0;i<a.size()/4;++i) count+=changed(a,b,i);
    return count;
}

void aimAt(Game& game,glm::ivec3 cell)
{
    require(game.player.setFeetPosition(game.world,{cell.x+.5f,9,cell.z+3.5f}),
            "Mining HUD fixture needs a safe floor");
    game.camera.setPosition(game.player.eyePosition());
    const auto direction=glm::normalize(glm::vec3(cell)+.5f-game.camera.position());
    const auto old=game.camera.forward();
    game.camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                       glm::degrees(std::asin(direction.y)-std::asin(old.y)));
}

} // namespace

void verifyMiningHud(int width,int height)
{
    require(width>=640 && height>=360,"Framebuffer is too small for the mining HUD test");
    width=std::min(width,1280);height=std::min(height,720);
    Temporary temporary;
    World room;buildBlankSection(room);
    constexpr glm::ivec3 Copper{0,10,0},Ruby{3,10,0};
    room.set(Copper,Block::CopperOre);room.set(Ruby,Block::RubyOre);
    SectionMetadata meta;meta.name="Mining HUD fixture";meta.category=SectionCategory::Mining;meta.tested=true;
    meta.spawn={.5f,9,3.5f};
    const auto section=acceptSection(room,meta,{},temporary.path/"sections");
    Options options;options.smokeTest=true;
    options.sectionLibrary=(temporary.path/"sections").string();
    options.levelLibrary=(temporary.path/"levels").string();
    Game game(options);
    LevelDefinition level;level.name="Mining HUD test";
    level.sections={section.lexically_relative(temporary.path/"sections")};
    require(game.levelController().start(game,level),"Mining HUD fixture level did not load");
    game.equipment.selectedSlot=0;game.equipment.pickaxeTier=oreInfo(Block::CopperOre)->requiredTier;
    Hud hud;
    const auto draw=[&](int w,int h,float scale=1) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);
        glDisable(GL_SCISSOR_TEST);glClearColor(.18f,.24f,.28f,1);glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
        glEnable(GL_BLEND);glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
        glEnable(GL_SCISSOR_TEST);glScissor(3,5,1,1);
        const auto before=state();
        hud.drawMining(game,w,h,scale);
        require(state()==before,"Mining HUD changed caller OpenGL state");
        Pixels pixels(static_cast<std::size_t>(w)*h*4);
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        require(glGetError()==GL_NO_ERROR,"Mining HUD produced an OpenGL error");
        return pixels;
    };
    InputActions tick;tick.active=true;
    InputActions held=tick;held.mineHeld=true;
    const auto blank=draw(width,height);
    aimAt(game,Copper);game.update(tick,.1f);
    require(game.mining().target() && game.mining().target()->block==Block::CopperOre,
            "Aiming must find the copper block before drawing its name");
    const auto target=draw(width,height);
    require(difference(blank,target)>150,"Aimed ore did not show its mining panel");
    game.update(held,.1f);game.update(held,.1f);
    require(game.mining().progress()>0 && game.world.get(Copper)==Block::CopperOre,
            "Holding the mining button must charge without instantly breaking the ore");
    const auto progress=draw(width,height);
    require(difference(target,progress)>40,"Mining progress did not visibly fill the meter");
    for (int y=height/2-5;y<=height/2+5;++y) for (int x=width/2-5;x<=width/2+5;++x)
        require(!changed(blank,progress,static_cast<std::size_t>(y)*width+x),
                "Mining panel obscures the crosshair");

    game.equipment.selectedSlot=1;
    require(difference(blank,draw(width,height))==0,"Mining panel remains visible with another tool selected");
    game.equipment.selectedSlot=0;
    aimAt(game,Ruby);game.update(held,.1f);
    require(game.mining().blocked() && game.mining().progress()==0,
            "A tier-one pickaxe must report the ruby's required tier");
    const auto locked=draw(width,height);
    require(difference(blank,locked)>150,"Blocked ore has no visible tier warning");
    game.equipment.pickaxeTier=oreInfo(Block::RubyOre)->requiredTier;game.update(held,.1f);
    require(!game.mining().blocked(),"The required tier must unlock mining");
    require(difference(locked,draw(width,height))>100,"Tier warning did not change after equipping a sufficient pickaxe");

    for (const auto dimensions : {glm::ivec2(640,360),glm::ivec2(480,640)}) {
        const auto w=dimensions.x,h=std::min(dimensions.y,height);
        game.equipment.selectedSlot=1;const auto empty=draw(w,h,2);
        game.equipment.selectedSlot=0;const auto panel=draw(w,h,2);
        require(difference(empty,panel)>80,"Mining panel disappeared in a smaller or high-DPI window");
        for (int y=0;y<h;++y) for (int x=0;x<w;++x)
            if (changed(empty,panel,static_cast<std::size_t>(y)*w+x))
                require(x>4 && x<w-5 && y>4 && y<h-5,"Mining panel is clipped by the viewport");
    }
    InputActions inventory=tick;inventory.inventory=true;game.update(inventory,.1f);
    require(game.menuOpen() && difference(blank,draw(width,height))==0,
            "Mining panel must hide while the inventory is open");
    game.update(inventory,.1f);aimAt(game,Copper);game.update(held,.1f);
    require(game.damagePlayer(100) && game.health().dead(),"Mining HUD fixture must be able to die");
    require(difference(blank,draw(width,height))==0,"Mining panel must hide during death and respawn");
    glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
    std::cout<<"Mining HUD passed: ore names, progress, tier warning, visibility, sizing and GL state\n";
}

} // namespace voxel
