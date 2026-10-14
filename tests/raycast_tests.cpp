#include "voxel/raycast.h"

#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            ++failures;
        }
    };
    const auto at = [](glm::ivec3 target) {
        return [target](glm::ivec3 position) {
            return position == target ? voxel::Block::Stone : voxel::Block::Air;
        };
    };
    const auto air = [](glm::ivec3) { return voxel::Block::Air; };
    const auto stone = [](glm::ivec3) { return voxel::Block::Stone; };

    for (int axis = 0; axis < 3; ++axis) {
        for (const int sign : {-1, 1}) {
            glm::vec3 direction(0.0f);
            direction[axis] = static_cast<float>(sign * 7);
            glm::ivec3 target(0);
            target[axis] = sign * 2;
            glm::ivec3 normal(0);
            normal[axis] = -sign;
            const auto hit = voxel::raycast(glm::vec3(0.5f), direction, 10.0f, at(target));
            check(hit.has_value(), "axis-aligned ray must find its target in either direction");
            if (hit) {
                check(hit->position == target, "ray must report the target voxel");
                check(hit->normal == normal, "hit normal must oppose the incoming ray");
                check(std::abs(hit->distance - 1.5f) < 0.00001f,
                      "ray distance must use world units independent of direction magnitude");
            }
        }
    }
    check(!voxel::raycast(glm::vec3(0.5f), {1, 0, 0}, 5.0f, air), "empty world must miss");
    check(!voxel::raycast(glm::vec3(0.5f), {1, 0, 0}, 1.49f, at({2, 0, 0})),
          "hit past maxDistance must be excluded");
    check(voxel::raycast(glm::vec3(0.5f), {1, 0, 0}, 1.5f, at({2, 0, 0})).has_value(),
          "hit exactly at maxDistance must be included");
    const auto inside = voxel::raycast({-0.2f, -1.2f, -2.2f}, {0, 1, 0}, 0.0f, stone);
    check(inside && inside->position == glm::ivec3(-1, -2, -3)
          && inside->normal == glm::ivec3(0) && inside->distance == 0.0f,
          "starting solid must return floor coordinates with zero distance and normal");
    const auto boundary = voxel::raycast({0, 0.5f, 0.5f}, {-1, 0, 0}, 0.0f, at({-1, 0, 0}));
    check(boundary && boundary->distance == 0.0f && boundary->normal == glm::ivec3(1, 0, 0),
          "negative ray on a boundary must enter the preceding cell immediately");

    const auto diagonal = voxel::raycast(glm::vec3(0.5f), {1, 1, 1}, 4.0f, at({1, 1, 1}));
    check(diagonal && diagonal->position == glm::ivec3(1)
          && diagonal->normal == glm::ivec3(-1, 0, 0)
          && std::abs(diagonal->distance - std::sqrt(0.75f)) < 0.00001f,
          "corner crossings must advance every tied axis and use X normal first");
    check(!voxel::raycast(glm::vec3(0.5f), {1, 1, 0}, 3.0f, at({1, 0, 0})),
          "edge crossings must skip voxels touched only along an edge");

    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    check(!voxel::raycast(glm::vec3(0), glm::vec3(0), 1.0f, stone), "zero direction must be rejected");
    check(!voxel::raycast(glm::vec3(0), {1, 0, 0}, -1.0f, stone), "negative distance must be rejected");
    check(!voxel::raycast(glm::vec3(0), {1, 0, 0}, infinity, stone), "infinite distance must be rejected");
    check(!voxel::raycast(glm::vec3(0), {1, 0, 0}, nan, stone), "NaN distance must be rejected");
    check(!voxel::raycast({infinity, 0, 0}, {1, 0, 0}, 1.0f, stone), "infinite origin must be rejected");
    check(!voxel::raycast(glm::vec3(0), {0, nan, 0}, 1.0f, stone), "NaN direction must be rejected");
    check(!voxel::raycast(glm::vec3(0), {0, infinity, 0}, 1.0f, stone), "infinite direction must be rejected");
    check(!voxel::raycast({std::numeric_limits<float>::max(), 0, 0}, {1, 0, 0}, 1.0f, stone),
          "origin beyond integer voxel coordinates must be rejected");
    check(!voxel::raycast(glm::vec3(0), {1, 0, 0}, 1.0f, {}), "empty sampler must be rejected");
    check(!voxel::raycast({static_cast<float>(std::numeric_limits<int>::min()), 0, 0},
                         {-1, 0, 0}, 1.0f, air), "traversal must stop at integer coordinate limits");
    return failures == 0 ? 0 : 1;
}
