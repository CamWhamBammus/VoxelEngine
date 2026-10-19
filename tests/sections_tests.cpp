#include "sections.h"

#include <cmath>
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
    check(sectionAssetCatalog().size() == HubAssetCount, "every existing and lush asset must be exposed");
    std::set<HubAssetKind> uniqueAssets;
    for (const auto& info : sectionAssetCatalog()) {
        uniqueAssets.insert(info.kind);
        check(!info.name.empty() && !info.group.empty(), "asset names and groups must be readable");
    }
    check(uniqueAssets.size() == sectionAssetCatalog().size(), "asset catalog must not contain duplicates");
    check(PlaceableBlocks.size() == BlockCount - 2, "the builder exposes every material except air and protected bedrock");
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
              && restored.assets[i].facing == assets[i].facing && restored.assets[i].variation == assets[i].variation,
              "section must round trip every model and its orientation");
    }
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
             {firstAsset + assets.size() * 24, 0xffffffffu}}) {
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

    if (failures == 0) std::cout << "Section room, catalogs, persistence and library checks passed\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
