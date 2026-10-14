#include "core/camera.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(float actual, float expected, float tolerance = 0.0001f)
{
    return std::abs(actual - expected) < tolerance;
}

bool near(const glm::vec3& actual, const glm::vec3& expected)
{
    return glm::length(actual - expected) < 0.0001f;
}
} // namespace

int main()
{
    voxel::Camera camera;
    check(near(camera.position(), {28.0f, 26.0f, 40.0f}), "default camera position");
    check(near(glm::length(camera.forward()), 1.0f), "forward is normalized");
    check(camera.forward().x < 0.0f && camera.forward().y < 0.0f
        && camera.forward().z < 0.0f, "default camera faces toward the terrain");

    const glm::vec4 eyeInView = camera.viewMatrix() * glm::vec4(camera.position(), 1.0f);
    check(near(glm::vec3(eyeInView), glm::vec3(0.0f)), "view maps eye to origin");
    const glm::vec4 aheadInView = camera.viewMatrix()
        * glm::vec4(camera.position() + camera.forward(), 1.0f);
    check(near(glm::vec3(aheadInView), {0.0f, 0.0f, -1.0f}),
        "view maps forward to negative z");

    camera.setPosition(glm::vec3(0.0f));
    camera.move({0.0f, 0.0f, 1.0f}, 5.0f);
    check(near(camera.position(), camera.forward() * 5.0f), "forward movement");
    camera.setPosition(glm::vec3(0.0f));
    camera.move({1.0f, 1.0f, 1.0f}, 5.0f);
    check(near(glm::length(camera.position()), 5.0f), "diagonal movement has constant speed");
    const glm::vec3 beforeZeroMove = camera.position();
    camera.move(glm::vec3(0.0f), 5.0f);
    check(near(camera.position(), beforeZeroMove), "zero input does not move camera");
    camera.setPosition(glm::vec3(0.0f));
    camera.move({0.0f, 1.0f, 0.0f}, 2.0f);
    check(near(camera.position(), {0.0f, 2.0f, 0.0f}), "vertical movement uses world up");
    camera.setPosition(glm::vec3(0.0f));
    camera.move({1.0f, 0.0f, 0.0f}, 2.0f);
    check(near(glm::dot(camera.position(), camera.forward()), 0.0f)
        && near(camera.position().y, 0.0f)
        && near(glm::length(camera.position()), 2.0f), "strafe is horizontal and perpendicular");

    camera.rotate(125.0f, 25.0f);
    check(near(camera.forward(), {1.0f, 0.0f, 0.0f}), "yaw and pitch use degrees");
    camera.rotate(0.0f, 1000.0f);
    check(near(camera.forward().y, std::sin(glm::radians(89.0f)))
        && camera.forward().x > 0.0f, "positive pitch stops short of vertical");
    camera.rotate(0.0f, -2000.0f);
    check(near(camera.forward().y, -std::sin(glm::radians(89.0f)))
        && camera.forward().x > 0.0f, "negative pitch stops short of vertical");

    const glm::mat4 projection = camera.projectionMatrix(16.0f / 9.0f);
    check(near(projection[0][0] * (16.0f / 9.0f), projection[1][1]),
        "projection accounts for aspect ratio");
    check(near(projection[1][1], 1.0f / std::tan(glm::radians(35.0f))),
        "projection uses a 70 degree vertical field of view");
    for (const float aspect : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()}) {
        const glm::mat4 safeProjection = camera.projectionMatrix(aspect);
        check(std::isfinite(safeProjection[0][0])
            && near(safeProjection[0][0], safeProjection[1][1]),
            "invalid aspect ratio falls back to a square viewport");
    }

    if (failures == 0) {
        std::cout << "Camera tests passed\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
