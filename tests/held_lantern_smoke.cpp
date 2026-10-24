#include "held_lantern_smoke.h"
#include "camera.h"
#include "graphics/renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Frame {
    int width, height;
    std::vector<unsigned char> pixels;
    std::vector<float> depth;
    bool held(std::size_t pixel) const { return depth[pixel] < .05f; }
    std::size_t heldPixels() const
    {
        return std::count_if(depth.begin(),depth.end(),[](float d) { return d<.05f; });
    }
};

void verifyFraming(const Frame& frame)
{
    const auto count=frame.heldPixels();
    require(count>frame.depth.size()/200 && count<frame.depth.size()/6,
            "Held lantern is missing, too small or blocks too much of the view");
    std::size_t right=0,lower=0,center=0;
    for (int y=0; y<frame.height; ++y) for (int x=0; x<frame.width; ++x) {
        if (!frame.held(static_cast<std::size_t>(y)*frame.width+x)) continue;
        right += x>frame.width/2;
        lower += y<frame.height*3/4;
        center += std::abs(x-frame.width/2)<frame.width/12
               && std::abs(y-frame.height/2)<frame.height/12;
    }
    require(right>count*9/10 && lower>count*9/10 && center==0,
            "The resting lantern must leave the aiming center and upper view clear");
}

double sceneBrightness(const Frame& frame)
{
    double total=0;
    std::size_t count=0;
    // Keep the measurement away from the lantern and its bloom. This samples
    // the cave wall itself, so a bright pane cannot stand in for carried light.
    for (int y=frame.height/3; y<frame.height*2/3; ++y)
        for (int x=frame.width/8; x<frame.width*3/8; ++x) {
            const auto i=static_cast<std::size_t>(y)*frame.width+x;
            if (frame.held(i)) continue;
            total+=.2126*frame.pixels[i*4]+.7152*frame.pixels[i*4+1]+.0722*frame.pixels[i*4+2];
            ++count;
        }
    require(count>100,"No visible cave wall remains for the carried lantern check");
    return total/count;
}

} // namespace

void verifyHeldLantern(int width, int height)
{
    require(width>=640 && height>=360,"Framebuffer is too small for the held lantern test");
    width=std::min(width,1280); height=std::min(height,720);
    World world;
    for (int z=-7; z<=8; ++z) for (int x=-8; x<=8; ++x)
        world.set({x,38,z},Block::MossStone);
    for (int y=39; y<=46; ++y) for (int x=-8; x<=8; ++x)
        world.set({x,y,0},Block::Stone);
    Camera camera;
    camera.setPosition({.5f,41.6f,4.5f});
    camera.rotate(35,25);
    WorldRenderer renderer;
    const auto draw=[&](std::optional<float> lantern,int w,int h,bool light=true,
                        std::optional<float> sword={},std::optional<float> pickaxe={}) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,w,h);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthRange(0,1);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        renderer.render(world,camera,w,h,false,true,0,light,false,{},{},{},{},sword,{},pickaxe,lantern);
        Frame frame{w,h,std::vector<unsigned char>(static_cast<std::size_t>(w)*h*4),
                        std::vector<float>(static_cast<std::size_t>(w)*h)};
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,frame.pixels.data());
        glReadPixels(0,0,w,h,GL_DEPTH_COMPONENT,GL_FLOAT,frame.depth.data());
        std::array<GLdouble,2> range{};
        glGetDoublev(GL_DEPTH_RANGE,range.data());
        require(range[0]==0 && range[1]==1,"Held lantern leaked its near depth range into later draws");
        require(glGetError()==GL_NO_ERROR,"Held lantern rendering generated an OpenGL error");
        return frame;
    };
    const auto absent=draw({},width,height,false);
    require(absent.heldPixels()==0,"Lantern fixture contains unexpected near geometry");
    const auto idle=draw(0,width,height);
    verifyFraming(idle);
    const auto rebuilds=renderer.sceneRebuildCount();
    std::size_t warmPixels=0,darkPixels=0;
    float nearest=1,farthest=0;
    for (std::size_t i=0; i<idle.depth.size(); ++i) if (idle.held(i)) {
        nearest=std::min(nearest,idle.depth[i]);
        farthest=std::max(farthest,idle.depth[i]);
        const int r=idle.pixels[i*4],g=idle.pixels[i*4+1],b=idle.pixels[i*4+2];
        warmPixels += r>170 && g>100 && r>b+40;
        darkPixels += std::max({r,g,b})<130;
    }
    require(farthest-nearest>.00002f,
            "Held lantern needs genuine depth between the handle, cage and panels");
    require(warmPixels>idle.heldPixels()/12 && darkPixels>idle.heldPixels()/12,
            "The lantern's warm luminous panes must remain distinct from its dark frame");
    const auto sway=draw(1.8f,width,height);
    std::size_t moved=0;
    for (std::size_t i=0; i<idle.depth.size(); ++i)
        moved += idle.held(i)!=sway.held(i);
    require(sway.heldPixels()>idle.heldPixels()*3/4 && moved>idle.heldPixels()/50,
            "The lantern did not gently sway in the player's hand");
    require(renderer.sceneRebuildCount()==rebuilds,"Lantern animation rebuilt static scenery");
    require(draw({},width,height).heldPixels()==0,"Switching away left the held lantern in view");
    require(draw(0,width,height).depth==idle.depth,"Reselecting the lantern changed its cached mesh");

    const auto swordOnly=draw({},width,height,true,0);
    const auto swordAndLantern=draw(0,width,height,true,0);
    require(swordOnly.depth==swordAndLantern.depth && swordOnly.pixels==swordAndLantern.pixels,
            "The lantern must not draw a second tool beside the selected sword");
    const auto pickaxeOnly=draw({},width,height,true,{},0);
    const auto pickaxeAndLantern=draw(0,width,height,true,{},0);
    require(pickaxeOnly.depth==pickaxeAndLantern.depth && pickaxeOnly.pixels==pickaxeAndLantern.pixels,
            "The lantern must not draw a second tool beside the selected pickaxe");

    require(sceneBrightness(idle)>sceneBrightness(absent)+4,
            "Equipping the lantern did not illuminate the cave around the player");
    // A wall immediately in front of the camera must not slice the cage or
    // glove. Only the model's reserved near-depth band is compared here.
    for (int y=39; y<=46; ++y) for (int x=-8; x<=8; ++x)
        world.set({x,y,3},Block::Stone);
    const auto wall=draw(0,width,height);
    require(wall.heldPixels()>idle.heldPixels()*95/100
            && wall.heldPixels()<idle.heldPixels()*105/100,
            "A nearby wall clips the held lantern");
    const auto unlitWall=draw(0,width,height,false);
    require(sceneBrightness(wall)>sceneBrightness(unlitWall)+4,
            "The held lantern's light source slipped behind a nearby wall");
    verifyFraming(draw(0,640,360));
    verifyFraming(draw(0,480,std::min(height,640)));
    std::cout<<"Held lantern passed: 3D cage, luminous panes, sway, carried light, wall clearance and window sizing\n";
}

} // namespace voxel
