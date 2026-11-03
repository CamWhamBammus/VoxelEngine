#include "graphics/benchmark.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "game.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace voxel {
int runRenderBenchmark(const Options& options)
{
    Options scene;
    scene.seed=options.seed;
    Game game(scene);
    Window window(false);
    WorldRenderer renderer;
    renderer.setRenderScale(options.renderScale);
    renderer.setSamples(options.msaa);
    std::cout<<"GPU: "<<glGetString(GL_RENDERER)<<"\nSeed: "<<scene.seed
             <<" | render scale: "<<options.renderScale<<" | MSAA: "<<options.msaa
             <<"\nRender only, fixed time, 4 warmup + 16 measured frames per view.\n"
             <<"view,width,height,median_ms,p95_ms,triangles,chunks,rebuilds\n"<<std::flush;
    struct View { const char* name; glm::vec3 eye,target; };
    constexpr View views[]{
        {"arrival",{2.5f,21,77.5f},{8,65,8}},
        {"away",{2.5f,21,77.5f},{2.5f,25,111}},
        {"tree",{62,70,81},{8,70,8}},
        {"hall",{20,12,35},{7,28,9}}
    };
    for (const auto size:{glm::ivec2(1280,720),glm::ivec2(2560,1440)}) {
        for (const auto& view:views) {
            game.camera.setPosition(view.eye);
            const auto direction=glm::normalize(view.target-view.eye),old=game.camera.forward();
            game.camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                               glm::degrees(std::asin(direction.y)-std::asin(old.y)));
            game.camera.setFieldOfView(72);
            std::vector<double> times;
            std::size_t rebuilds=0;
            for (int frame=0;frame<20;++frame) {
                // Include completed GPU work, not just asynchronous submission.
                glFinish();
                const auto start=std::chrono::steady_clock::now();
                renderer.render(game.world,game.camera,size.x,size.y,false,true,1,false,true);
                glFinish();
                const auto end=std::chrono::steady_clock::now();
                if (frame==3) rebuilds=renderer.sceneRebuildCount();
                if (frame>=4) times.push_back(std::chrono::duration<double,std::milli>(end-start).count());
            }
            std::sort(times.begin(),times.end());
            std::cout<<view.name<<','<<size.x<<','<<size.y<<','<<std::fixed<<std::setprecision(2)
                     <<(times[7]+times[8])*.5<<','<<times[15]<<','<<renderer.drawnTriangles()<<','
                     <<renderer.visibleChunks()<<','<<renderer.sceneRebuildCount()-rebuilds<<std::endl;
        }
    }
    return 0;
}
}
