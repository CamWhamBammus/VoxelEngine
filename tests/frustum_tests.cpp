#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <limits>

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}
} // namespace

int main()
{
    const voxel::Frustum identity{glm::mat4(1.0f)};
    check(identity.intersects(glm::vec3(-0.5f), glm::vec3(0.5f)), "identity accepts interior box");
    check(identity.intersects(glm::vec3(-1.0f), glm::vec3(1.0f)), "identity accepts exact clip cube");
    check(identity.intersects(glm::vec3(-100.0f), glm::vec3(100.0f)),
          "box enclosing frustum remains visible even when its corners are outside");
    check(identity.intersects(glm::vec3(1.0f), glm::vec3(2.0f)), "box touching one clip corner stays visible");
    check(identity.intersects({-5, -0.1f, -0.1f}, {5, 0.1f, 0.1f}),
          "box crossing frustum remains visible");
    for (int axis = 0; axis < 3; ++axis) {
        for (const float sign : {-1.0f, 1.0f}) {
            glm::vec3 center(0.0f);
            center[axis] = 2.0f * sign;
            check(!identity.intersects(center - glm::vec3(0.25f), center + glm::vec3(0.25f)),
                  "identity rejects boxes beyond each of the six clip planes");
            center[axis] = 1.25f * sign;
            check(identity.intersects(center - glm::vec3(0.25f), center + glm::vec3(0.25f)),
                  "identity keeps boxes touching each clip plane");
        }
    }
    check(identity.intersects({1.000001f, 0, 0}, {1.000002f, 0, 0}),
          "small boundary error remains conservatively visible");
    check(!identity.intersects({1.001f, 0, 0}, {1.002f, 0, 0}),
          "box outside boundary tolerance is rejected");
    check(identity.intersects({0, 0, 0}, {0, 0, 0}), "zero-size interior box stays visible");

    const glm::vec3 eye(18.0f, 12.0f, 30.0f);
    const auto projection = glm::perspective(glm::radians(70.0f), 1.5f, 0.1f, 100.0f);
    const auto view = glm::lookAt(eye, eye + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    const voxel::Frustum perspective(projection * view);
    const glm::vec3 ahead = eye + glm::vec3(0, 0, -10);
    const glm::vec3 behind = eye + glm::vec3(0, 0, 10);
    check(perspective.intersects(ahead - glm::vec3(1.0f), ahead + glm::vec3(1.0f)),
          "translated perspective accepts box ahead");
    check(!perspective.intersects(behind - glm::vec3(1.0f), behind + glm::vec3(1.0f)),
          "translated perspective rejects box behind camera");
    const glm::vec3 far = eye + glm::vec3(0, 0, -120);
    check(!perspective.intersects(far - glm::vec3(1.0f), far + glm::vec3(1.0f)),
          "perspective rejects box beyond far plane");
    const glm::vec3 near = eye + glm::vec3(0, 0, -0.02f);
    check(!perspective.intersects(near - glm::vec3(0.005f), near + glm::vec3(0.005f)),
          "perspective rejects box before near plane");
    const glm::vec3 side = eye + glm::vec3(100, 0, -10);
    check(!perspective.intersects(side - glm::vec3(1.0f), side + glm::vec3(1.0f)),
          "perspective rejects offscreen box");
    check(perspective.intersects(eye - glm::vec3(1000.0f), eye + glm::vec3(1000.0f)),
          "perspective keeps box enclosing camera and clip volume");

    const auto turnedView = glm::lookAt(eye, eye + glm::vec3(1, 0, 0), glm::vec3(0, 1, 0));
    const voxel::Frustum turned(projection * turnedView);
    check(turned.intersects(eye + glm::vec3(9, -1, -1), eye + glm::vec3(11, 1, 1)),
          "rotated camera accepts box along new forward axis");
    check(!turned.intersects(ahead - glm::vec3(1.0f), ahead + glm::vec3(1.0f)),
          "rotated camera rejects box along old forward axis");
    for (const float scale : {1e-20f, 1e20f}) {
        const voxel::Frustum scaled(glm::mat4(1.0f) * scale);
        check(scaled.intersects(glm::vec3(-0.5f), glm::vec3(0.5f))
              && !scaled.intersects(glm::vec3(2.0f), glm::vec3(3.0f)),
              "positive homogeneous matrix scaling preserves clip planes");
    }

    const voxel::Frustum zero{glm::mat4(0.0f)};
    check(zero.intersects(glm::vec3(1000.0f), glm::vec3(1001.0f)),
          "zero matrix conservatively accepts geometry");
    auto singular = glm::mat4(1.0f);
    singular[1] = singular[0];
    check(voxel::Frustum(singular).intersects(glm::vec3(1000.0f), glm::vec3(1001.0f)),
          "singular matrix conservatively accepts geometry");
    for (const float invalid : {std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        auto matrix = glm::mat4(1.0f);
        matrix[2][1] = invalid;
        check(voxel::Frustum(matrix).intersects(glm::vec3(1000.0f), glm::vec3(1001.0f)),
              "nonfinite matrix conservatively accepts geometry");
        check(identity.intersects({invalid, 2, 2}, {3, 3, 3}),
              "nonfinite bounds conservatively remain visible");
    }
    check(identity.intersects(glm::vec3(3.0f), glm::vec3(2.0f)),
          "inverted bounds conservatively remain visible");
    return failures == 0 ? 0 : 1;
}
