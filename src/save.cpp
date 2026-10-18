#include "save.h"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace voxel {

namespace {

constexpr std::array<char, 8> Magic{'V', 'X', 'W', 'O', 'R', 'L', 'D', '\0'};
constexpr std::uint32_t Version = 2;
constexpr std::uint32_t PayloadSize = World::ChunkCount * Chunk::Size * Chunk::Size * Chunk::Size;
constexpr std::uint32_t LegacyWidth = 5;
constexpr std::uint32_t LegacyPayloadSize = LegacyWidth * LegacyWidth * Chunk::Size * Chunk::Size * Chunk::Size;
constexpr std::uint32_t ChecksumOffset = 2166136261u;

void hashByte(std::uint32_t& checksum, std::uint8_t byte)
{
    checksum = (checksum ^ byte) * 16777619u;
}

void writeInteger(std::ostream& output, std::uint32_t value, std::uint32_t& checksum)
{
    for (int shift = 0; shift < 32; shift += 8) {
        const auto byte = static_cast<std::uint8_t>(value >> shift);
        output.put(static_cast<char>(byte));
        hashByte(checksum, byte);
    }
}

std::uint32_t readInteger(std::istream& input, std::uint32_t& checksum)
{
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const int byte = input.get();
        if (byte == std::char_traits<char>::eof()) {
            throw std::runtime_error("Truncated world snapshot header or checksum");
        }
        value |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(byte)) << shift;
        hashByte(checksum, static_cast<std::uint8_t>(byte));
    }
    return value;
}

} // namespace

void encodeWorld(const World& world, std::ostream& output)
{
    output.write(Magic.data(), static_cast<std::streamsize>(Magic.size()));
    std::uint32_t checksum = ChecksumOffset;
    // Header order: version, seed, chunk size, world dimensions, payload size.
    // The checksum covers these fields as well as the voxel bytes.
    writeInteger(output, Version, checksum);
    writeInteger(output, world.seed(), checksum);
    writeInteger(output, Chunk::Size, checksum);
    writeInteger(output, World::Width, checksum);
    writeInteger(output, World::Height, checksum);
    writeInteger(output, World::Width, checksum);
    writeInteger(output, PayloadSize, checksum);
    std::vector<char> payload;
    payload.reserve(PayloadSize);
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        const Chunk& chunk = world.chunk(index);
        for (int y = 0; y < Chunk::Size; ++y) {
            for (int z = 0; z < Chunk::Size; ++z) {
                for (int x = 0; x < Chunk::Size; ++x) {
                    const Block block = chunk.get(x, y, z);
                    if (!isValidBlock(block)) {
                        throw std::runtime_error("Cannot save an invalid block material");
                    }
                    const auto byte = static_cast<std::uint8_t>(block);
                    payload.push_back(static_cast<char>(byte));
                    hashByte(checksum, byte);
                }
            }
        }
    }
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    std::uint32_t ignoredChecksum = ChecksumOffset;
    writeInteger(output, checksum, ignoredChecksum);
    if (!output) {
        throw std::runtime_error("Failed to write world snapshot");
    }
}

World decodeWorld(std::istream& input)
{
    std::array<char, Magic.size()> magic{};
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) || magic != Magic) {
        throw std::runtime_error("Invalid world snapshot signature");
    }
    std::uint32_t checksum = ChecksumOffset;
    const auto version = readInteger(input, checksum);
    const auto seed = readInteger(input, checksum);
    const auto chunkSize = readInteger(input, checksum);
    const auto width = readInteger(input, checksum);
    const auto height = readInteger(input, checksum);
    const auto depth = readInteger(input, checksum);
    const auto payloadSize = readInteger(input, checksum);
    if (version != Version && version != 1) {
        throw std::runtime_error("Unsupported world snapshot version");
    }
    const bool legacy = version == 1;
    const auto expectedWidth = legacy ? LegacyWidth : World::Width;
    const auto expectedHeight = legacy ? 1u : World::Height;
    const auto expectedPayload = legacy ? LegacyPayloadSize : PayloadSize;
    if (chunkSize != Chunk::Size || width != expectedWidth || height != expectedHeight
        || depth != expectedWidth || payloadSize != expectedPayload) {
        throw std::runtime_error("World snapshot dimensions do not match this engine");
    }

    // Never allocate from untrusted metadata; this format has a fixed bound.
    std::vector<char> payload(expectedPayload);
    if (!input.read(payload.data(), static_cast<std::streamsize>(payload.size()))) {
        throw std::runtime_error("Truncated world snapshot voxel data");
    }
    for (const char value : payload) {
        const auto byte = static_cast<std::uint8_t>(value);
        if (!isValidBlock(static_cast<Block>(byte))) {
            throw std::runtime_error("Invalid material in world snapshot");
        }
        hashByte(checksum, byte);
    }
    std::uint32_t ignoredChecksum = ChecksumOffset;
    if (readInteger(input, ignoredChecksum) != checksum) {
        throw std::runtime_error("World snapshot checksum mismatch");
    }
    if (input.peek() != std::char_traits<char>::eof() || input.bad()) {
        throw std::runtime_error("Unexpected trailing data or read failure in world snapshot");
    }

    // Validate everything before constructing a replacement for the caller.
    World world(seed);
    std::size_t cursor = 0;
    const std::size_t chunkCount = width * depth * height;
    for (std::size_t index = 0; index < chunkCount; ++index) {
        // Version one stored a centered 5x1x5 region. Keep those coordinates
        // exactly; newly available chunks retain the seed's generated terrain.
        const int chunkX = static_cast<int>(index % width) - static_cast<int>(width / 2);
        const int chunkZ = static_cast<int>((index / width) % depth) - static_cast<int>(depth / 2);
        const int chunkY = static_cast<int>(index / (width * depth));
        const glm::ivec3 origin = glm::ivec3(chunkX, chunkY, chunkZ) * Chunk::Size;
        for (int y = 0; y < Chunk::Size; ++y) {
            for (int z = 0; z < Chunk::Size; ++z) {
                for (int x = 0; x < Chunk::Size; ++x) {
                    world.set(origin + glm::ivec3(x, y, z),
                              static_cast<Block>(static_cast<std::uint8_t>(payload[cursor++])));
                }
            }
        }
    }
    return world;
}

void saveWorld(const World& world, const std::filesystem::path& path)
{
    if (!path.has_filename() || path.filename() == "." || path.filename() == "..") {
        throw std::runtime_error("World save requires a filename");
    }
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    if (!std::filesystem::is_directory(parent)) {
        throw std::runtime_error("World save parent directory does not exist");
    }
    // Complete the snapshot first, write and flush a sibling file, then rename
    // it over the destination. Until that last step the old save stays intact.
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
