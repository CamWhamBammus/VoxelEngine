#include "save.h"
#include "ores.h"

#include <array>
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

struct PreviousFormat {
    std::uint32_t version, width, height, materials;
};
constexpr std::array previousFormats{PreviousFormat{1, 5, 1, 16}, PreviousFormat{2, 9, 4, 20}};

std::uint8_t previousMaterial(std::size_t index, std::uint32_t materials)
{
    // Vary local coordinates and chunk indices so an axis swap cannot pass.
    return static_cast<std::uint8_t>((index * 7 + index / 16 * 3 + index / 256 * 5
                                     + index / 4096 * 11) % materials);
}

std::string previousSnapshot(PreviousFormat format)
{
    const auto payloadSize = format.width * format.width * format.height * 16 * 16 * 16;
    std::string bytes("VXWORLD\0", 8);
    bytes.resize(36 + payloadSize + 4);
    writeInteger(bytes, 8, format.version);
    writeInteger(bytes, 12, 2026);
    writeInteger(bytes, 16, 16);
    writeInteger(bytes, 20, format.width);
    writeInteger(bytes, 24, format.height);
    writeInteger(bytes, 28, format.width);
    writeInteger(bytes, 32, payloadSize);
    for (std::size_t index = 0; index < payloadSize; ++index) {
        bytes[36 + index] = static_cast<char>(previousMaterial(index, format.materials));
    }
    repairChecksum(bytes);
    return bytes;
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
    using voxel::World;
    constexpr auto low = World::Min;
    constexpr auto high = World::MaxExclusive - glm::ivec3(1);
    for (const std::uint32_t seed : {0u, 1337u, 0x89abcdefu}) {
        voxel::World source(seed);
        source.takeDirtyChunks();
        source.set(low, voxel::Block::Air);
        source.set({-1, 15, -1}, voxel::Block::Grass);
        source.set({0, 15, 0}, voxel::Block::Dirt);
        source.set({16, 15, 16}, voxel::Block::Stone);
        source.set(high, voxel::Block::Sand);
        for (std::size_t value = 0; value < voxel::BlockCount; ++value) {
            const auto block = static_cast<voxel::Block>(value);
            source.set({low.x + static_cast<int>(value), 3, low.z}, block);
            source.set({high.x - static_cast<int>(value), high.y, high.z}, block);
        }
        const auto bytes = encode(source);
        check(static_cast<unsigned char>(bytes[8]) == 3 && static_cast<unsigned char>(bytes[20]) == 15
              && static_cast<unsigned char>(bytes[24]) == 6 && static_cast<unsigned char>(bytes[28]) == 15,
              "new snapshots write version three with a 15x6x15 chunk grid");
        check(bytes.size() == 36 + voxel::World::ChunkCount * 16 * 16 * 16 + 4, "snapshot must use compact fixed byte layout");
        check(static_cast<std::uint8_t>(bytes[12]) == static_cast<std::uint8_t>(seed)
              && static_cast<std::uint8_t>(bytes[15]) == static_cast<std::uint8_t>(seed >> 24),
              "seed must use portable little-endian storage");
        std::istringstream stream(bytes, std::ios::binary);
        auto restored = voxel::decodeWorld(stream);
        check(restored.seed() == seed, "snapshot must retain terrain seed");
        check(encode(restored) == bytes, "all materials and negative-coordinate edits must round trip exactly");
        for (std::size_t value = 0; value < voxel::BlockCount; ++value) {
            const auto block = static_cast<voxel::Block>(value);
            check(restored.get({low.x + static_cast<int>(value), 3, low.z}) == block
                  && restored.get({high.x - static_cast<int>(value), high.y, high.z}) == block,
                  "each material ID including bedrock round trips at opposite world edges");
            if (const auto* ore = voxel::oreInfo(block)) {
                const auto* recovered = voxel::oreInfo(restored.get({low.x + static_cast<int>(value), 3, low.z}));
                check(recovered && recovered->item == ore->item && recovered->requiredTier == ore->requiredTier,
                      "saved ore blocks retain their resource drops and mining tier");
            }
        }
        check(restored.takeDirtyChunks().size() == voxel::World::ChunkCount,
              "loaded worlds must request all mesh uploads");
        check(restored.takeDirtyChunks().empty(), "loaded dirty flags must be consumable");
    }

    // Build both previous formats independently of today's encoder or terrain.
    for (const auto format : previousFormats) {
        const auto legacy = previousSnapshot(format);
        std::istringstream legacyStream(legacy, std::ios::binary);
        const auto legacyWorld = voxel::decodeWorld(legacyStream);
        check(legacyWorld.seed() == 2026, "previous snapshots retain the world seed");
        const voxel::World legacyTerrain(2026);
        const auto migratedBytes = encode(legacyWorld);
        check(static_cast<unsigned char>(migratedBytes[8]) == 3,
              "version-one and version-two snapshots upgrade to version three");
        std::istringstream migratedStream(migratedBytes, std::ios::binary);
        check(voxel::decodeWorld(migratedStream).sameContents(legacyWorld),
              "saving and loading an imported world preserves every migrated voxel");
        std::size_t cursor = 0;
        const int radius = static_cast<int>(format.width / 2);
        const auto chunkCount = format.width * format.width * format.height;
        for (std::size_t index = 0; index < chunkCount; ++index) {
            const glm::ivec3 origin((static_cast<int>(index % format.width) - radius) * 16,
                static_cast<int>(index / (format.width * format.width)) * 16,
                (static_cast<int>(index / format.width % format.width) - radius) * 16);
            for (int y = 0; y < voxel::Chunk::Size; ++y) {
                for (int z = 0; z < voxel::Chunk::Size; ++z) {
                    for (int x = 0; x < voxel::Chunk::Size; ++x) {
                        check(legacyWorld.get(origin + glm::ivec3(x, y, z))
                              == static_cast<voxel::Block>(previousMaterial(cursor++, format.materials)),
                              "previous payloads retain all material IDs at their exact world coordinates");
                    }
                }
            }
        }
        for (std::size_t index = 0; index < voxel::World::ChunkCount; ++index) {
            const auto origin = legacyWorld.origin(index);
            if (origin.y < static_cast<int>(format.height) * 16
                && origin.x >= -radius * 16 && origin.x <= radius * 16
                && origin.z >= -radius * 16 && origin.z <= radius * 16) continue;
            check(legacyWorld.chunk(index) == legacyTerrain.chunk(index),
                  "space outside each previous snapshot retains deterministic seed terrain");
        }
        for (const auto& malformed : {legacy.substr(0, legacy.size() - 1), legacy + '\0'}) {
            check(rejects(malformed), "previous imports reject truncation and trailing data");
        }
        for (const auto claimedVersion : {1u, 2u, 3u}) {
            if (claimedVersion == format.version) continue;
            auto changed = legacy;
            writeInteger(changed, 8, claimedVersion);
            repairChecksum(changed);
            check(rejects(changed), "a previous format cannot claim another version's dimensions");
        }
        auto changed = legacy;
        changed[36] = static_cast<char>(voxel::BlockCount);
        repairChecksum(changed);
        check(rejects(changed), "previous formats reject unknown materials with a valid checksum");
        changed = legacy;
        changed[36] ^= 1;
        check(rejects(changed), "previous formats still enforce voxel checksums");
    }

    voxel::World source(42);
    const auto valid = encode(source);
    for (const std::size_t length : {std::size_t(0), std::size_t(7), std::size_t(8), std::size_t(20),
                                    std::size_t(35), std::size_t(36), valid.size() - 5, valid.size() - 1}) {
        check(rejects(valid.substr(0, length)), "truncated snapshots must fail");
    }
    for (const auto previousVersion : {1u, 2u}) {
        auto changed = valid;
        writeInteger(changed, 8, previousVersion);
        repairChecksum(changed);
        check(rejects(changed), "previous versions cannot claim version-three dimensions");
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
    for (const auto version : {1u, 2u, 3u}) {
        auto header = version == 3 ? valid.substr(0, 36)
            : previousSnapshot(previousFormats[version - 1]).substr(0, 36);
        for (const auto offset : {16u, 20u, 24u, 28u, 32u}) {
            auto hostile = header;
            writeInteger(hostile, offset, 0xffffffffu);
            std::istringstream stream(hostile, std::ios::binary);
            try {
                static_cast<void>(voxel::decodeWorld(stream));
                check(false, "hostile dimensions must be rejected");
            } catch (const std::runtime_error&) {
                check(stream.tellg() == std::streampos(36),
                      "invalid format sizes are rejected before any payload read or allocation");
            }
        }
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
