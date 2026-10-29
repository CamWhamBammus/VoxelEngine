#include "sections.h"
#include "asset_collision.h"
#include "player.h"
#include "editing.h"
#include "save.h"
#include "spawner_chains.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace voxel {
namespace {

constexpr std::array Worlds{
    SectionWorldInfo{SectionWorld::LushCaves, "Lush Caves", "Roots, gardens, trees and water", "lush-caves"},
    SectionWorldInfo{SectionWorld::CrystalHollows, "Crystal Hollows", "Pale stone, slate and crystal seams", "crystal-hollows"},
    SectionWorldInfo{SectionWorld::EmberDepths, "Ember Depths", "Slate, obsidian and warm clay bands", "ember-depths"},
};
constexpr std::array Categories{
    SectionCategoryInfo{SectionCategory::Parkour, "Parkour", "Jumps, climbs and careful crossings", "parkour"},
    SectionCategoryInfo{SectionCategory::Puzzle, "Puzzle", "Spaces for future environmental puzzles", "puzzle"},
    SectionCategoryInfo{SectionCategory::Exploration, "Exploration", "Branching paths and hidden discoveries", "exploration"},
    SectionCategoryInfo{SectionCategory::Mining, "Mining", "Ore chambers and future gathering routes", "mining"},
    SectionCategoryInfo{SectionCategory::Treasure, "Treasure", "Final chambers and reward locations", "treasure"},
    SectionCategoryInfo{SectionCategory::Arena, "Arena", "Linked monster waves, cover and combat", "arena"},
};
constexpr std::array Assets{
    SectionAssetInfo{HubAssetKind::LanternCap, "Lantern", "Lights"},
    SectionAssetInfo{HubAssetKind::WallBracket, "Wall bracket", "Lights"},
    SectionAssetInfo{HubAssetKind::HangingLamp, "Hanging lamp", "Lights"},
    SectionAssetInfo{HubAssetKind::CrystalBeacon, "Crystal beacon", "Lights"},
    SectionAssetInfo{HubAssetKind::WindowBox, "Window box", "Buildings"},
    SectionAssetInfo{HubAssetKind::Shutters, "Shutters", "Buildings"},
    SectionAssetInfo{HubAssetKind::Awning, "Awning", "Buildings"},
    SectionAssetInfo{HubAssetKind::RoofBrace, "Roof brace", "Buildings"},
    SectionAssetInfo{HubAssetKind::Banner, "Banner", "Buildings"},
    SectionAssetInfo{HubAssetKind::CrateStack, "Crate stack", "Buildings"},
    SectionAssetInfo{HubAssetKind::Fern, "Fern", "Greenery"},
    SectionAssetInfo{HubAssetKind::Flowers, "Flowers", "Greenery"},
    SectionAssetInfo{HubAssetKind::Mushrooms, "Mushrooms", "Greenery"},
    SectionAssetInfo{HubAssetKind::Reeds, "Reeds", "Greenery"},
    SectionAssetInfo{HubAssetKind::LilyPads, "Lily pads", "Greenery"},
    SectionAssetInfo{HubAssetKind::Vines, "Vines", "Greenery"},
    SectionAssetInfo{HubAssetKind::Shrub, "Shrub", "Greenery"},
    SectionAssetInfo{HubAssetKind::PathPaving, "Fitted paving", "Paths"},
    SectionAssetInfo{HubAssetKind::PathEdging, "Path edging", "Paths"},
    SectionAssetInfo{HubAssetKind::PathStep, "Path step", "Paths"},
    SectionAssetInfo{HubAssetKind::Boulder, "Boulder", "Cave"},
    SectionAssetInfo{HubAssetKind::RockLedge, "Rock ledge", "Cave"},
    SectionAssetInfo{HubAssetKind::StalactiteCluster, "Stalactites", "Cave"},
    SectionAssetInfo{HubAssetKind::RootArch, "Root arch", "Roots"},
    SectionAssetInfo{HubAssetKind::RootCurtain, "Root curtain", "Roots"},
    SectionAssetInfo{HubAssetKind::BroadleafClump, "Broadleaf clump", "Greenery"},
    SectionAssetInfo{HubAssetKind::FernBed, "Fern bed", "Greenery"},
    SectionAssetInfo{HubAssetKind::MossGarden, "Moss garden", "Greenery"},
    SectionAssetInfo{HubAssetKind::Dormer, "Dormer", "Buildings"},
    SectionAssetInfo{HubAssetKind::BayWindow, "Bay window", "Buildings"},
    SectionAssetInfo{HubAssetKind::Chimney, "Chimney", "Buildings"},
    SectionAssetInfo{HubAssetKind::Balcony, "Balcony", "Buildings"},
    SectionAssetInfo{HubAssetKind::PorchCanopy, "Porch canopy", "Buildings"},
    SectionAssetInfo{HubAssetKind::Forge, "Forge", "Workshop"},
    SectionAssetInfo{HubAssetKind::Anvil, "Anvil", "Workshop"},
    SectionAssetInfo{HubAssetKind::ToolRack, "Tool rack", "Workshop"},
    SectionAssetInfo{HubAssetKind::SmithWorkbench, "Workbench", "Workshop"},
    SectionAssetInfo{HubAssetKind::AnvilSign, "Anvil sign", "Workshop"},
    SectionAssetInfo{HubAssetKind::TreeFern, "Tree fern", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::LushSapling, "Lush sapling", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::HangingRoots, "Hanging roots", "Lush / ceiling"},
    SectionAssetInfo{HubAssetKind::BlossomVines, "Blossom vines", "Lush / wall"},
    SectionAssetInfo{HubAssetKind::GlowMushroomColony, "Glow mushrooms", "Lush / ground / light"},
    SectionAssetInfo{HubAssetKind::GiantMushroom, "Giant mushroom", "Lush / ground / light"},
    SectionAssetInfo{HubAssetKind::ShelfFungi, "Shelf fungi", "Lush / wall"},
    SectionAssetInfo{HubAssetKind::CaveLotus, "Cave lotus", "Lush / water surface"},
    SectionAssetInfo{HubAssetKind::WaterGrass, "Water grass", "Lush / water surface"},
    SectionAssetInfo{HubAssetKind::MossyLog, "Mossy log", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::RootFan, "Root fan", "Lush / wall"},
    SectionAssetInfo{HubAssetKind::MossyStones, "Mossy stones", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::CrystalFlower, "Crystal flower", "Lush / ground / light"},
    SectionAssetInfo{HubAssetKind::SporeStalks, "Spore stalks", "Lush / ground / light"},
    SectionAssetInfo{HubAssetKind::CaveOrchid, "Cave orchid", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::HangingPlanter, "Hanging planter", "Lush / ceiling"},
    SectionAssetInfo{HubAssetKind::LootChest, "Wooden chest", "Chests / authored contents"},
    SectionAssetInfo{HubAssetKind::CrystalChest, "Crystal chest", "Chests / authored contents"},
    SectionAssetInfo{HubAssetKind::IronboundChest, "Ironbound chest", "Chests / authored contents"},
    SectionAssetInfo{HubAssetKind::KeyPickup, "Cave key", "Loot / walk over to collect"},
    SectionAssetInfo{HubAssetKind::ItemPickup, "Item pickup", "Items / walk over to collect"},
    SectionAssetInfo{HubAssetKind::TimberFence, "Timber fence", "Buildings / 1 block fence / ground"},
    SectionAssetInfo{HubAssetKind::PicketFence, "Picket fence", "Buildings / 1 block fence / ground"},
    SectionAssetInfo{HubAssetKind::FenceCorner, "Fence corner", "Buildings / 1 block corner / ground"},
    SectionAssetInfo{HubAssetKind::GardenGate, "Garden gate", "Buildings / 1 block gate / Y open or closed"},
    SectionAssetInfo{HubAssetKind::IronRailing, "Iron railing", "Buildings / 1 block railing / ground"},
    SectionAssetInfo{HubAssetKind::TimberPillar, "Timber pillar", "Buildings / 1 block pillar / stack to extend"},
    SectionAssetInfo{HubAssetKind::TimberBeam, "Timber beam", "Buildings / 1 block beam / join end to end"},
    SectionAssetInfo{HubAssetKind::DiagonalBrace, "Diagonal brace", "Buildings / 1 block brace / wall"},
    SectionAssetInfo{HubAssetKind::StoneDoorway, "Stone doorway", "Buildings / 3 x 3 frame / 1 x 2 clear door"},
    SectionAssetInfo{HubAssetKind::WindowFrame, "Window frame", "Buildings / 1 x 1 window / wall"},
    SectionAssetInfo{HubAssetKind::WoodenStairs, "Wooden stairs", "Buildings / six steps / ground"},
    SectionAssetInfo{HubAssetKind::Barrel, "Barrel", "Buildings / 1 block barrel / ground"},
    SectionAssetInfo{HubAssetKind::TrialSpawner, "Trial spawner", "Encounters / E to set enemies and rewards"},
    SectionAssetInfo{HubAssetKind::JungleMonstera, "Jungle monstera", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::JungleCycad, "Jungle cycad", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::BambooClump, "Bamboo clump", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::FiddleheadFerns, "Fiddlehead ferns", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::HangingFern, "Hanging fern", "Lush / ceiling"},
    SectionAssetInfo{HubAssetKind::MossDrape, "Moss drape", "Lush / wall"},
    SectionAssetInfo{HubAssetKind::PitcherPlants, "Pitcher plants", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::SeedPodCluster, "Seed pods", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::RootBridge, "Root bridge", "Lush / crossings"},
    SectionAssetInfo{HubAssetKind::RootStump, "Root stump", "Lush / ground"},
    SectionAssetInfo{HubAssetKind::MossyPlatform, "Mossy platform", "Lush / parkour"},
    SectionAssetInfo{HubAssetKind::MossyColumn, "Mossy column", "Lush / parkour"},
    SectionAssetInfo{HubAssetKind::OvergrownArch, "Overgrown arch", "Lush / ruins"},
    SectionAssetInfo{HubAssetKind::RuinedPillar, "Ruined pillar", "Lush / ruins"},
    SectionAssetInfo{HubAssetKind::RuinedWall, "Ruined wall", "Lush / ruins"},
    SectionAssetInfo{HubAssetKind::RuinSlab, "Ruin slab", "Lush / ruins"},
    SectionAssetInfo{HubAssetKind::ArmorStand, "Armor stand", "Armory"},
    SectionAssetInfo{HubAssetKind::ShieldSign, "Shield sign", "Armory"},
    SectionAssetInfo{HubAssetKind::ArmorerBench, "Armorer bench", "Armory"},
};
static_assert(Assets.size() == HubAssetCount);
constexpr std::array<char, 8> Magic{'V', 'X', 'S', 'E', 'C', 'T', '\0', '\0'};
constexpr std::uint32_t Version = 6;
constexpr std::size_t MaxNameLength = 64;
constexpr std::size_t WorldBytes = World::ChunkCount * Chunk::Size * Chunk::Size * Chunk::Size + 40;
constexpr std::size_t MaxSectionBytes = WorldBytes + SectionMaxAssets * (56 + ChestSlotCount * 8) + 256;

bool validWorld(SectionWorld id)
{
    return static_cast<unsigned>(id) < Worlds.size();
}
bool validCategory(SectionCategory id)
{
    return static_cast<unsigned>(id) < Categories.size();
}
bool finite(glm::vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
bool inside(glm::vec3 point)
{
    return finite(point) && point.x >= SectionInteriorMin.x && point.x < SectionInteriorMax.x
        && point.y >= SectionInteriorMin.y && point.y < SectionInteriorMax.y
        && point.z >= SectionInteriorMin.z && point.z < SectionInteriorMax.z;
}
void validate(const SectionMetadata& meta, std::span<const HubAsset> assets)
{
    if (!validWorld(meta.world) || !validCategory(meta.category)) {
        throw std::runtime_error("Unknown section world or category");
    }
    if (meta.lighting != SectionLighting::Ambient && meta.lighting != SectionLighting::Dark)
        throw std::runtime_error("Unknown section lighting");
    if (meta.name.empty() || meta.name.size() > MaxNameLength
        || std::any_of(meta.name.begin(), meta.name.end(), [](unsigned char c) { return c < 32 || c == 127; })) {
        throw std::runtime_error("Section names must contain 1 to 64 printable characters");
    }
    if (!inside(meta.spawn) || !inside(meta.finish) || (meta.checkpoint && !inside(*meta.checkpoint))) {
        throw std::runtime_error("Section markers must be finite and inside the building room");
    }
    if (assets.size() > SectionMaxAssets) {
        throw std::runtime_error("Section has too many assets");
    }
    for (const auto& asset : assets) {
        if (static_cast<unsigned>(asset.kind) >= Assets.size() || asset.facing < 0 || asset.facing > 3
            || !inside(asset.position)) {
            throw std::runtime_error("Invalid section asset kind, rotation or position");
        }
        if (!validChestItems(asset))
            throw std::runtime_error("Invalid chest or pickup contents: use valid items in stacks of 1 to 99");
        if (isSpawnerAsset(asset.kind) ? !validSpawnerSettings(asset.spawner) : asset.spawner != SpawnerSettings{})
            throw std::runtime_error("Spawners need 1 or more enemies, with 0 to 20 of each kind");
    }
    if (const auto error = validateSpawnerChains(assets); !error.empty())
        throw std::runtime_error(error);
}

std::uint32_t checksum(std::string_view data)
{
    std::uint32_t result = 2166136261u;
    for (unsigned char byte : data) result = (result ^ byte) * 16777619u;
    return result;
}
void append(std::string& bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<char>(value >> shift));
}
void append(std::string& bytes, glm::vec3 value)
{
    append(bytes, std::bit_cast<std::uint32_t>(value.x));
    append(bytes, std::bit_cast<std::uint32_t>(value.y));
    append(bytes, std::bit_cast<std::uint32_t>(value.z));
}
struct Reader {
    std::string_view bytes;
    std::size_t cursor = 0;

    std::string_view take(std::size_t count)
    {
        if (count > bytes.size() - cursor) throw std::runtime_error("Truncated section document");
        const auto result = bytes.substr(cursor, count);
        cursor += count;
        return result;
    }
    std::uint32_t integer()
    {
        const auto data = take(4);
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << (i * 8);
        return value;
    }
    glm::vec3 point()
    {
        // Keep reads sequenced; argument evaluation order is not specified.
        const float x = std::bit_cast<float>(integer());
        const float y = std::bit_cast<float>(integer());
        const float z = std::bit_cast<float>(integer());
        return {x, y, z};
    }
};

std::string encode(const World& world, const SectionMetadata& meta, std::span<const HubAsset> assets)
{
    validate(meta, assets);
    if (meta.tested) {
        AssetCollision collision;
        collision.rebuild(assets);
        if (!sectionMarkerSafe(world, meta.spawn, &collision) || !sectionMarkerSafe(world, meta.finish, &collision)
            || (meta.checkpoint && !sectionMarkerSafe(world, *meta.checkpoint, &collision)))
            throw std::runtime_error("Start, checkpoint and finish need solid footing and clear headroom");
    }
    std::string bytes(Magic.data(), Magic.size());
    append(bytes, Version);
    append(bytes, static_cast<std::uint32_t>(meta.world));
    append(bytes, static_cast<std::uint32_t>(meta.category));
    append(bytes, meta.tested ? 1u : 0u);
    append(bytes, static_cast<std::uint32_t>(meta.name.size()));
    bytes += meta.name;
    append(bytes, meta.spawn);
    append(bytes, meta.finish);
    append(bytes, meta.checkpoint ? 1u : 0u);
    if (meta.checkpoint) append(bytes, *meta.checkpoint);
    append(bytes, static_cast<std::uint32_t>(assets.size()));
    for (const auto& asset : assets) {
        append(bytes, static_cast<std::uint32_t>(asset.kind));
        append(bytes, asset.position);
        append(bytes, static_cast<std::uint32_t>(asset.facing));
        append(bytes, asset.variation);
        append(bytes, static_cast<std::uint32_t>(asset.chestItems.size()));
        for (const auto& stack : asset.chestItems) {
            append(bytes, static_cast<std::uint32_t>(stack.item));
            append(bytes, static_cast<std::uint32_t>(stack.count));
        }
        append(bytes, static_cast<std::uint32_t>(asset.pickupItem.item));
        append(bytes, static_cast<std::uint32_t>(asset.pickupItem.count));
        if (isSpawnerAsset(asset.kind)) {
            for (const int count : asset.spawner.counts) append(bytes, static_cast<std::uint32_t>(count));
            append(bytes, asset.spawner.id);
            append(bytes, asset.spawner.next);
        }
    }
    std::ostringstream snapshot(std::ios::binary);
    encodeWorld(world, snapshot);
    const auto worldData = snapshot.str();
    append(bytes, static_cast<std::uint32_t>(worldData.size()));
    bytes += worldData;
    append(bytes, static_cast<std::uint32_t>(meta.lighting));
    append(bytes, checksum(std::string_view(bytes).substr(Magic.size())));
    return bytes;
}

struct ParsedSection {
    SectionMetadata meta;
    std::vector<HubAsset> assets;
    std::string worldData;
};
ParsedSection read(const std::filesystem::path& path)
{
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Section is not a regular file: " + path.string());
    const auto size = std::filesystem::file_size(path);
    if (size < Magic.size() + 8 || size > MaxSectionBytes) throw std::runtime_error("Invalid section document size");
    std::ifstream input(path, std::ios::binary);
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) || input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Cannot read section document");
    }
    Reader reader{bytes};
    if (reader.take(Magic.size()) != std::string_view(Magic.data(), Magic.size())) throw std::runtime_error("Invalid section signature");
    Reader trailer{std::string_view(bytes).substr(bytes.size() - 4)};
    if (trailer.integer() != checksum(std::string_view(bytes).substr(Magic.size(), bytes.size() - Magic.size() - 4))) {
        throw std::runtime_error("Section checksum mismatch");
    }
    const auto version = reader.integer();
    if (version < 1 || version > Version) throw std::runtime_error("Unsupported section document version");
    ParsedSection result;
    result.meta.world = static_cast<SectionWorld>(reader.integer());
    result.meta.category = static_cast<SectionCategory>(reader.integer());
    const auto tested = reader.integer();
    if (tested > 1) throw std::runtime_error("Invalid tested flag in section");
    result.meta.tested = tested != 0;
    const auto nameSize = reader.integer();
    if (nameSize > MaxNameLength) throw std::runtime_error("Section name is too long");
    result.meta.name = reader.take(nameSize);
    result.meta.spawn = reader.point();
    result.meta.finish = reader.point();
    const auto hasCheckpoint = reader.integer();
    if (hasCheckpoint > 1) throw std::runtime_error("Invalid checkpoint flag in section");
    if (hasCheckpoint) result.meta.checkpoint = reader.point();
    const auto assetCount = reader.integer();
    if (assetCount > SectionMaxAssets) throw std::runtime_error("Section asset count exceeds limit");
    result.assets.reserve(assetCount);
    for (std::uint32_t i = 0; i < assetCount; ++i) {
        HubAsset asset;
        const auto kind = reader.integer();
        const auto kindCount = version == 1 ? static_cast<std::size_t>(HubAssetKind::LootChest) + 1
                             : version == 2 ? static_cast<std::size_t>(HubAssetKind::KeyPickup) + 1
                             : version == 3 ? static_cast<std::size_t>(HubAssetKind::Barrel) + 1
                                            : HubAssetCount;
        if (kind >= kindCount)
            throw std::runtime_error("Invalid section asset kind");
        asset.kind = static_cast<HubAssetKind>(kind);
        asset.position = reader.point();
        const auto facing = reader.integer();
        if (facing > 3) throw std::runtime_error("Invalid section asset rotation");
        asset.facing = static_cast<int>(facing);
        asset.variation = reader.integer();
        if (version >= 2) {
            const auto count = reader.integer();
            if (count > ChestSlotCount || (count != 0 && !isChestAsset(asset.kind) && !isSpawnerAsset(asset.kind)))
                throw std::runtime_error("Invalid chest contents size");
            asset.chestItems.reserve(count);
            for (std::uint32_t stack = 0; stack < count; ++stack) {
                const auto item = reader.integer();
                const auto amount = reader.integer();
                if (item >= ItemCount || amount == 0 || amount > ChestStackLimit)
                    throw std::runtime_error("Invalid chest item or quantity");
                asset.chestItems.push_back({static_cast<ItemId>(item), static_cast<int>(amount)});
            }
        } else if (asset.kind == HubAssetKind::LootChest) {
            // Materialize old randomized presets once, with a stable seed, so
            // opening a legacy room never silently removes its authored rewards.
            std::uint32_t seed = 0;
            for (int axis = 0; axis < 3; ++axis)
                seed = (seed ^ std::bit_cast<std::uint32_t>(asset.position[axis])) * 16777619u;
            asset.chestItems = legacyChestLoot(asset.variation, seed);
            if (asset.variation % 3 == 1) asset.kind = HubAssetKind::CrystalChest;
            else if (asset.variation % 3 == 2) asset.kind = HubAssetKind::IronboundChest;
        }
        if (version >= 3) {
            const auto item = reader.integer();
            const auto amount = reader.integer();
            if (item >= ItemCount || amount > ChestStackLimit)
                throw std::runtime_error("Invalid pickup item or quantity");
            asset.pickupItem = {static_cast<ItemId>(item), static_cast<int>(amount)};
        }
        if (version >= 4 && isSpawnerAsset(asset.kind)) {
            for (int& count : asset.spawner.counts) {
                const auto value = reader.integer();
                if (value > SpawnerMobLimit) throw std::runtime_error("Spawner enemy count exceeds 20");
                count = static_cast<int>(value);
            }
            if (version >= 6) {
                asset.spawner.id = reader.integer();
                asset.spawner.next = reader.integer();
            }
        }
        result.assets.push_back(std::move(asset));
    }
    const auto worldSize = reader.integer();
    if (worldSize != WorldBytes) throw std::runtime_error("Section voxel snapshot has invalid dimensions");
    result.worldData = reader.take(worldSize);
    if (version >= 5) result.meta.lighting = static_cast<SectionLighting>(reader.integer());
    validate(result.meta, result.assets);
    if (reader.cursor != bytes.size() - 4) throw std::runtime_error("Unexpected trailing data in section");
    return result;
}

void atomicWrite(std::string_view bytes, const std::filesystem::path& path, bool overwrite)
{
    if (!path.has_filename() || path.filename() == "." || path.filename() == "..") throw std::runtime_error("Section save requires a filename");
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    if (!std::filesystem::is_directory(parent)) throw std::runtime_error("Section save directory does not exist");
    const auto pattern = (parent / ".section-save-XXXXXX").string();
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back('\0');
    int descriptor = ::mkstemp(temporary.data());
    if (descriptor < 0) throw std::system_error(errno, std::generic_category(), "Cannot create temporary section");
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
            if (written < 0) {
                if (errno == EINTR) continue;
                throw std::system_error(errno, std::generic_category(), "Cannot write section");
            }
            if (written == 0) throw std::runtime_error("Section write made no progress");
            offset += static_cast<std::size_t>(written);
        }
        if (::fsync(descriptor) != 0) throw std::system_error(errno, std::generic_category(), "Cannot flush section");
        const int closeResult = ::close(descriptor);
        descriptor = -1;
        if (closeResult != 0) throw std::system_error(errno, std::generic_category(), "Cannot close section");
        if (overwrite) {
            std::filesystem::rename(temporary.data(), path);
        } else {
            // A hard link claims an unused destination atomically. Unlike an
            // existence check followed by rename, concurrent accepts cannot
            // replace another section that claimed the same filename.
            if (::link(temporary.data(), path.c_str()) != 0) throw std::system_error(errno, std::generic_category(), "Cannot create section destination");
            ::unlink(temporary.data());
        }
    } catch (...) {
        if (descriptor >= 0) ::close(descriptor);
        ::unlink(temporary.data());
        throw;
    }
}

// Directory names come only from the catalogs, never from editable metadata.
// Refuse symlinked descendants so a library cannot redirect a save elsewhere.
std::filesystem::path directory(const std::filesystem::path& root, std::span<const std::string_view> components)
{
    if (root.empty()) throw std::runtime_error("Section library needs a directory");
    std::filesystem::create_directories(root);
    auto current = root;
    for (const auto part : components) {
        current /= part;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(current))) throw std::runtime_error("Section library folders cannot be symbolic links");
        std::filesystem::create_directory(current);
        if (!std::filesystem::is_directory(current)) throw std::runtime_error("Section library path is not a directory");
    }
    return current;
}
std::string slug(std::string_view name)
{
    std::string result;
    for (unsigned char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) result += static_cast<char>(c);
        else if (c >= 'A' && c <= 'Z') result += static_cast<char>(c - 'A' + 'a');
        else if (!result.empty() && result.back() != '-') result += '-';
    }
    if (!result.empty() && result.back() == '-') result.pop_back();
    return result.empty() ? "section" : result;
}
std::filesystem::path numberedSave(const World& world, const SectionMetadata& meta,
                                  std::span<const HubAsset> assets, const std::filesystem::path& folder)
{
    const auto bytes = encode(world, meta, assets);
    for (unsigned index = 1; index <= 99999; ++index) {
        std::ostringstream filename;
        filename << slug(meta.name) << '-' << std::setw(3) << std::setfill('0') << index << ".vxs";
        const auto path = folder / filename.str();
        if (std::filesystem::exists(std::filesystem::symlink_status(path))) continue;
        try {
            atomicWrite(bytes, path, false);
            return path;
        } catch (const std::system_error& error) {
            if (error.code() != std::errc::file_exists) throw;
        }
    }
    throw std::runtime_error("Section library has too many entries with this name");
}
void collect(std::vector<SectionEntry>& result, const std::filesystem::path& folder,
             SectionWorld expectedWorld, std::optional<SectionCategory> category)
{
    if (!std::filesystem::is_directory(folder) || std::filesystem::is_symlink(folder)) return;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() != ".vxs" || entry.is_symlink() || !entry.is_regular_file()) continue;
        try {
            const auto parsed = read(entry.path());
            if (parsed.meta.world == expectedWorld && (!category || parsed.meta.category == *category)
                && (!category || parsed.meta.tested)) {
                result.push_back({entry.path(), parsed.meta});
            }
        } catch (const std::exception&) {
            // The editor can still open all healthy documents in the library.
        }
    }
}

} // namespace

std::span<const SectionWorldInfo> sectionWorldCatalog() { return Worlds; }
std::span<const SectionCategoryInfo> sectionCategoryCatalog() { return Categories; }
std::span<const SectionAssetInfo> sectionAssetCatalog() { return Assets; }

void buildBlankSection(World& world)
{
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        const auto origin = world.origin(index);
        for (int y = 0; y < Chunk::Size; ++y) {
            for (int z = 0; z < Chunk::Size; ++z) {
                for (int x = 0; x < Chunk::Size; ++x) {
                    const auto cell = origin + glm::ivec3(x, y, z);
                    const bool room = cell.x >= -33 && cell.x <= 32 && cell.z >= -33 && cell.z <= 32
                        && cell.y >= 8 && cell.y <= 57;
                    world.set(cell, room && !sectionInterior(cell) ? Block::Stone : Block::Air);
                }
            }
        }
    }
    // Lamps are inset into protected walls, leaving the full build floor clear.
    for (int along = -24; along <= 24; along += 12) {
        for (const int y : {12, 28, 44}) {
            world.set({-33, y, along}, Block::Lantern);
            world.set({32, y, along}, Block::Lantern);
            world.set({along, y, -33}, Block::Lantern);
            world.set({along, y, 32}, Block::Lantern);
        }
    }
}

void buildCaveSection(World& world)
{
    for (std::size_t index = 0; index < World::ChunkCount; ++index) {
        const auto origin = world.origin(index);
        for (int y = 0; y < Chunk::Size; ++y)
            for (int z = 0; z < Chunk::Size; ++z)
                for (int x = 0; x < Chunk::Size; ++x) {
                    const auto cell = origin + glm::ivec3(x, y, z);
                    const bool room = cell.x >= SectionInteriorMin.x - 1 && cell.x <= SectionInteriorMax.x
                        && cell.y >= SectionInteriorMin.y - 1 && cell.y <= SectionInteriorMax.y
                        && cell.z >= SectionInteriorMin.z - 1 && cell.z <= SectionInteriorMax.z;
                    const bool chamber = cell.x >= -3 && cell.x < 4 && cell.y >= 25 && cell.y < 30
                        && cell.z >= 17 && cell.z < 28;
                    world.set(cell, room && !chamber ? Block::Stone : Block::Air);
                }
    }
}

bool sectionMarkerSafe(const World& world, glm::vec3 feet, const AssetCollision* assets)
{
    constexpr float half = Player::Width * 0.5f;
    if (!inside(feet) || feet.x - half < SectionInteriorMin.x || feet.x + half > SectionInteriorMax.x
        || feet.z - half < SectionInteriorMin.z || feet.z + half > SectionInteriorMax.z
        || feet.y + Player::Height > SectionInteriorMax.y) return false;
    Player player;
    // This alias is used only by the local player while the caller owns the cache.
    if (assets) player.setAssetCollision(std::shared_ptr<const AssetCollision>(assets, [](const AssetCollision*) {}));
    if (!player.setFeetPosition(world, feet) || !player.grounded()) return false;
    // A sloped model supports the upright body at its highest contact. Nearby
    // footprint samples may sit below that height; require a surface under all
    // four corners, without forcing fractional model tops onto the voxel grid.
    const bool stair = isStair(world.get(glm::ivec3(glm::floor(feet - glm::vec3(0,.01f,0)))));
    const float depth = (assets && !assets->empty()) || stair ? Player::Width + .02f : .02f;
    for (float dx : {-half + 0.01f, half - 0.01f}) {
        for (float dz : {-half + 0.01f, half - 0.01f}) {
            const glm::vec3 point = feet + glm::vec3(dx, .01f, dz);
            const auto terrain = raycast(point, {0,-1,0}, depth + .01f, [&](glm::ivec3 cell) {
                const auto block = world.get(cell);
                return isSolid(block) ? block : Block::Air;
            });
            bool supported = terrain && terrain->normal.y > 0;
            if (!supported && assets) {
                const auto surface = assets->raycast(point, {0, -1, 0}, depth + .01f);
                supported = surface && surface->normal.y > .2f;
            }
            if (!supported) return false;
        }
    }
    return true;
}

void saveSection(const World& world, const SectionMetadata& meta, std::span<const HubAsset> assets,
                 const std::filesystem::path& path, bool overwrite)
{
    const auto bytes = encode(world, meta, assets);
    atomicWrite(bytes, path, overwrite);
}

SectionDocument loadSection(const std::filesystem::path& path)
{
    auto parsed = read(path);
    std::istringstream snapshot(parsed.worldData, std::ios::binary);
    auto world = decodeWorld(snapshot);
    // Loading also serves the creative editor: old sections must stay openable
    // for repair when newly collidable scenery obstructs a former spawn point.
    // Test mode and level loading validate support before positioning a player.
    return {std::move(world), std::move(parsed.meta), std::move(parsed.assets)};
}

std::filesystem::path saveSectionDraft(const World& world, const SectionMetadata& meta,
                                      std::span<const HubAsset> assets, const std::filesystem::path& libraryRoot)
{
    validate(meta, assets);
    const std::array<std::string_view, 2> parts{"drafts", Worlds[static_cast<unsigned>(meta.world)].folder};
    return numberedSave(world, meta, assets, directory(libraryRoot, parts));
}

std::filesystem::path acceptSection(const World& world, const SectionMetadata& meta,
                                   std::span<const HubAsset> assets, const std::filesystem::path& libraryRoot)
{
    validate(meta, assets);
    if (!meta.tested) throw std::runtime_error("Reach the finish in Test mode before accepting this section");
    AssetCollision collision;
    collision.rebuild(assets);
    if (!sectionMarkerSafe(world, meta.spawn, &collision) || !sectionMarkerSafe(world, meta.finish, &collision)
        || (meta.checkpoint && !sectionMarkerSafe(world, *meta.checkpoint, &collision))) {
        throw std::runtime_error("Start, checkpoint and finish need solid footing and clear headroom");
    }
    const std::array parts{Worlds[static_cast<unsigned>(meta.world)].folder,
                           Categories[static_cast<unsigned>(meta.category)].folder};
    return numberedSave(world, meta, assets, directory(libraryRoot, parts));
}

std::vector<SectionEntry> listSections(const std::filesystem::path& libraryRoot, std::optional<SectionWorld> filter)
{
    std::vector<SectionEntry> result;
    if (filter && !validWorld(*filter)) return result;
    for (const auto& world : Worlds) {
        if (filter && world.id != *filter) continue;
        const auto worldFolder = libraryRoot / world.folder;
        if (std::filesystem::is_symlink(worldFolder)) continue;
        for (const auto& category : Categories) collect(result, worldFolder / category.folder, world.id, category.id);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
    return result;
}

std::vector<SectionEntry> listSectionDrafts(const std::filesystem::path& libraryRoot)
{
    std::vector<SectionEntry> result;
    const auto folder = libraryRoot / "drafts";
    if (!std::filesystem::is_directory(folder) || std::filesystem::is_symlink(folder)) return result;
    for (const auto& world : Worlds) {
        collect(result, folder / world.folder, world.id, std::nullopt);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
    return result;
}

} // namespace voxel
