#include "graphics/held_pickaxe.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace voxel {

glm::mat4 heldPickaxePose(float swingRemaining, float aspectRatio)
{
    if (!std::isfinite(swingRemaining)) swingRemaining=0;
    if (!std::isfinite(aspectRatio) || aspectRatio<=0) aspectRatio=16.0f/9.0f;
    const float aspect=std::clamp(aspectRatio,.6f,3.6f);
    const float t=1.0f-std::clamp(swingRemaining,0.0f,1.0f);
    struct Key { float time; glm::vec3 position; glm::vec3 rotation; };
    // Raise the head towards the shoulder, drive it forward and down, then
    // recoil before returning to the grip. Smooth endpoints avoid a snap when
    // holding the mining button for repeated strikes.
    const std::array<Key,6> keys{{
        {0.00f,{0,0,0},{-11,-25,-17}},
        {0.26f,{.015f,.055f,.025f},{22,-35,-25}},
        {0.53f,{-.13f,-.055f,-.19f},{-68,0,12}},
        {0.64f,{-.10f,-.025f,-.12f},{-47,-5,7}},
        {0.79f,{-.05f,-.035f,-.055f},{-30,-14,1}},
        {1.00f,{0,0,0},{-11,-25,-17}}
    }};
    glm::vec3 position{}, rotation{};
    for (std::size_t i=1; i<keys.size(); ++i) {
        if (t<=keys[i].time) {
            float f=(t-keys[i-1].time)/(keys[i].time-keys[i-1].time);
            f=f*f*(3-2*f);
            position=glm::mix(keys[i-1].position,keys[i].position,f);
            rotation=glm::mix(keys[i-1].rotation,keys[i].rotation,f);
            break;
        }
    }
    position+=glm::vec3{.245f*aspect,-.365f,-1.19f};
    auto model=glm::translate(glm::mat4(1),position);
    model=glm::rotate(model,glm::radians(rotation.z),{0,0,1});
    model=glm::rotate(model,glm::radians(rotation.y),{0,1,0});
    model=glm::rotate(model,glm::radians(rotation.x),{1,0,0});
    // A broad pick head needs a little more room than a blade in tall windows.
    const float scale=.85f*std::min(1.0f,aspect/(4.0f/3.0f));
    return glm::scale(model,glm::vec3(scale));
}

} // namespace voxel
