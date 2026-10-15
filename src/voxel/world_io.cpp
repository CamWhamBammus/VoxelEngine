#include "voxel/world_io.h"

#include "voxel/world_codec.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace voxel {

void saveWorld(const World& world, const std::filesystem::path& path)
{
    if (!path.has_filename() || path.filename() == "." || path.filename() == "..") {
        throw std::runtime_error("World save requires a filename");
    }
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    if (!std::filesystem::is_directory(parent)) {
        throw std::runtime_error("World save parent directory does not exist");
    }
    std::ostringstream encoded(std::ios::binary);
    encodeWorld(world, encoded);
    const std::string bytes = encoded.str();

    // mkstemp creates the sibling file exclusively, avoiding collisions and
    // symlink races even when multiple processes save at the same time.
    const auto pattern = (parent / ".voxel-save-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    int descriptor = ::mkstemp(name.data());
    if (descriptor < 0) {
        throw std::system_error(errno, std::generic_category(), "Cannot create temporary world save");
    }
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "Cannot write world save");
            }
            if (written == 0) {
                throw std::runtime_error("World save write made no progress");
            }
            offset += static_cast<std::size_t>(written);
        }
        if (::fsync(descriptor) != 0) {
            throw std::system_error(errno, std::generic_category(), "Cannot flush world save");
        }
        const int closeResult = ::close(descriptor);
        descriptor = -1;
        if (closeResult != 0) {
            throw std::system_error(errno, std::generic_category(), "Cannot close world save");
        }
        std::filesystem::rename(std::filesystem::path(name.data()), path);
    } catch (...) {
        if (descriptor >= 0) {
            ::close(descriptor);
        }
        ::unlink(name.data());
        throw;
    }
}

World loadWorld(const std::filesystem::path& path)
{
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("World save is not a regular file: " + path.string());
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open world save: " + path.string());
    }
    return decodeWorld(input);
}

} // namespace voxel
