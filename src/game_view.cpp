#include "game.h"

#include <algorithm>
#include <cmath>

namespace voxel {

Camera Game::viewCamera() const
{
    auto view = camera;
    if (!healthVisible() || health_.dead()) return view;
    const float flash = std::clamp(health_.damageFlash(), 0.0f, 1.0f);
    if (flash == 0) return view;

    // A small impact followed by one settling motion. Only the rendered copy
    // moves: the mouse aim, movement bearing and eye collision stay unchanged.
    const float kick = flash * flash * std::cos((1.0f - flash) * 4.71238898f);
    auto forward = camera.forward();
    forward.y = 0;
    forward = glm::normalize(forward);
    const auto right = glm::cross(forward, glm::vec3(0,1,0));
    const auto direction = playerHitDirection();
    view.rotate(0, (-1.15f + .45f * glm::dot(direction, forward)) * kick);
    view.setRoll(-2.0f * glm::dot(direction, right) * kick);
    return view;
}

} // namespace voxel
