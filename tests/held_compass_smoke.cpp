#include "held_compass_smoke.h"
#include "camera.h"
#include "graphics/held_compass.h"
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
    bool held(std::size_t pixel) const { return depth[pixel]<.05f; }
    std::size_t heldPixels() const
    {
        return std::count_if(depth.begin(),depth.end(),[](float d) { return d<.05f; });
    }
};

void verifyFraming(const Frame& frame)
{
    const auto count=frame.heldPixels();
    require(count>frame.depth.size()/500 && count<frame.depth.size()/6,
            "Held compass is missing, too small or blocks too much of the view");
    std::size_t right=0,lower=0,center=0;
    for (int y=0; y<frame.height; ++y) for (int x=0; x<frame.width; ++x) {
        if (!frame.held(static_cast<std::size_t>(y)*frame.width+x)) continue;
        right += x>frame.width/2;
        lower += y<frame.height*3/4;
        center += std::abs(x-frame.width/2)<frame.width/12
               && std::abs(y-frame.height/2)<frame.height/12;
    }
    require(right>count*9/10 && lower>count*9/10 && center==0,
            "The compass must leave the aiming center and upper view clear");
}

} // namespace

void verifyHeldCompass(int width, int height)
{
    require(width>=640 && height>=360,"Framebuffer is too small for the held compass test");
    width=std::min(width,1280); height=std::min(height,720);
    World world;
    for (int z=-7; z<=8; ++z) for (int x=-8; x<=8; ++x)
        world.set({x,38,z},Block::MossStone);
    for (int y=39; y<=46; ++y) for (int x=-8; x<=8; ++x)
        world.set({x,y,-7},Block::Stone);
    Camera camera;
    camera.setPosition({.5f,41.6f,4.5f});
    camera.rotate(35,25);
    WorldRenderer renderer;
    const auto draw=[&](std::optional<HeldCompassState> compass,int w,int h,
                        std::optional<float> sword={},std::optional<float> pickaxe={},
                        std::optional<float> lantern={},bool dark=false) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,w,h);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthRange(0,1);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        renderer.render(world,camera,w,h,false,true,0,false,false,{},{},{},{},
                        sword,{},pickaxe,lantern,dark,compass);
        Frame frame{w,h,std::vector<unsigned char>(static_cast<std::size_t>(w)*h*4),
                        std::vector<float>(static_cast<std::size_t>(w)*h)};
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,frame.pixels.data());
        glReadPixels(0,0,w,h,GL_DEPTH_COMPONENT,GL_FLOAT,frame.depth.data());
        std::array<GLdouble,2> range{};
        glGetDoublev(GL_DEPTH_RANGE,range.data());
        require(range[0]==0 && range[1]==1,"Held compass leaked its near depth range into later draws");
        require(glGetError()==GL_NO_ERROR,"Held compass rendering generated an OpenGL error");
        return frame;
    };
    require(draw({},width,height).heldPixels()==0,"Compass fixture contains unexpected near geometry");
    const HeldCompassState forward{0,0};
    const auto idle=draw(forward,width,height);
    verifyFraming(idle);
    float nearest=1,farthest=0;
    for (std::size_t i=0; i<idle.depth.size(); ++i) if (idle.held(i)) {
        nearest=std::min(nearest,idle.depth[i]);
        farthest=std::max(farthest,idle.depth[i]);
    }
    require(farthest-nearest>.00002f,
            "Held compass needs genuine depth between its case, dial and needle");
    const auto rebuilds=renderer.sceneRebuildCount();
    const auto right=draw(HeldCompassState{1.57079632679f,0},width,height);
    std::size_t changedDepth=0,stableBody=0,changedSilhouette=0;
    for (std::size_t i=0; i<idle.depth.size(); ++i) {
        changedSilhouette += idle.held(i)!=right.held(i);
        if (!idle.held(i) || !right.held(i)) continue;
        changedDepth += idle.depth[i]!=right.depth[i];
        stableBody += idle.depth[i]==right.depth[i];
    }
    require(changedDepth>idle.heldPixels()/500,
            "Changing the exit bearing did not rotate a visible 3D compass needle");
    require(stableBody>idle.heldPixels()*3/4 && changedSilhouette<idle.heldPixels()/50,
            "Changing the exit bearing must turn the needle without rotating the compass case");
    require(renderer.sceneRebuildCount()==rebuilds,"Compass needle rotation rebuilt static scenery");
    require(draw({},width,height).heldPixels()==0,"Switching away left the compass in view");
    require(draw(forward,width,height).depth==idle.depth,"Reselecting the compass changed its cached meshes");

    const auto swordOnly=draw({},width,height,0);
    const auto swordAndCompass=draw(forward,width,height,0);
    require(swordOnly.depth==swordAndCompass.depth && swordOnly.pixels==swordAndCompass.pixels,
            "The compass must not draw a second tool beside the selected sword");
    const auto pickaxeOnly=draw({},width,height,{},0);
    const auto pickaxeAndCompass=draw(forward,width,height,{},0);
    require(pickaxeOnly.depth==pickaxeAndCompass.depth && pickaxeOnly.pixels==pickaxeAndCompass.pixels,
            "The compass must not draw a second tool beside the selected pickaxe");
    const auto lanternOnly=draw({},width,height,{},{},0);
    const auto lanternAndCompass=draw(forward,width,height,{},{},0);
    require(lanternOnly.depth==lanternAndCompass.depth && lanternOnly.pixels==lanternAndCompass.pixels,
            "The compass must not draw a second tool beside the selected lantern");

    // Walls may obscure the scenery, but cannot slice the case held in front
    // of the camera. Compare the model's reserved near-depth band only.
    for (int y=39; y<=46; ++y) for (int x=-8; x<=8; ++x)
        world.set({x,y,3},Block::Stone);
    const auto wall=draw(forward,width,height);
    require(wall.heldPixels()>idle.heldPixels()*95/100
            && wall.heldPixels()<idle.heldPixels()*105/100,
            "A nearby wall clips the held compass");
    verifyFraming(draw(forward,640,360));
    verifyFraming(draw(forward,480,std::min(height,640)));
    // A flat dial should remain readable when the lamp is ahead of it, while
    // still respecting the author's source-only darkness.
    for (int y=39; y<=46; ++y) for (int x=-8; x<=8; ++x)
        world.set({x,y,3},Block::Air);
    const auto dark=draw(forward,width,height,{},{},{},true);
    std::size_t litPixels=0;
    for (std::size_t i=0; i<dark.depth.size(); ++i) if (dark.held(i))
        litPixels += std::max({dark.pixels[i*4],dark.pixels[i*4+1],dark.pixels[i*4+2]})>0;
    require(litPixels==0,"The compass must not glow or introduce an invisible light into an unlit cave");
    world.set(glm::ivec3(glm::floor(camera.position()+camera.forward()*3.f)),Block::Lantern);
    const auto lamp=draw(forward,width,height,{},{},{},true);
    std::size_t readable=0,redNeedle=0;
    for (std::size_t i=0; i<lamp.depth.size(); ++i) if (lamp.held(i)) {
        const int r=lamp.pixels[i*4],g=lamp.pixels[i*4+1],b=lamp.pixels[i*4+2];
        readable += r+g+b>120;
        redNeedle += r>60 && r>g*2 && r>b*2;
    }
    require(readable>lamp.heldPixels()/3 && redNeedle>20,
            "The dial and red needle must remain readable with a visible lamp in front of the player");
    std::cout<<"Held compass passed: 3D case, independent exit needle, tool switching, wall clearance and window sizing\n";
}

} // namespace voxel
