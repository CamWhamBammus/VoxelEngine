#include "voxel/world_codec.h"

#include <array>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {

constexpr std::array<char, 8> Magic{'V', 'X', 'W', 'O', 'R', 'L', 'D', '\0'};
constexpr std::uint32_t Version = 1;
constexpr std::uint32_t PayloadSize = World::ChunkCount * Chunk::Size * Chunk::Size * Chunk::Size;
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
    // The checksum covers metadata as well as voxel bytes, including the seed.
    for (const auto value : {Version, world.seed(), static_cast<std::uint32_t>(Chunk::Size),
                             static_cast<std::uint32_t>(World::Width), 1u,
                             static_cast<std::uint32_t>(World::Width), PayloadSize}) {
        writeInteger(output, value, checksum);
    }
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
    std::uint32_t unused = ChecksumOffset;
    writeInteger(output, checksum, unused);
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
    if (version != Version) {
        throw std::runtime_error("Unsupported world snapshot version");
    }
    if (chunkSize != Chunk::Size || width != World::Width || height != 1
        || depth != World::Width || payloadSize != PayloadSize) {
        throw std::runtime_error("World snapshot dimensions do not match this engine");
    }

    // Never allocate from untrusted metadata; this format has a fixed bound.
    std::vector<char> payload(PayloadSize);
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
    std::uint32_t unused = ChecksumOffset;
    if (readInteger(input, unused) != checksum) {
        throw std::runtime_error("World snapshot checksum mismatch");
    }
    if (input.peek() != std::char_traits<char>::eof() || input.bad()) {
        throw std::runtime_error("Unexpected trailing data or read failure in world snapshot");
    }

    World world(seed);
    std::size_t cursor = 0;
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        const glm::ivec3 origin = world.origin(index);
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

} // namespace voxel
