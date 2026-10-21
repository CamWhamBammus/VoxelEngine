#include "sections.h"
#include "asset_collision.h"
#include "player.h"

#include <array>
#include <cmath>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory()
    {
        auto pattern = (std::filesystem::temp_directory_path() / "voxel-sections-test-XXXXXX").string();
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back('\0');
        if (!::mkdtemp(name.data())) throw std::runtime_error("Cannot make section test directory");
        path = name.data();
    }
    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
template<class Operation> bool fails(Operation operation)
{
    try { operation(); return false; }
    catch (const std::exception&) { return true; }
}
std::string readBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void writeBytes(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
void integer(std::string& bytes, std::size_t offset, std::uint32_t value)
{
    for (int i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<char>(value >> (8 * i));
}
void fixChecksum(std::string& bytes)
{
    std::uint32_t checksum = 2166136261u;
    for (std::size_t i = 8; i < bytes.size() - 4; ++i) checksum = (checksum ^ static_cast<unsigned char>(bytes[i])) * 16777619u;
    integer(bytes, bytes.size() - 4, checksum);
}
std::string olderVersion(std::string_view bytes, std::size_t firstAsset, std::span<const voxel::HubAsset> assets,
                         std::uint32_t version)
{
    // Version 1 used the same metadata and voxel snapshot, but each model was
    // exactly 24 bytes. Version 2 added chest contents; neither had pickupItem.
    std::string old(bytes.substr(0, firstAsset));
    std::size_t cursor = firstAsset;
    for (const auto& asset : assets) {
        old += bytes.substr(cursor, version == 1 ? 24 : 28 + asset.chestItems.size() * 8);
        cursor += 36 + asset.chestItems.size() * 8;
    }
    old += bytes.substr(cursor);
    integer(old, 8, version);
    fixChecksum(old);
    return old;
}
} // namespace

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) { std::cerr << message << '\n'; ++failures; }
    };
    TemporaryDirectory temp;
    check(sectionWorldCatalog().size() == 3, "three world collections must be available");
    check(sectionCategoryCatalog().size() == 5, "five section categories must be available");
    check(sectionAssetCatalog().size() == HubAssetCount, "every scenery, building, and loot asset must be exposed");
    check(static_cast<int>(HubAssetKind::HangingPlanter) == 53
          && static_cast<int>(HubAssetKind::LootChest) == 54
          && static_cast<int>(HubAssetKind::CrystalChest) == 55
          && static_cast<int>(HubAssetKind::IronboundChest) == 56
          && static_cast<int>(HubAssetKind::KeyPickup) == 57
          && static_cast<int>(HubAssetKind::ItemPickup) == 58,
          "new building pieces never change earlier saved asset identifiers");
    constexpr std::array buildingKit{
        HubAssetKind::TimberFence, HubAssetKind::PicketFence, HubAssetKind::FenceCorner,
        HubAssetKind::GardenGate, HubAssetKind::IronRailing, HubAssetKind::TimberPillar,
        HubAssetKind::TimberBeam, HubAssetKind::DiagonalBrace, HubAssetKind::StoneDoorway,
        HubAssetKind::WindowFrame, HubAssetKind::WoodenStairs, HubAssetKind::Barrel
    };
    for (std::size_t i = 0; i < buildingKit.size(); ++i) {
        check(static_cast<std::size_t>(buildingKit[i]) == 59 + i,
              "building pieces append with stable document identifiers");
        check(sectionAssetCatalog()[59 + i].kind == buildingKit[i],
              "each new building piece is independently available in the creative catalog");
    }
    check(sectionAssetCatalog()[54].name == "Wooden chest"
          && sectionAssetCatalog()[55].name == "Crystal chest"
          && sectionAssetCatalog()[56].name == "Ironbound chest"
          && sectionAssetCatalog()[57].name == "Cave key",
          "the creative menu exposes three separate chest styles and a key pickup");
    std::set<HubAssetKind> uniqueAssets;
    for (const auto& info : sectionAssetCatalog()) {
        uniqueAssets.insert(info.kind);
        check(!info.name.empty() && !info.group.empty(), "asset names and groups must be readable");
    }
    check(uniqueAssets.size() == sectionAssetCatalog().size(), "asset catalog must not contain duplicates");
    check(PlaceableBlocks.size() == BlockCount - 2 - 8 * 3,
          "the builder exposes each material with one entry per rotatable stair kind");
    check(sectionInterior({-32, 9, -32}) && sectionInterior({31, 56, 31}), "both interior corners must be editable");
    for (const auto cell : {glm::ivec3{-33, 9, 0}, {32, 9, 0}, {0, 8, 0}, {0, 57, 0}, {0, 9, -33}, {0, 9, 32}}) {
        check(!sectionInterior(cell), "the enclosing shell must be protected");
    }

    World world(731);
    world.set({80, 80, 80}, Block::Crystal);
    buildBlankSection(world);
    check(world.seed() == 731, "blank room must preserve the world's seed");
    check(world.get({80, 80, 80}) == Block::Air, "blank room must clear the old world");
    for (int x = -32; x < 32; ++x) {
        for (int z = -32; z < 32; ++z) {
            check(world.get({x, 8, z}) == Block::Stone && world.get({x, 57, z}) == Block::Stone,
                  "blank room needs a continuous floor and ceiling");
            check(world.get({x, 9, z}) == Block::Air, "the build floor must start clear");
        }
    }
    for (int along = -32; along < 32; ++along) {
        for (int y = 9; y < 57; ++y) {
            check(isSolid(world.get({-33, y, along})) && isSolid(world.get({32, y, along}))
                    && isSolid(world.get({along, y, -33})) && isSolid(world.get({along, y, 32})),
                  "room walls must enclose the whole interior");
        }
    }

    SectionMetadata meta;
    meta.name = "Fern bridge";
    meta.checkpoint = glm::vec3{12.5f, 9.0f, 0.5f};
    check(sectionMarkerSafe(world, meta.spawn) && sectionMarkerSafe(world, meta.finish)
          && sectionMarkerSafe(world, *meta.checkpoint), "default and supported markers must be safe");
    check(!sectionMarkerSafe(world, {0.5f, 10.0f, 24.5f}), "floating markers must be rejected");
    check(!sectionMarkerSafe(world, {31.9f, 9.0f, 0.5f}), "markers must leave room for the player width");
    check(!sectionMarkerSafe(world, {0.5f, 9.0f, std::numeric_limits<float>::quiet_NaN()}), "non-finite markers must be rejected");
    world.set({0, 10, 24}, Block::Stone);
    check(!sectionMarkerSafe(world, meta.spawn), "headroom must be checked at markers");
    world.set({0, 10, 24}, Block::Air);
    world.set({0, 8, 24}, Block::Water);
    check(!sectionMarkerSafe(world, meta.spawn), "water is not marker support");
    world.set({0, 8, 24}, Block::Stone);

    std::vector<HubAsset> assets;
    for (const auto& info : sectionAssetCatalog()) {
        const auto index = assets.size();
        assets.push_back({info.kind, {-24.5f + static_cast<float>(index % 12) * 4.0f, 9.0f,
                                      -16.5f + static_cast<float>(index / 12) * 4.0f},
                          static_cast<int>(index % 4), static_cast<std::uint32_t>(100 + index)});
        if (info.kind == HubAssetKind::ItemPickup) assets.back().pickupItem = {ItemId::Ruby, 3};
    }
    for (std::size_t i = 0; i < PlaceableBlocks.size(); ++i) world.set({static_cast<int>(i) - 10, 9, 4}, PlaceableBlocks[i]);
    const auto document = temp.path / "complete.vxs";
    saveSection(world, meta, assets, document);
    const auto originalBytes = readBytes(document);
    const auto restored = loadSection(document);
    check(restored.world.sameContents(world), "section must round trip all voxels and seed");
    check(restored.meta.name == meta.name && restored.meta.world == meta.world && restored.meta.category == meta.category
          && restored.meta.spawn == meta.spawn && restored.meta.finish == meta.finish
          && restored.meta.checkpoint == meta.checkpoint && restored.meta.tested == meta.tested,
          "section must round trip metadata and markers");
    check(restored.assets.size() == assets.size(), "section must preserve the model count");
    for (std::size_t i = 0; i < assets.size(); ++i) {
        check(restored.assets[i].kind == assets[i].kind && restored.assets[i].position == assets[i].position
              && restored.assets[i].facing == assets[i].facing && restored.assets[i].variation == assets[i].variation
              && restored.assets[i].pickupItem == assets[i].pickupItem,
              "section must round trip every model, orientation and pickup stack");
    }
    std::vector<HubAsset> chests{
        {HubAssetKind::LootChest, {-12.5f,9,10.5f}, 0, 0},
        {HubAssetKind::CrystalChest, {-8.5f,9,10.5f}, 1, 1},
        {HubAssetKind::IronboundChest, {-4.5f,9,10.5f}, 3, 2,
            {{ItemId::CopperOre, 19}, {ItemId::Key, 2}, {ItemId::CopperOre, 3}}},
        {HubAssetKind::KeyPickup, {4.5f,9,10.5f}, 0, 0},
    };
    for (int i = 0; i < ChestSlotCount; ++i)
        chests[0].chestItems.push_back({static_cast<ItemId>(i % ItemCount), ChestStackLimit - i});
    const auto chestDocument = temp.path / "authored-chests.vxs";
    saveSection(world, meta, chests, chestDocument);
    check(loadSection(chestDocument).assets == chests,
          "all styles, rotations, empty chests, keys and exact twenty-stack contents survive a section save");
    std::vector<HubAsset> pickups;
    for (const auto& item : itemCatalog())
        pickups.push_back({HubAssetKind::ItemPickup, {-28.f + 3.f * static_cast<int>(item.id), 9, 16},
                           static_cast<int>(item.id) % 4, 0, {}, {item.id, ChestStackLimit - static_cast<int>(item.id)}});
    const auto pickupDocument = temp.path / "ground-items.vxs";
    saveSection(world, meta, pickups, pickupDocument);
    check(loadSection(pickupDocument).assets == pickups,
          "every resource and key preserves its exact ground pickup quantity and order");
    const std::vector<HubAsset> markerDrops{
        {HubAssetKind::ItemPickup, meta.spawn, 0, 0, {}, {ItemId::Ruby, 3}},
        {HubAssetKind::KeyPickup, meta.finish}
    };
    AssetCollision pickupCollision;
    pickupCollision.rebuild(markerDrops);
    check(sectionMarkerSafe(world, meta.spawn, &pickupCollision)
          && sectionMarkerSafe(world, meta.finish, &pickupCollision),
          "dropped items at Start or End never block player headroom or marker safety");
    const auto markerDocument = temp.path / "marker-drops.vxs";
    auto testedMarkers = meta;
    testedMarkers.tested = true;
    saveSection(world, testedMarkers, markerDrops, markerDocument);
    check(loadSection(markerDocument).assets == markerDrops,
          "tested sections preserve pickup spawn points that overlap player markers");
    check(fails([&] { saveSection(world, meta, {}, document); }), "a new save cannot replace an existing document");
    check(readBytes(document) == originalBytes, "rejected overwrite must preserve the original document");
    SectionMetadata replacement = meta;
    replacement.name = "Changed name";
    saveSection(world, replacement, {}, document, true);
    check(loadSection(document).meta.name == replacement.name, "explicit draft overwrites must succeed");
    saveSection(world, meta, assets, document, true);
    check(fails([&] { saveSection(world, meta, assets, temp.path / "missing" / "file.vxs"); }), "low-level saves must report missing parents");
    const auto blocked = temp.path / "blocked.vxs";
    std::filesystem::create_directory(blocked);
    writeBytes(blocked / "keep", "untouched");
    check(fails([&] { saveSection(world, meta, assets, blocked, true); }), "saving over a directory must fail");
    check(readBytes(blocked / "keep") == "untouched", "a failed atomic save must preserve the destination");
    for (const auto& entry : std::filesystem::directory_iterator(temp.path)) {
        check(!entry.path().filename().string().starts_with(".section-save-"), "atomic saves must clean temporary files");
    }

    const auto malformed = temp.path / "malformed.vxs";
    const auto rejects = [&](std::string bytes) {
        writeBytes(malformed, bytes);
        return fails([&] { static_cast<void>(loadSection(malformed)); });
    };
    check(rejects(originalBytes + "extra"), "trailing data must be rejected");
    check(rejects(originalBytes.substr(0, originalBytes.size() - 1)), "truncated documents must be rejected");
    auto corrupt = originalBytes;
    corrupt[30] ^= 1;
    check(rejects(corrupt), "metadata corruption must be detected by the checksum");
    const std::size_t assetCountOffset = 56 + meta.name.size() + 12;
    const std::size_t firstAsset = assetCountOffset + 4;
    for (const auto& [offset, value] : std::vector<std::pair<std::size_t, std::uint32_t>>{
             {8, 99}, {12, 9}, {16, 9}, {20, 2}, {24, 1000000},
             {assetCountOffset, static_cast<std::uint32_t>(SectionMaxAssets + 1)},
             {firstAsset, 10000}, {firstAsset + 16, 4},
             {firstAsset + 4, 0x7fc00000u}, // NaN model X
             {firstAsset + assets.size() * 36, 0xffffffffu}}) {
        corrupt = originalBytes;
        integer(corrupt, offset, value);
        fixChecksum(corrupt);
        check(rejects(corrupt), "unsupported or unbounded document fields must be rejected before allocation");
    }
    replacement = meta;
    replacement.world = static_cast<SectionWorld>(999);
    check(fails([&] { saveSection(world, replacement, assets, malformed, true); }), "invalid in-memory metadata cannot be saved");
    replacement = meta;
    replacement.finish.y = std::numeric_limits<float>::infinity();
    check(fails([&] { saveSection(world, replacement, assets, malformed, true); }), "non-finite markers cannot be saved");
    auto badAssets = assets;
    badAssets.front().position.x = 200;
    check(fails([&] { saveSection(world, meta, badAssets, malformed, true); }), "models outside the room cannot be saved");

    const auto authoredBytes = readBytes(chestDocument);
    for (const auto& [offset, value] : std::vector<std::pair<std::size_t, std::uint32_t>>{
             {firstAsset + 24, ChestSlotCount + 1}, {firstAsset + 24, 0xffffffffu},
             {firstAsset + 28, static_cast<std::uint32_t>(ItemId::Count)},
             {firstAsset + 28, 0xffffffffu}, {firstAsset + 32, 0},
             {firstAsset + 32, ChestStackLimit + 1}, {firstAsset + 32, 0xffffffffu},
             {firstAsset, static_cast<std::uint32_t>(HubAssetKind::Fern)}}) {
        corrupt = authoredBytes;
        integer(corrupt, offset, value);
        fixChecksum(corrupt);
        check(rejects(corrupt), "malformed authored item counts, ids, quantities and non-chest containers are rejected");
    }
    auto truncatedItems = authoredBytes.substr(0, firstAsset + 32);
    truncatedItems.resize(truncatedItems.size() + 4);
    fixChecksum(truncatedItems);
    check(rejects(truncatedItems), "truncated item stacks are rejected before loading a section");
    auto invalidChest = chests;
    for (const auto invalid : {ItemStack{ItemId::Ruby, 0}, ItemStack{ItemId::Ruby, -1},
                               ItemStack{ItemId::Ruby, 100}, ItemStack{ItemId::Count, 1}}) {
        invalidChest[0].chestItems[0] = invalid;
        check(fails([&] { saveSection(world, meta, invalidChest, chestDocument, true); }),
              "invalid in-memory chest items cannot overwrite a valid section");
        check(readBytes(chestDocument) == authoredBytes, "failed chest validation preserves the saved file");
    }
    invalidChest = chests;
    invalidChest[0].chestItems.push_back({ItemId::Coal, 1});
    check(fails([&] { saveSection(world, meta, invalidChest, malformed, true); }),
          "more than twenty authored stacks cannot be saved");
    invalidChest = chests;
    invalidChest.back().chestItems = {{ItemId::Key, 1}};
    check(fails([&] { saveSection(world, meta, invalidChest, malformed, true); }),
          "key pickup models cannot hold chest contents");

    std::vector<HubAsset> oldAssets{
        {HubAssetKind::LootChest, {-12.5f,9,10.5f}, 0, 0},
        {HubAssetKind::LootChest, {-8.5f,9,10.5f}, 1, 4},
        {HubAssetKind::LootChest, {-4.5f,9,10.5f}, 3, 8},
        {HubAssetKind::Fern, {10.5f,9,10.5f}, 2, 2},
    };
    const auto oldFile = temp.path / "legacy-chests.vxs";
    saveSection(world, meta, oldAssets, oldFile);
    const auto legacy = olderVersion(readBytes(oldFile), firstAsset, oldAssets, 1);
    writeBytes(oldFile, legacy);
    const auto imported = loadSection(oldFile);
    check(imported.world.sameContents(world) && imported.meta.name == meta.name
          && imported.assets[0].kind == HubAssetKind::LootChest
          && imported.assets[1].kind == HubAssetKind::CrystalChest
          && imported.assets[2].kind == HubAssetKind::IronboundChest
          && imported.assets[3] == oldAssets[3],
          "version one imports preserve terrain, metadata and original chest appearances");
    for (std::size_t i = 0; i < 3; ++i) {
        std::uint32_t seed = 0;
        for (int axis = 0; axis < 3; ++axis)
            seed = (seed ^ std::bit_cast<std::uint32_t>(oldAssets[i].position[axis])) * 16777619u;
        check(imported.assets[i].chestItems == legacyChestLoot(oldAssets[i].variation, seed),
              "legacy presets materialize once using the original position-based roll with seed zero");
    }
    check(loadSection(oldFile).assets == imported.assets && readBytes(oldFile) == legacy,
          "legacy migration is repeatable and never rewrites the source during load");
    saveSection(imported.world, imported.meta, imported.assets, oldFile, true);
    check(loadSection(oldFile).assets == imported.assets && readBytes(oldFile) != legacy,
          "saving a migrated room writes stable explicit contents in the current version");
    auto invalidOldKind = legacy;
    integer(invalidOldKind, firstAsset, static_cast<std::uint32_t>(HubAssetKind::CrystalChest));
    fixChecksum(invalidOldKind);
    check(rejects(invalidOldKind), "version one rejects asset ids that were introduced with version two");

    const auto versionTwo = olderVersion(authoredBytes, firstAsset, chests, 2);
    writeBytes(oldFile, versionTwo);
    const auto oldKeys = loadSection(oldFile);
    check(oldKeys.assets == chests && oldKeys.assets.back().kind == HubAssetKind::KeyPickup
          && itemPickupStack(oldKeys.assets.back()) == ItemStack{ItemId::Key, 1}
          && readBytes(oldFile) == versionTwo,
          "version two retains legacy key kind, placement, quantity and asset order without rewriting the source");
    saveSection(oldKeys.world, oldKeys.meta, oldKeys.assets, oldFile, true);
    check(loadSection(oldFile).assets == oldKeys.assets,
          "saving legacy keys in version three preserves their stable source identities");
    corrupt = versionTwo;
    integer(corrupt, firstAsset, static_cast<std::uint32_t>(HubAssetKind::ItemPickup));
    fixChecksum(corrupt);
    check(rejects(corrupt), "version two rejects the newer generic pickup kind");

    const auto pickupBytes = readBytes(pickupDocument);
    for (const auto& [offset, value] : std::vector<std::pair<std::size_t, std::uint32_t>>{
             {firstAsset + 28, static_cast<std::uint32_t>(ItemId::Count)},
             {firstAsset + 28, 0xffffffffu}, {firstAsset + 32, 0},
             {firstAsset + 32, ChestStackLimit + 1}, {firstAsset + 32, 0xffffffffu},
             {firstAsset, static_cast<std::uint32_t>(HubAssetKind::Fern)},
             {firstAsset, static_cast<std::uint32_t>(HubAssetKind::KeyPickup)}}) {
        corrupt = pickupBytes;
        integer(corrupt, offset, value);
        fixChecksum(corrupt);
        check(rejects(corrupt), "invalid pickup ids, amounts and payloads on other asset kinds are rejected");
    }
    for (const auto invalid : {ItemStack{ItemId::Ruby, 0}, ItemStack{ItemId::Ruby, -1},
                               ItemStack{ItemId::Ruby, 100}, ItemStack{ItemId::Count, 1}}) {
        auto invalidPickups = pickups;
        invalidPickups.front().pickupItem = invalid;
        check(fails([&] { saveSection(world, meta, invalidPickups, pickupDocument, true); })
              && readBytes(pickupDocument) == pickupBytes,
              "invalid pickup stacks cannot replace a valid section file");
    }
    auto invalidPayload = pickups;
    invalidPayload.front().chestItems = {{ItemId::Ruby, 1}};
    check(fails([&] { saveSection(world, meta, invalidPayload, malformed, true); }),
          "ground item pickups cannot also contain chest stacks");
    invalidPayload = chests;
    invalidPayload.back().pickupItem = {ItemId::Key, 0};
    check(fails([&] { saveSection(world, meta, invalidPayload, malformed, true); }),
          "non-pickup payloads require the canonical empty value even when count is zero");
    corrupt = originalBytes;
    integer(corrupt, firstAsset + 28, static_cast<std::uint32_t>(ItemId::Key));
    fixChecksum(corrupt);
    check(rejects(corrupt), "version three rejects typed zero-count pickup payloads on ordinary models");

    const auto library = temp.path / "sections";
    check(listSections(library).empty() && listSectionDrafts(library).empty(), "a missing library should be empty");
    check(fails([&] { acceptSection(world, meta, assets, library); }), "untested sections cannot enter the accepted library");
    check(!std::filesystem::exists(library), "failed acceptance should not create library folders");
    const auto draft1 = saveSectionDraft(world, meta, assets, library);
    const auto draft2 = saveSectionDraft(world, meta, assets, library);
    check(draft1 != draft2 && std::filesystem::exists(draft1), "new drafts must preserve previous work");
    check(draft1.parent_path() == library / "drafts" / "lush-caves", "drafts belong in their own world folder");
    check(listSections(library).empty() && listSectionDrafts(library).size() == 2, "drafts must not appear as accepted sections");
    meta.tested = true;
    const auto accepted1 = acceptSection(world, meta, assets, library);
    const auto acceptedBytes = readBytes(accepted1);
    const auto accepted2 = acceptSection(world, meta, assets, library);
    check(accepted1 != accepted2 && readBytes(accepted1) == acceptedBytes, "repeated accepts must preserve earlier sections");
    check(accepted1.parent_path() == library / "lush-caves" / "parkour", "accepted paths must reflect world and category");
    check(listSections(library, SectionWorld::LushCaves).size() == 2
          && listSections(library, SectionWorld::EmberDepths).empty(), "library filtering must use the chosen world");
    replacement = meta;
    replacement.name = "../../escaped";
    replacement.world = SectionWorld::CrystalHollows;
    replacement.category = SectionCategory::Puzzle;
    const auto sanitized = acceptSection(world, replacement, {}, library);
    check(sanitized.parent_path() == library / "crystal-hollows" / "puzzle"
          && sanitized.filename() == "escaped-001.vxs", "section names cannot escape the library path");
    check(listSections(library).size() == 3, "the library must list all accepted worlds");
    writeBytes(accepted1.parent_path() / "broken.vxs", "bad");
    saveSection(world, replacement, {}, accepted1.parent_path() / "wrong-world.vxs");
    std::filesystem::create_symlink(accepted1, accepted1.parent_path() / "linked.vxs");
    check(listSections(library).size() == 3, "corrupt files, symlinks, and misplaced world metadata must not pollute the library");
    replacement = meta;
    replacement.checkpoint = glm::vec3{0.5f, 11.0f, 0.5f};
    check(fails([&] { acceptSection(world, replacement, {}, library); }), "unsafe checkpoints must block acceptance");
    const auto redirected = temp.path / "redirected";
    std::filesystem::create_directory(redirected);
    std::filesystem::create_directory_symlink(temp.path, redirected / "lush-caves");
    check(fails([&] { acceptSection(world, meta, {}, redirected); }), "symlinked world folders cannot redirect library saves");

    // Real model tops are not integer voxel heights. The document stores that
    // exact standable height and revalidates it against the same model geometry.
    World parkour;
    buildBlankSection(parkour);
    std::vector<HubAsset> platforms{{HubAssetKind::CrateStack, {20, 9, 20}, 1, 1},
                                    {HubAssetKind::GiantMushroom, {20, 9, 0}, 2, 1}};
    AssetCollision collision;
    collision.rebuild(platforms);
    const auto settle = [&](glm::vec3 feet) {
        constexpr float half = Player::Width * .5f;
        feet.y += collision.sweep(feet + glm::vec3(-half, 0, -half),
                                  feet + glm::vec3(half, Player::Height, half), 1, -6);
        return feet;
    };
    SectionMetadata modelMeta;
    modelMeta.spawn = settle({20, 15, 20});
    modelMeta.finish = settle({20, 15, 0});
    modelMeta.tested = true;
    check(std::abs(modelMeta.spawn.y - std::round(modelMeta.spawn.y)) > .01f
          && sectionMarkerSafe(parkour, modelMeta.spawn, &collision),
          "fractional crate tops support safe section markers");
    check(sectionMarkerSafe(parkour, modelMeta.finish, &collision),
          "sloped mushroom caps support markers at the full body's resting height");
    check(!sectionMarkerSafe(parkour, modelMeta.spawn)
          && !sectionMarkerSafe(parkour, modelMeta.spawn + glm::vec3(0, .1f, 0), &collision),
          "model markers need actual contact with their support, not a nearby hovering point");
    const auto modelFile = acceptSection(parkour, modelMeta, platforms, temp.path / "model-library");
    const auto modelRestored = loadSection(modelFile);
    check(modelRestored.meta.spawn == modelMeta.spawn && modelRestored.meta.finish == modelMeta.finish,
          "accepted model parkour preserves exact fractional start and end positions");
    auto obstructed = platforms;
    obstructed.push_back({HubAssetKind::Boulder, modelMeta.spawn, 0, 0});
    collision.rebuild(obstructed);
    check(!sectionMarkerSafe(parkour, modelMeta.spawn, &collision), "models cannot occupy marker headroom");
    check(fails([&] { acceptSection(parkour, modelMeta, obstructed, temp.path / "model-library"); })
          && fails([&] { saveSection(parkour, modelMeta, obstructed, temp.path / "obstructed.vxs"); }),
          "tested saves and acceptance reject model-blocked markers");
    modelMeta.tested = false;
    const auto repair = temp.path / "repair.vxs";
    saveSection(parkour, modelMeta, obstructed, repair);
    check(loadSection(repair).assets.size() == obstructed.size(), "incomplete drafts stay recoverable with blocked markers");
    auto forged = readBytes(repair);
    integer(forged, 20, 1);
    fixChecksum(forged);
    writeBytes(repair, forged);
    check(loadSection(repair).assets.size() == obstructed.size(),
          "old accepted documents remain openable for repair after collision rules change");

    if (failures == 0) std::cout << "Section room, catalogs, persistence and library checks passed\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
