#include "editing.h"
#include "world.h"

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
    constexpr std::uint32_t seed = 1337;
    voxel::World world(seed);
    const glm::vec3 camera(-0.5f, 20.0f, -0.5f);
    const auto hit = voxel::raycast(camera, {0, -1, 0}, 30.0f,
                                    [&world](glm::ivec3 position) { return world.get(position); });
    check(hit.has_value(), "downward ray must target generated terrain");
    if (!hit) {
        return 1;
    }
    check(hit->position == glm::ivec3(-1, voxel::terrainHeight(-1, -1, seed), -1),
          "editing ray must target the expected surface voxel");
    const auto original = world.get(hit->position);
    check(voxel::removeBlock(world, *hit), "raycast target must be removable");
    check(world.get(hit->position) == voxel::Block::Air, "removal must replace target with air");
    check(!voxel::removeBlock(world, *hit), "removing an already empty target must be a no-op");
    check(!voxel::placeBlock(world, *hit, voxel::Block::Sand, camera),
          "placement must reject a stale hit whose target was removed");
    world.set(hit->position, original);

    const glm::ivec3 destination = hit->position + hit->normal;
    check(voxel::placeBlock(world, *hit, voxel::Block::Sand, camera),
          "raycast face must accept placement into its empty adjacent cell");
    check(world.get(destination) == voxel::Block::Sand, "placement must use selected material");
    check(!voxel::placeBlock(world, *hit, voxel::Block::Stone, camera),
          "placement must not overwrite an occupied destination");
    world.set(destination, voxel::Block::Air);
    check(!voxel::placeBlock(world, *hit, voxel::Block::Air, camera),
          "placement must reject air as a material");
    check(!voxel::placeBlock(world, *hit, voxel::Block::Stone,
                            glm::vec3(destination) + glm::vec3(0.25f)),
          "placement must reject the camera cell, including negative coordinates");
    check(world.get(destination) == voxel::Block::Air, "rejected edits must preserve destination");

    for (const auto normal : {glm::ivec3(0), glm::ivec3(1, 1, 0), glm::ivec3(2, 0, 0),
                              glm::ivec3(std::numeric_limits<int>::min(), 0, 0)}) {
        const voxel::RayHit invalid{hit->position, normal, hit->distance};
        check(!voxel::placeBlock(world, invalid, voxel::Block::Stone, camera),
              "placement must require an exact axis-aligned unit normal");
    }
    const auto startInside = voxel::raycast(glm::vec3(hit->position) + glm::vec3(0.5f),
                                            {1, 0, 0}, 1.0f,
                                            [&world](glm::ivec3 position) { return world.get(position); });
    check(startInside && !voxel::placeBlock(world, *startInside, voxel::Block::Dirt, camera),
          "starting-solid hits must not supply an arbitrary placement face");

    for (const auto boundary : {
             voxel::RayHit{{-32, 0, 0}, {-1, 0, 0}, 1.0f},
             voxel::RayHit{{47, 0, 0}, {1, 0, 0}, 1.0f},
             voxel::RayHit{{0, 0, -32}, {0, 0, -1}, 1.0f},
             voxel::RayHit{{0, 0, 47}, {0, 0, 1}, 1.0f},
             voxel::RayHit{{0, 0, 0}, {0, -1, 0}, 1.0f},
             voxel::RayHit{{0, 15, 0}, {0, 1, 0}, 1.0f}}) {
        world.set(boundary.position, voxel::Block::Stone);
        check(!voxel::placeBlock(world, boundary, voxel::Block::Grass, camera),
              "placement outside each finite world boundary must fail");
    }
    for (const int coordinate : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        const voxel::RayHit invalid{{coordinate, 0, 0}, {coordinate < 0 ? -1 : 1, 0, 0}, 1.0f};
        check(!voxel::placeBlock(world, invalid, voxel::Block::Stone, camera),
              "extreme hit coordinates must fail without integer overflow");
        check(!voxel::removeBlock(world, invalid), "removal outside the world must fail");
    }
    check(!voxel::placeBlock(world, *hit, voxel::Block::Stone,
                            {std::numeric_limits<float>::infinity(), 0, 0}),
          "nonfinite camera position must not permit placement");
    voxel::World trackedWorld;
    voxel::EditHistory history;
    const voxel::RayHit tracked{{0, 14, 0}, {0, 1, 0}, 1.0f};
    trackedWorld.set(tracked.position, voxel::Block::Stone);
    check(voxel::removeBlock(trackedWorld, tracked, &history), "editor removal records history");
    check(history.undo(trackedWorld) && trackedWorld.get(tracked.position) == voxel::Block::Stone,
          "editor removal can be undone");
    check(voxel::placeBlock(trackedWorld, tracked, voxel::Block::Sand, {5, 15, 5}, &history),
          "editor placement records history");
    check(history.undo(trackedWorld) && trackedWorld.get({0, 15, 0}) == voxel::Block::Air,
          "editor placement can be undone");
    check(history.redo(trackedWorld) && trackedWorld.get({0, 15, 0}) == voxel::Block::Sand,
          "editor placement can be redone with its material");

    using voxel::PlacementFailure;
    voxel::World previewWorld;
    const voxel::RayHit previewHit{{-1, 14, 1}, {0, 1, 0}, 1.0f};
    previewWorld.set(previewHit.position, voxel::Block::Stone);
    previewWorld.takeDirtyChunks();
    const voxel::World beforePreview = previewWorld;
    const glm::vec3 previewCamera{5, 15, 5};
    const auto preview = voxel::checkPlacement(previewWorld, previewHit, voxel::Block::Sand, previewCamera);
    check(preview.failure == PlacementFailure::None && preview.position == glm::ivec3(-1, 15, 1),
          "placement preview returns the exact editable destination");
    for (const auto invalid : {voxel::Block::Air, static_cast<voxel::Block>(255)}) {
        check(voxel::checkPlacement(previewWorld, previewHit, invalid, previewCamera).failure
                  == PlacementFailure::InvalidMaterial, "preview explains invalid materials");
    }
    check(voxel::checkPlacement(previewWorld, {{2, 15, 2}, {0, 1, 0}, 1.0f},
                                voxel::Block::Sand, previewCamera).failure == PlacementFailure::MissingTarget,
          "preview explains an absent or stale target");
    for (const auto normal : {glm::ivec3(0), glm::ivec3(1, 1, 0), glm::ivec3(2, 0, 0)}) {
        const voxel::RayHit invalid{previewHit.position, normal, 1.0f};
        check(voxel::checkPlacement(previewWorld, invalid, voxel::Block::Sand, previewCamera).failure
                  == PlacementFailure::InvalidFace, "preview explains invalid target faces");
    }
    check(voxel::checkPlacement(previewWorld, previewHit, voxel::Block::Sand,
                                {0, std::numeric_limits<float>::quiet_NaN(), 0}).failure
              == PlacementFailure::InvalidCamera, "preview rejects nonfinite camera positions");
    check(voxel::checkPlacement(previewWorld, {{0, 0, 0}, {0, -1, 0}, 1.0f},
                                voxel::Block::Sand, previewCamera).failure == PlacementFailure::OutsideWorld,
          "preview distinguishes outside-world air from editable air");
    check(voxel::checkPlacement(previewWorld, {{0, 0, 0}, {0, 1, 0}, 1.0f},
                                voxel::Block::Sand, previewCamera).failure == PlacementFailure::Occupied,
          "preview explains occupied destination cells");
    check(voxel::checkPlacement(previewWorld, previewHit, voxel::Block::Sand,
                                {-0.25f, 15.5f, 1.5f}).failure == PlacementFailure::CameraOverlap,
          "preview explains camera overlap with correct negative-coordinate rounding");
    check(previewWorld.sameContents(beforePreview) && previewWorld.takeDirtyChunks().empty(),
          "successful and rejected previews never edit blocks or dirty meshes");
    voxel::EditHistory previewHistory;
    check(voxel::placeBlock(previewWorld, previewHit, voxel::Block::Sand, previewCamera, &previewHistory)
          && previewWorld.get(preview.position) == voxel::Block::Sand && previewHistory.undoCount() == 1,
          "actual placement uses the validated position and records exactly one edit");
    const auto afterPlacement = previewWorld;
    previewWorld.takeDirtyChunks();
    check(!voxel::placeBlock(previewWorld, previewHit, voxel::Block::Grass, previewCamera, &previewHistory)
          && previewWorld.sameContents(afterPlacement) && previewHistory.undoCount() == 1
          && previewWorld.takeDirtyChunks().empty(),
          "rejected placement leaves contents, history, and mesh state unchanged");
    return failures == 0 ? 0 : 1;
}
