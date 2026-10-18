#include "mesh.h"
#include "editing.h"
#include "save.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

struct TemporaryDirectory {
    std::filesystem::path path;

    TemporaryDirectory()
    {
        const auto pattern = (std::filesystem::temp_directory_path() / "voxel-session-XXXXXX").string();
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back('\0');
        if (!::mkdtemp(name.data())) {
            throw std::runtime_error("Cannot create session test directory");
        }
        path = name.data();
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

std::string snapshot(const voxel::World& world)
{
    std::ostringstream stream(std::ios::binary);
    voxel::encodeWorld(world, stream);
    return stream.str();
}

bool sameMesh(const voxel::MeshData& first, const voxel::MeshData& second)
{
    if (first.indices != second.indices || first.vertices.size() != second.vertices.size()) {
        return false;
    }
    for (std::size_t index = 0; index < first.vertices.size(); ++index) {
        const auto& a = first.vertices[index];
        const auto& b = second.vertices[index];
        if (a.position != b.position || a.normal != b.normal || a.color != b.color) {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            ++failures;
        }
    };
    TemporaryDirectory directory;
    const auto savePath = directory.path / "session.vxl";
    voxel::World world(7654321);
    voxel::EditHistory history;
    check(world.takeDirtyChunks().size() == voxel::World::ChunkCount,
          "new session must initially upload every chunk");
    check(world.takeDirtyChunks().empty(), "consuming initial uploads must clear dirty chunks");

    // The top layer is above generated terrain; these cells straddle x=0.
    const glm::ivec3 left(-1, 14, 1), right(0, 14, 1);
    const glm::vec3 camera(1.5f, 14.5f, 1.5f);
    const std::vector<std::size_t> seamChunks{39, 40};
    const auto target = [&]() {
        return voxel::raycast(camera, {-1, 0, 0}, 8.0f,
                              [&world](glm::ivec3 position) { return world.get(position); });
    };
    check(history.apply(world, left, voxel::Block::Sand), "session must accept the first seam edit");
    check(world.takeDirtyChunks() == seamChunks, "first seam edit must invalidate both chunk meshes");
    const auto firstHit = target();
    check(firstHit && firstHit->position == left && firstHit->normal == glm::ivec3(1, 0, 0),
          "raycast must find the edited negative-coordinate voxel");
    if (!firstHit) {
        return 1;
    }
    check(voxel::placeBlock(world, *firstHit, voxel::Block::Stone, camera, &history),
          "placement through a raycast face must cross the chunk seam");
    const auto placedHit = target();
    check(placedHit && placedHit->position == right && history.undoCount() == 2,
          "placed voxel must become the nearest ray target and enter edit history");
    if (!placedHit) {
        return 1;
    }
    const auto savedLeftMesh = voxel::buildMesh(world, 39);
    const auto savedRightMesh = voxel::buildMesh(world, 40);
    const auto savedState = snapshot(world);
    voxel::saveWorld(world, savePath);
    check(history.undoCount() == 2 && history.redoCount() == 0,
          "saving must preserve the active undo history");
    check(world.takeDirtyChunks() == seamChunks,
          "saving must preserve pending mesh invalidations until rendering consumes them");
    check(world.takeDirtyChunks().empty(), "consuming edit uploads must clear dirty chunks");

    check(history.undo(world) && world.get(right) == voxel::Block::Air,
          "undo after saving must remove the placed seam voxel");
    const auto undoneHit = target();
    check(undoneHit && undoneHit->position == left, "undo must update subsequent ray targets");
    check(world.takeDirtyChunks() == seamChunks, "undo must invalidate both meshes at the seam");
    check(!sameMesh(voxel::buildMesh(world, 39), savedLeftMesh) && !sameMesh(voxel::buildMesh(world, 40), savedRightMesh),
          "undo must expose the neighboring face and remove the placed voxel geometry");
    check(history.redo(world) && snapshot(world) == savedState,
          "redo must restore exactly the saved world contents");
    check(world.takeDirtyChunks() == seamChunks, "redo must request both seam mesh rebuilds");
    check(voxel::removeBlock(world, *placedHit, &history), "removal must also join the session history");
    check(world.get(right) == voxel::Block::Air && history.undoCount() == 3,
          "post-save removal must change the world without altering its saved snapshot");

    // Match the application's successful reload transition: replace first,
    // then clear history so edits from the previous state cannot be replayed.
    world = voxel::loadWorld(savePath);
    history.clear();
    check(world.seed() == 7654321 && snapshot(world) == savedState,
          "reload must restore seed, materials, and saved edits exactly");
    check(history.undoCount() == 0 && history.redoCount() == 0,
          "successful reload must start with cleared session history");
    check(world.takeDirtyChunks().size() == voxel::World::ChunkCount,
          "loaded world must replace every previously cached chunk mesh");
    check(world.takeDirtyChunks().empty(), "loaded mesh invalidations must clear after upload");
    check(sameMesh(voxel::buildMesh(world, 39), savedLeftMesh) && sameMesh(voxel::buildMesh(world, 40), savedRightMesh),
          "saved seam edits must reconstruct identical positions, normals, colors, and indices");
    const auto loadedHit = target();
    check(loadedHit && loadedHit->position == right && world.get(left) == voxel::Block::Sand
          && world.get(right) == voxel::Block::Stone, "loaded edits must remain raycastable with their materials");
    if (!loadedHit) {
        return 1;
    }

    check(voxel::removeBlock(world, *loadedHit, &history), "editing must continue after reload");
    check(history.apply(world, left, voxel::Block::Dirt) && history.undo(world),
          "session must retain both undo and redo branches before a failed reload");
    world.takeDirtyChunks();
    const auto beforeFailure = snapshot(world);
    const auto corruptPath = directory.path / "corrupt.vxl";
    { std::ofstream output(corruptPath, std::ios::binary); output << "invalid snapshot"; }
    bool rejected = false;
    try {
        world = voxel::loadWorld(corruptPath);
        history.clear();
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected && snapshot(world) == beforeFailure,
          "failed reload must preserve the entire active edited world");
    check(history.undoCount() == 1 && history.redoCount() == 1 && world.takeDirtyChunks().empty(),
          "failed reload must preserve undo, redo, and consumed mesh state");
    check(history.redo(world) && world.get(left) == voxel::Block::Dirt,
          "redo must still operate after a failed reload");
    check(history.undo(world) && history.undo(world) && snapshot(world) == savedState,
          "undo must still restore the saved session after a failed reload");

    check(history.apply(world, left, voxel::Block::Grass), "fresh edit must branch after recovery");
    const auto beforeSaveFailure = snapshot(world);
    rejected = false;
    try {
        voxel::saveWorld(world, directory.path / "missing" / "session.vxl");
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected && snapshot(world) == beforeSaveFailure && history.undoCount() == 1,
          "failed save must preserve edits and undo history");
    check(world.takeDirtyChunks() == seamChunks, "failed save must preserve pending mesh updates");
    check(snapshot(voxel::loadWorld(savePath)) == savedState,
          "failed save must leave the previously saved session loadable and unchanged");
    return failures == 0 ? 0 : 1;
}
