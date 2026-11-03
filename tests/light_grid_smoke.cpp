#include "light_grid_smoke.h"
#include "graphics/renderer.h"
#include "camera.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace voxel {
void verifyLightGridRendering()
{
    constexpr int width=480,height=300;
    World world;
    for (int x=-12;x<=12;++x) for (int z=-12;z<=12;++z) {
        world.set({x,39,z},Block::Brick);
        for (int y=40;y<=47;++y) world.set({x,y,z},z==-10?Block::Stone:Block::Air);
    }
    for (int x=-3;x<=3;++x) for (int z=-3;z<=1;++z) world.set({x,40,z},Block::Water);
    world.set({-6,42,2},Block::Lantern);
    world.set({4,41,-4},Block::Crystal);
    world.set({-3,44,-8},Block::AmberCrystal);
    world.set({-8,40,-6},Block::GlowMoss);
    std::vector<HubAsset> assets{{HubAssetKind::ChainLantern,{2.5f,45,3.5f}},
                               {HubAssetKind::TimberFence,{-4.5f,40,-1.5f}}};
    Camera camera;
    WorldRenderer renderer;
    GLuint sentinel[2];
    glGenTextures(2,sentinel);
    GLint oldActive=0,oldRange=0,oldIndices=0;
    glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive);
    glActiveTexture(GL_TEXTURE4);glGetIntegerv(GL_TEXTURE_BINDING_3D,&oldRange);
    glBindTexture(GL_TEXTURE_3D,sentinel[0]);
    glActiveTexture(GL_TEXTURE5);glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldIndices);
    glBindTexture(GL_TEXTURE_2D,sentinel[1]);
    const auto release=[&] {
        glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_3D,oldRange);
        glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,oldIndices);
        glActiveTexture(oldActive);glDeleteTextures(2,sentinel);
    };
    try {
        const auto draw=[&](bool spatial) {
            renderer.setLightGridEnabled(spatial);
            renderer.render(world,camera,width,height,false,true,0,false,false,assets);
            GLint active=0,ranges=0,indices=0;
            glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
            glActiveTexture(GL_TEXTURE4);glGetIntegerv(GL_TEXTURE_BINDING_3D,&ranges);
            glActiveTexture(GL_TEXTURE5);glGetIntegerv(GL_TEXTURE_BINDING_2D,&indices);
            if (active!=GL_TEXTURE5 || GLuint(ranges)!=sentinel[0] || GLuint(indices)!=sentinel[1])
                throw std::runtime_error("Spatial lighting did not restore its texture units");
            std::vector<unsigned char> pixels(width*height*4);
            glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Spatial light rendering OpenGL failure");
            return pixels;
        };
        for (const float x:{-4.01f,-4.f,-3.99f,-.01f,.01f,3.99f,4.01f}) {
            camera.setPosition({x,43.2f,10});
            const auto direction=glm::normalize(glm::vec3(0,40,-3)-camera.position()),old=camera.forward();
            camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                          glm::degrees(std::asin(direction.y)-std::asin(old.y)));
            const auto tiled=draw(true);
            const auto rebuilds=renderer.sceneRebuildCount();
            const auto reference=draw(false);
            int maximumDifference=0;
            for (std::size_t i=0;i<tiled.size();++i)
                maximumDifference=std::max(maximumDifference,std::abs(int(tiled[i])-int(reference[i])));
            if (maximumDifference>1 || renderer.sceneRebuildCount()!=rebuilds)
                throw std::runtime_error("Spatial light bins changed reference lighting or rebuilt scenery");
        }
        // Replace sources and occluders in the same world object: old grid
        // entries and visibility data must not survive a voxel edit.
        world.set({-6,42,2},Block::Air);
        world.set({1,42,0},Block::Lantern);
        world.set({0,41,2},Block::Stone);
        if (draw(true)!=draw(false)) throw std::runtime_error("Edited light grid differs from all-source reference");
    } catch (...) { release(); throw; }
    release();
    std::cout<<"Spatial lighting passed: all-source agreement, negative cell seams, water, models, edits and texture state\n";
}
}
