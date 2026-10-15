#include "voxel/world_codec.h"

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
        const auto bytes = encode(source);
        check(bytes.size() == 36 + 25 * 16 * 16 * 16 + 4, "snapshot must use compact fixed byte layout");
        check(static_cast<std::uint8_t>(bytes[12]) == static_cast<std::uint8_t>(seed)
              && static_cast<std::uint8_t>(bytes[15]) == static_cast<std::uint8_t>(seed >> 24),
              "seed must use portable little-endian storage");
        std::istringstream stream(bytes, std::ios::binary);
        auto restored = voxel::decodeWorld(stream);
        check(restored.seed() == seed, "snapshot must retain terrain seed");
        check(encode(restored) == bytes, "all materials and negative-coordinate edits must round trip exactly");
        check(restored.takeDirtyChunks().size() == voxel::World::ChunkCount,
              "loaded worlds must request all mesh uploads");
        check(restored.takeDirtyChunks().empty(), "loaded dirty flags must be consumable");
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
    changed = valid;
    changed[36] = static_cast<char>(255);
    repairChecksum(changed);
    check(rejects(changed), "unknown materials must fail even with a valid checksum");
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
