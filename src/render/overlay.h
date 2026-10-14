#pragma once

#include "render/shader.h"
#include "voxel/raycast.h"

namespace voxel {
class Camera;

class Overlay {
public:
    Overlay();
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    void draw(const std::optional<RayHit>& hit, const Camera& camera, int width, int height);

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint buffer_ = 0;
};

} // namespace voxel
