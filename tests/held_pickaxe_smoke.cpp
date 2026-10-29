#include "held_pickaxe_smoke.h"
#include "camera.h"
#include "equipment.h"
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
    bool pickaxe(std::size_t pixel) const { return depth[pixel] < .05f; }
    std::size_t pickaxePixels() const
    {
        return std::count_if(depth.begin(), depth.end(), [](float d) { return d < .05f; });
    }
};

glm::vec3 pickaxeColor(const Frame& frame)
{
    glm::vec3 total(0);
    std::size_t count=0;
    // Only shaded geometry counts, excluding the surrounding scene and bloom.
    for (std::size_t i=0; i<frame.depth.size(); ++i) if (frame.pickaxe(i)) {
        total += glm::vec3(frame.pixels[i*4],frame.pixels[i*4+1],frame.pixels[i*4+2]);
        ++count;
    }
    require(count>100,"Held pickaxe has no visible shaded geometry");
    return total/static_cast<float>(count);
}

void verifyFraming(const Frame& frame)
{
    const auto count=frame.pickaxePixels();
    require(count>frame.depth.size()/200 && count<frame.depth.size()/5,
            "Held pickaxe is missing, too small or obstructs the view");
    std::size_t right=0, lower=0;
    for (int y=0; y<frame.height; ++y) for (int x=0; x<frame.width; ++x) {
        if (!frame.pickaxe(static_cast<std::size_t>(y)*frame.width+x)) continue;
        right += x>frame.width/2;
        lower += y<frame.height*3/4;
    }
    require(right>count*9/10 && lower>count*9/10,
            "Idle pickaxe must stay in the right hand without covering the upper view");
}

} // namespace

void verifyHeldPickaxe(int width, int height)
{
    require(width>=640 && height>=360,"Framebuffer is too small for the held pickaxe test");
    width=std::min(width,1280); height=std::min(height,720);
    World world;
    // A small high-altitude fixture leaves the generated ground well behind
    // the near weapon, so its resolved depth identifies the actual 3D mesh.
    for (int z=-7; z<=8; ++z) for (int x=-8; x<=8; ++x)
        world.set({x,38,z},Block::MossStone);
    for (int y=39; y<=46; ++y) for (int x=-8; x<=8; ++x)
        world.set({x,y,-7},Block::Stone);
    Camera camera;
    camera.setPosition({.5f,41.6f,4.5f});
    camera.rotate(35,25); // Look along -Z, level with the horizon.
    WorldRenderer renderer;
    const auto draw=[&](std::optional<float> swing, int w, int h, std::optional<float> sword = {}, int tier=1) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,w,h);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthRange(0,1);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
        renderer.render(world,camera,w,h,false,true,0,false,false,{},{},{},{},sword,{},swing,{},false,{},{},1,tier);
        Frame frame{w,h,std::vector<unsigned char>(static_cast<std::size_t>(w)*h*4),
                        std::vector<float>(static_cast<std::size_t>(w)*h)};
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,frame.pixels.data());
        glReadPixels(0,0,w,h,GL_DEPTH_COMPONENT,GL_FLOAT,frame.depth.data());
        std::array<GLdouble,2> range{};
        glGetDoublev(GL_DEPTH_RANGE,range.data());
        require(range[0]==0 && range[1]==1,"Held pickaxe leaked its near depth range into later draws");
        require(glGetError()==GL_NO_ERROR,"Held pickaxe rendering generated an OpenGL error");
        return frame;
    };
    const auto noPickaxe=draw({},width,height);
    require(noPickaxe.pickaxePixels()==0,"Weapon fixture contains unexpected near geometry");
    const auto idle=draw(0,width,height);
    verifyFraming(idle);
    const auto rebuilds=renderer.sceneRebuildCount();
    const auto swing=draw(.47f,width,height);
    std::size_t moved=0;
    for (std::size_t i=0; i<idle.depth.size(); ++i)
        moved += idle.pickaxe(i)!=swing.pickaxe(i);
    require(swing.pickaxePixels()>idle.pickaxePixels()/3 && moved>idle.pickaxePixels()/2,
            "Pickaxe swing did not move its visible 3D silhouette");
    require(renderer.sceneRebuildCount()==rebuilds,"Pickaxe animation rebuilt static scenery");
    require(draw({},width,height).pickaxePixels()==0,"Switching away left a held pickaxe in the view");
    const auto upgraded=draw(0,width,height,{},GearTierCount);
    require(upgraded.depth==idle.depth && glm::length(pickaxeColor(upgraded)-pickaxeColor(idle))>5,
            "Upgrading must recolor the cached 3D pickaxe without changing its silhouette");
    const auto restored=draw(0,width,height);
    require(restored.pixels==idle.pixels && renderer.sceneRebuildCount()==rebuilds,
            "Changing pickaxe material must update its cache without rebuilding the world");


    const auto swordOnly=draw({},width,height,0);
    const auto bothTools=draw(0,width,height,0);
    require(swordOnly.depth==bothTools.depth && swordOnly.pixels==bothTools.pixels,
            "Selecting a sword must not draw a second tool when both arguments are provided");
    require(draw(0,width,height).pickaxePixels()==idle.pickaxePixels(),
            "Switching back from sword must restore exactly one cached pickaxe");

    // Both lamps sit behind the viewer, out of frame; any change measured here
    // must be world light falling on the head rather than a visible lamp.
    world.set({2,42,5},Block::Lantern);
    const auto warm=draw(0,width,height);
    world.set({2,42,5},Block::Crystal);
    const auto cool=draw(0,width,height);
    const auto warmColor=pickaxeColor(warm),coolColor=pickaxeColor(cool);
    require(warmColor.r-warmColor.b>coolColor.r-coolColor.b+8,
            "Held pickaxe did not pick up warm lantern and cool crystal lighting");

    // Stand half a block from a solid wall: its depth may hide the scene but
    // must not slice the camera-attached weapon in half.
    for (int y=39; y<=46; ++y) for (int x=-8; x<=8; ++x)
        world.set({x,y,3},Block::Stone);
    const auto wall=draw(0,width,height);
    require(wall.pickaxePixels()>idle.pickaxePixels()*95/100
            && wall.pickaxePixels()<idle.pickaxePixels()*105/100,
            "A close wall clips the held pickaxe silhouette");
    verifyFraming(draw(0,640,360));
    verifyFraming(draw(0,480,std::min(height,640)));
    std::cout << "Held pickaxe passed: shaded 3D mesh, swing, world lighting, wall clearance and window sizing\n";
}

} // namespace voxel
