#include "save.h"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string encode(const voxel::World& world)
{
    std::ostringstream stream(std::ios::binary);
    voxel::encodeWorld(world, stream);
    return stream.str();
}

void writeInteger(std::string& bytes, std::size_t offset, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        bytes[offset++] = static_cast<char>(value >> shift);
    }
}

void repairChecksum(std::string& bytes)
{
    std::uint32_t checksum = 2166136261u;
    for (std::size_t index = 8; index < bytes.size() - 4; ++index) {
        checksum = (checksum ^ static_cast<std::uint8_t>(bytes[index])) * 16777619u;
    }
    writeInteger(bytes, bytes.size() - 4, checksum);
}

bool rejects(const std::string& bytes)
{
    try {
        std::istringstream stream(bytes, std::ios::binary);
        static_cast<void>(voxel::decodeWorld(stream));
        return false;
    } catch (const std::runtime_error&) {
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
    for (const std::uint32_t seed : {0u, 1337u, 0x89abcdefu}) {
        voxel::World source(seed);
        source.takeDirtyChunks();
        source.set({-32, 0, -32}, voxel::Block::Air);
        source.set({-1, 15, -1}, voxel::Block::Grass);
        source.set({0, 15, 0}, voxel::Block::Dirt);
        source.set({16, 15, 16}, voxel::Block::Stone);
        source.set({47, 15, 47}, voxel::Block::Sand);
        for (std::size_t value = 0; value < voxel::BlockCount; ++value) {
            const auto block = static_cast<voxel::Block>(value);
            source.set({-32 + static_cast<int>(value), 3, -32}, block);
            source.set({32 + static_cast<int>(value), 13, 47}, block);
        }
        const auto bytes = encode(source);
        check(bytes.size() == 36 + 25 * 16 * 16 * 16 + 4, "snapshot must use compact fixed byte layout");
        check(static_cast<std::uint8_t>(bytes[12]) == static_cast<std::uint8_t>(seed)
              && static_cast<std::uint8_t>(bytes[15]) == static_cast<std::uint8_t>(seed >> 24),
              "seed must use portable little-endian storage");
        std::istringstream stream(bytes, std::ios::binary);
        auto restored = voxel::decodeWorld(stream);
        check(restored.seed() == seed, "snapshot must retain terrain seed");
        check(encode(restored) == bytes, "all materials and negative-coordinate edits must round trip exactly");
        for (std::size_t value = 0; value < voxel::BlockCount; ++value) {
            const auto block = static_cast<voxel::Block>(value);
            check(restored.get({-32 + static_cast<int>(value), 3, -32}) == block
                  && restored.get({32 + static_cast<int>(value), 13, 47}) == block,
                  "each material ID including bedrock round trips at opposite world edges");
        }
        check(restored.takeDirtyChunks().size() == voxel::World::ChunkCount,
              "loaded worlds must request all mesh uploads");
        check(restored.takeDirtyChunks().empty(), "loaded dirty flags must be consumable");
    }

    // Build the original format independently of today's encoder or terrain.
    // Old saves contain only Air=0 through Sand=4 and must retain their meaning.
    constexpr std::size_t legacyPayloadSize = 25 * 16 * 16 * 16;
    std::string legacy("VXWORLD\0", 8);
    legacy.resize(36 + legacyPayloadSize + 4);
    writeInteger(legacy, 8, 1);
    writeInteger(legacy, 12, 2026);
    writeInteger(legacy, 16, 16);
    writeInteger(legacy, 20, 5);
    writeInteger(legacy, 24, 1);
    writeInteger(legacy, 28, 5);
    writeInteger(legacy, 32, legacyPayloadSize);
    for (std::size_t index = 0; index < legacyPayloadSize; ++index) {
        legacy[36 + index] = static_cast<char>(index % 5);
    }
    repairChecksum(legacy);
    std::istringstream legacyStream(legacy, std::ios::binary);
    const auto legacyWorld = voxel::decodeWorld(legacyStream);
    check(legacyWorld.seed() == 2026 && encode(legacyWorld) == legacy,
          "original version-one saves remain readable and byte-for-byte stable");
    std::size_t legacyCursor = 0;
    for (std::size_t index = 0; index < voxel::World::ChunkCount; ++index) {
        for (int y = 0; y < voxel::Chunk::Size; ++y) {
            for (int z = 0; z < voxel::Chunk::Size; ++z) {
                for (int x = 0; x < voxel::Chunk::Size; ++x) {
                    check(legacyWorld.chunk(index).get(x, y, z)
                              == static_cast<voxel::Block>(legacyCursor++ % 5),
                          "legacy payload order and material IDs retain their meaning");
                }
            }
        }
    }

    voxel::World source(42);
    const auto valid = encode(source);
    for (const std::size_t length : {std::size_t(0), std::size_t(7), std::size_t(8), std::size_t(20),
                                    std::size_t(35), std::size_t(36), valid.size() - 5, valid.size() - 1}) {
        check(rejects(valid.substr(0, length)), "truncated snapshots must fail");
    }
    auto changed = valid;
    changed[0] = 'Q';
    check(rejects(changed), "incorrect magic must fail");
    for (const std::size_t offset : {8u, 16u, 20u, 24u, 28u, 32u}) {
        changed = valid;
        writeInteger(changed, offset, 0xffffffffu);
        repairChecksum(changed);
        check(rejects(changed), "hostile version/dimension/payload lengths must fail before allocation");
    }
    for (const auto value : {static_cast<unsigned int>(voxel::BlockCount), 255u}) {
        changed = valid;
        changed[36] = static_cast<char>(value);
        repairChecksum(changed);
        check(rejects(changed), "unknown materials must fail even with a valid checksum");
    }
    changed = valid;
    changed[36] = changed[36] == 0 ? 1 : 0;
    check(rejects(changed), "changed voxel data must fail checksum verification");
    changed = valid;
    changed[12] ^= 1;
    check(rejects(changed), "changed seed must fail checksum verification");
    changed = valid;
    changed.back() ^= 1;
    check(rejects(changed), "changed checksum must fail");
    check(rejects(valid + '\0'), "trailing bytes must fail");

    const auto before = encode(source);
    try {
        std::istringstream stream(changed, std::ios::binary);
        source = voxel::decodeWorld(stream);
        check(false, "corrupt decode must throw before caller assignment");
    } catch (const std::runtime_error&) {
        check(encode(source) == before, "failed decoding must leave caller's world unchanged");
    }
    try {
        std::ostringstream stream;
        stream.setstate(std::ios::badbit);
        voxel::encodeWorld(source, stream);
        check(false, "failed output streams must throw");
    } catch (const std::runtime_error&) {
    }
    return failures == 0 ? 0 : 1;
}
