#include "levels.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory()
    {
        const auto pattern = (std::filesystem::temp_directory_path() / "voxel-levels-test-XXXXXX").string();
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back('\0');
        if (!::mkdtemp(name.data())) throw std::runtime_error("Cannot create level test directory");
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
    std::uint32_t value = 2166136261u;
    for (std::size_t i = 8; i < bytes.size() - 4; ++i) value = (value ^ static_cast<unsigned char>(bytes[i])) * 16777619u;
    integer(bytes, bytes.size() - 4, value);
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
    const auto sections = temp.path / "sections";
    const auto levels = temp.path / "levels";
    check(listLevels(levels).empty(), "missing level libraries should be empty");
    check(!loadLevelDraft(levels, SectionWorld::LushCaves), "a missing draft should return no draft");

    World world(81);
    buildBlankSection(world);
    SectionMetadata meta;
    meta.name = "First roots";
    meta.tested = true;
    const auto first = acceptSection(world, meta, {}, sections);
    const auto firstBytes = readBytes(first);
    meta.name = "Garden stones";
    meta.category = SectionCategory::Exploration;
    meta.spawn = {7.5f, 9.0f, 20.5f};
    meta.finish = {-8.5f, 9.0f, -20.5f};
    const auto second = acceptSection(world, meta, {}, sections);
    const auto relativeFirst = first.lexically_relative(sections);
    const auto relativeSecond = second.lexically_relative(sections);

    LevelDefinition definition;
    definition.name = "First descent";
    definition.number = 5;
    definition.sections = {relativeFirst, relativeSecond, relativeFirst};
    const auto published = saveLevel(definition, sections, levels);
    const auto publishedBytes = readBytes(published);
    check(published == levels / "lush-caves" / "level-005.vlevel", "level number should choose its world slot");
    const auto restored = loadLevel(published);
    check(restored.name == definition.name && restored.number == 5 && restored.world == definition.world
          && restored.sections == definition.sections, "manifest must preserve metadata, order and repeated sections");
    const auto firstDocument = resolveLevelSection(restored, 0, sections);
    const auto secondDocument = resolveLevelSection(restored, 1, sections);
    check(firstDocument.meta.name == "First roots" && secondDocument.meta.spawn == meta.spawn
          && secondDocument.meta.finish == meta.finish, "resolution must load each section's own Start and End positions");
    check(fails([&] { static_cast<void>(resolveLevelSection(restored, 3, sections)); }), "out-of-range section indices must fail");
    check(fails([&] { saveLevel(definition, sections, levels); }), "publishing must not silently replace an occupied level number");
    check(readBytes(published) == publishedBytes, "rejected overwrites must preserve a published level");
    auto changed = definition;
    changed.name = "Revised descent";
    saveLevel(changed, sections, levels, true);
    check(loadLevel(published).name == changed.name, "explicit replacement should update a published level");
    saveLevel(definition, sections, levels, true);
    changed.sections.push_back("lush-caves/parkour/missing-001.vxs");
    check(fails([&] { saveLevel(changed, sections, levels, true); }), "missing sections must fail before replacing an existing level");
    check(readBytes(published) == publishedBytes, "failed replacement validation must preserve old bytes");
    check(readBytes(first) == firstBytes, "level operations must never modify their accepted section revisions");

    // Drafts have their own type and destination, including empty sequences.
    LevelDefinition draft;
    draft.name = "Work in progress";
    const auto draftPath = saveLevelDraft(draft, sections, levels);
    check(draftPath == levels / "drafts" / "lush-caves.vlevel", "draft must be separate from published levels");
    check(loadLevelDraft(levels, draft.world)->sections.empty(), "empty builder state must survive autosave");
    check(fails([&] { static_cast<void>(loadLevel(draftPath)); }), "a draft cannot be loaded as a published level");
    draft = definition;
    draft.number = 1;
    saveLevelDraft(draft, sections, levels);
    check(loadLevelDraft(levels, draft.world)->sections == definition.sections, "draft overwrite must preserve the ordered queue");
    check(readBytes(published) == publishedBytes && listLevels(levels).size() == 1, "draft autosave cannot publish or replace a level");
    const auto oldDraft = readBytes(draftPath);
    changed = draft;
    changed.sections = {"lush-caves/parkour/missing-001.vxs"};
    saveLevelDraft(changed, sections, levels);
    check(loadLevelDraft(levels, draft.world)->sections == changed.sections,
          "draft autosave must preserve missing references so the author can repair them");
    saveLevelDraft(draft, sections, levels);
    changed.sections = {"../escape.vxs"};
    check(fails([&] { saveLevelDraft(changed, sections, levels); }), "unsafe draft reference syntax must fail validation");
    check(readBytes(draftPath) == oldDraft, "a failed autosave must preserve the previous draft");

    auto low = definition;
    low.number = 1;
    saveLevel(low, sections, levels);
    auto high = definition;
    high.number = LevelMaxNumber;
    high.name = std::string(64, 'x');
    high.sections.assign(LevelMaxSections, relativeFirst);
    const auto highPath = saveLevel(high, sections, levels);
    check(loadLevel(highPath).sections.size() == LevelMaxSections, "the maximum section count must round trip");
    const auto ordered = listLevels(levels, SectionWorld::LushCaves);
    check(ordered.size() == 3 && ordered[0].definition.number == 1 && ordered[1].definition.number == 5
          && ordered[2].definition.number == LevelMaxNumber, "levels must sort by numeric slot within each world");
    check(listLevels(levels, SectionWorld::EmberDepths).empty(), "level listing must filter worlds");
    check(listLevels(levels, static_cast<SectionWorld>(999)).empty(), "an unknown world filter must not return all worlds");

    const auto malformed = temp.path / "malformed.vlevel";
    const auto rejects = [&](std::string bytes) {
        writeBytes(malformed, bytes);
        return fails([&] { static_cast<void>(loadLevel(malformed)); });
    };
    check(rejects(publishedBytes + "extra"), "trailing bytes must be rejected");
    check(rejects(publishedBytes.substr(0, publishedBytes.size() - 1)), "truncated manifests must be rejected");
    check(rejects(std::string(100000, 'x')), "oversized manifests must be rejected before allocation");
    auto broken = publishedBytes;
    broken[31] ^= 1;
    check(rejects(broken), "manifest corruption must fail its checksum");
    const auto sectionCountOffset = 28 + definition.name.size();
    const auto pathLengthOffset = sectionCountOffset + 4;
    for (const auto& [offset, value] : std::vector<std::pair<std::size_t, std::uint32_t>>{
             {8, 99}, {12, 2}, {16, 99}, {20, 0}, {20, 1000}, {24, 65},
             {sectionCountOffset, 65}, {sectionCountOffset, 0}, {pathLengthOffset, 0xffffffffu}}) {
        broken = publishedBytes;
        integer(broken, offset, value);
        fixChecksum(broken);
        check(rejects(broken), "invalid bounded manifest fields must be rejected even with a valid checksum");
    }
    broken = publishedBytes;
    broken[28] = '\0';
    fixChecksum(broken);
    check(rejects(broken), "embedded control characters in names must be rejected");

    for (const int invalid : {0, -1, 1000}) {
        changed = definition;
        changed.number = invalid;
        check(fails([&] { saveLevel(changed, sections, levels); }), "out-of-range level slots must not be saved");
    }
    changed = definition;
    changed.sections.clear();
    check(fails([&] { saveLevel(changed, sections, levels, true); }), "empty sequences cannot be published");
    changed.sections.assign(LevelMaxSections + 1, relativeFirst);
    check(fails([&] { saveLevel(changed, sections, levels, true); }), "too many sections must fail before write");
    for (const auto& name : {std::string(), std::string(65, 'x'), std::string("Bad\nname")}) {
        changed = definition;
        changed.name = name;
        check(fails([&] { saveLevel(changed, sections, levels, true); }), "invalid names must not overwrite levels");
    }
    for (const auto& reference : std::vector<std::filesystem::path>{
             first, "../outside.vxs", "lush-caves/parkour/../../outside.vxs", "lush-caves/./parkour/first.vxs",
             "drafts/lush-caves/first.vxs", "crystal-hollows/parkour/first.vxs", "lush-caves/unknown/first.vxs",
             "lush-caves/parkour/first.txt", "lush-caves/parkour/subfolder/first.vxs", "lush-caves/parkour/.vxs",
             "lush-caves/parkour/a\\b.vxs", std::string("lush-caves/parkour/") + std::string(520, 'a') + ".vxs"}) {
        changed = definition;
        changed.sections = {reference};
        check(fails([&] { saveLevel(changed, sections, levels, true); }), "unaccepted, escaping and malformed references must be rejected");
    }
    check(readBytes(published) == publishedBytes, "all rejected definitions must preserve the published file");

    // A section can have a valid codec checksum but still be unsafe to start or
    // finish. Publication and per-section loading must recheck actual voxels.
    const auto forged = sections / "lush-caves" / "parkour" / "forged-001.vxs";
    SectionMetadata badMeta;
    badMeta.tested = false;
    saveSection(world, badMeta, {}, forged);
    changed = definition;
    changed.sections = {forged.lexically_relative(sections)};
    check(fails([&] { validateLevel(changed, sections); }), "untested .vxs files cannot be used in levels");
    const auto forgeTestedSection = [&] {
        // Public saves now check tested markers too. Encode the recoverable
        // draft, then deliberately forge its tested flag and valid checksum to
        // ensure level validation independently rejects unsafe old documents.
        auto draftMeta = badMeta;
        draftMeta.tested = false;
        saveSection(world, draftMeta, {}, forged, true);
        auto bytes = readBytes(forged);
        integer(bytes, 20, 1);
        fixChecksum(bytes);
        writeBytes(forged, bytes);
    };
    badMeta.tested = true;
    badMeta.world = SectionWorld::EmberDepths;
    saveSection(world, badMeta, {}, forged, true);
    check(fails([&] { validateLevel(changed, sections); }), "section metadata must match the chosen world");
    badMeta.world = SectionWorld::LushCaves;
    badMeta.category = SectionCategory::Puzzle;
    saveSection(world, badMeta, {}, forged, true);
    check(fails([&] { validateLevel(changed, sections); }), "section metadata must match its accepted category folder");
    badMeta.category = SectionCategory::Parkour;
    badMeta.spawn.y = 10;
    forgeTestedSection();
    check(fails([&] { validateLevel(changed, sections); }), "floating Start markers must be rejected");
    badMeta.spawn.y = 9;
    world.set({0, 10, -25}, Block::Stone);
    forgeTestedSection();
    check(fails([&] { validateLevel(changed, sections); }), "blocked End markers must be rejected");
    world.set({0, 10, -25}, Block::Air);
    badMeta.checkpoint = glm::vec3{0.5f, 12.0f, 0.5f};
    forgeTestedSection();
    check(fails([&] { validateLevel(changed, sections); }), "unsafe checkpoints must be rejected");
    std::filesystem::remove(forged);

    // Symlinked files or folders cannot redirect the accepted library.
    const auto linked = first.parent_path() / "linked-001.vxs";
    std::filesystem::create_symlink(first, linked);
    changed = definition;
    changed.sections = {linked.lexically_relative(sections)};
    check(fails([&] { validateLevel(changed, sections); }), "even in-library section symlinks must be rejected");
    std::filesystem::remove(linked);
    std::filesystem::create_symlink(temp.path / "does-not-exist.vxs", linked);
    check(fails([&] { validateLevel(changed, sections); }), "dangling section symlinks must be rejected");
    std::filesystem::remove(linked);
    const auto parkour = first.parent_path();
    const auto movedParkour = temp.path / "outside-parkour";
    std::filesystem::rename(parkour, movedParkour);
    std::filesystem::create_directory_symlink(movedParkour, parkour);
    check(fails([&] { validateLevel(definition, sections); }), "symlinked category directories cannot escape the section root");
    std::filesystem::remove(parkour);
    std::filesystem::rename(movedParkour, parkour);

    // A missing/corrupt later segment is detected before a level starts, and a
    // section disappearing after validation is also detected on transition.
    const auto movedSecond = temp.path / "temporarily-moved.vxs";
    std::filesystem::rename(second, movedSecond);
    check(fails([&] { validateLevel(definition, sections); }), "validation must include later sections before playing");
    check(fails([&] { static_cast<void>(resolveLevelSection(definition, 1, sections)); }), "transition resolution must detect deleted sections");
    std::filesystem::rename(movedSecond, second);
    check(!fails([&] { validateLevel(definition, sections); }), "restored accepted sections should become playable again");

    // Listing is resilient to corrupt, misplaced, draft and symlinked entries.
    const auto folder = published.parent_path();
    writeBytes(folder / "level-002.vlevel", "broken");
    writeBytes(folder / "level-003.vlevel", oldDraft);
    writeBytes(folder / "level-004.vlevel", publishedBytes);
    std::filesystem::create_symlink(published, folder / "level-006.vlevel");
    check(listLevels(levels).size() == 3, "invalid, draft, wrong-slot and symlink manifests should not pollute the level picker");
    const auto outside = temp.path / "outside.vlevel";
    writeBytes(outside, "keep me");
    const auto symlinkSlot = folder / "level-007.vlevel";
    std::filesystem::create_symlink(outside, symlinkSlot);
    changed = definition;
    changed.number = 7;
    check(fails([&] { saveLevel(changed, sections, levels, true); }), "published destinations cannot follow symlinks even with explicit replacement");
    check(readBytes(outside) == "keep me", "symlink save failure must preserve external files");
    const auto blockedSlot = folder / "level-008.vlevel";
    std::filesystem::create_directory(blockedSlot);
    writeBytes(blockedSlot / "keep", "keep me");
    changed.number = 8;
    check(fails([&] { saveLevel(changed, sections, levels, true); }), "a directory cannot be replaced by a manifest");
    check(readBytes(blockedSlot / "keep") == "keep me", "failed atomic rename must preserve the destination directory");
    for (const auto& entry : std::filesystem::recursive_directory_iterator(levels)) {
        check(!entry.path().filename().string().starts_with(".level-save-"), "atomic saves must clean temporary files on success and failure");
    }
    const auto linkedRoot = temp.path / "linked-library";
    std::filesystem::create_directory(linkedRoot);
    std::filesystem::create_directory_symlink(folder, linkedRoot / "lush-caves");
    check(fails([&] { saveLevel(definition, sections, linkedRoot); }), "symlinked output world folders must be rejected");
    check(listLevels(linkedRoot).empty(), "listing must not follow symlinked world folders");

    if (failures == 0) std::cout << "Level library tests passed\n";
    return failures == 0 ? 0 : 1;
}
