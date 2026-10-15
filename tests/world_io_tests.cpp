#include "voxel/world_io.h"
#include "voxel/world_codec.h"

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
        const auto pattern = (std::filesystem::temp_directory_path() / "voxel-io-test-XXXXXX").string();
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back('\0');
        if (!::mkdtemp(name.data())) {
            throw std::runtime_error("Cannot create test directory");
        }
        path = name.data();
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

std::string encode(const voxel::World& world)
{
    std::ostringstream stream(std::ios::binary);
    voxel::encodeWorld(world, stream);
    return stream.str();
}

template<typename Operation>
bool fails(Operation operation)
{
    try {
        operation();
        return false;
    } catch (const std::exception&) {
        return true;
    }
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
    const auto path = directory.path / "edited world.vxl";
    voxel::World original(76);
    original.set({-32, 15, -32}, voxel::Block::Sand);
    original.set({47, 15, 47}, voxel::Block::Dirt);
    voxel::saveWorld(original, path);
    check(encode(voxel::loadWorld(path)) == encode(original), "saved world must round trip seed and edits");

    voxel::World replacement(908);
    replacement.set({-1, 15, -1}, voxel::Block::Stone);
    voxel::saveWorld(replacement, path);
    check(encode(voxel::loadWorld(path)) == encode(replacement), "save must replace an existing snapshot");
    check(std::distance(std::filesystem::directory_iterator(directory.path),
                        std::filesystem::directory_iterator()) == 1,
          "successful saves must not leave temporary files behind");

    check(fails([&] { voxel::saveWorld(original, directory.path / "missing" / "world.vxl"); }),
          "save must explicitly fail when its parent is missing");
    check(!std::filesystem::exists(directory.path / "missing"), "save must not implicitly create directories");
    check(fails([&] { voxel::saveWorld(original, path / "world.vxl"); }),
          "save must fail when parent is an existing regular file");
    check(encode(voxel::loadWorld(path)) == encode(replacement),
          "failed save must preserve an existing file used as an invalid parent");

    const auto blocked = directory.path / "blocked.vxl";
    std::filesystem::create_directory(blocked);
    const auto sentinel = blocked / "keep.txt";
    { std::ofstream output(sentinel); output << "preserve"; }
    check(fails([&] { voxel::saveWorld(original, blocked); }),
          "rename failure must be reported instead of deleting its destination");
    std::ifstream sentinelInput(sentinel);
    std::string sentinelText;
    sentinelInput >> sentinelText;
    check(sentinelText == "preserve", "failed rename must preserve existing destination contents");
    for (const auto& entry : std::filesystem::directory_iterator(directory.path)) {
        check(entry.path().filename().string().find(".voxel-save-") != 0,
              "failed saves must clean their temporary files");
    }

    check(fails([&] { static_cast<void>(voxel::loadWorld(directory.path / "absent.vxl")); }),
          "missing snapshots must fail to load");
    check(fails([&] { static_cast<void>(voxel::loadWorld(blocked)); }), "directories must fail to load");
    check(fails([&] { voxel::saveWorld(original, directory.path / "."); }),
          "save must require a real filename");

    const auto corrupt = directory.path / "corrupt.vxl";
    { std::ofstream output(corrupt, std::ios::binary); output << "truncated"; }
    const auto before = encode(original);
    check(fails([&] { original = voxel::loadWorld(corrupt); }), "corrupt snapshots must fail to load");
    check(encode(original) == before, "failed file load must leave the active world unchanged");
    { std::ofstream output(path, std::ios::binary | std::ios::app); output << "trailing"; }
    check(fails([&] { static_cast<void>(voxel::loadWorld(path)); }), "file loader must reject trailing data");
    return failures == 0 ? 0 : 1;
}
