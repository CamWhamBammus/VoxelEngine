#include "levels.h"
#include "asset_collision.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <unistd.h>

namespace voxel {
namespace {

constexpr std::array<char, 8> Magic{'V', 'X', 'L', 'E', 'V', 'E', 'L', '\0'};
constexpr std::uint32_t Version = 1;
constexpr std::size_t MaxName = 64;
constexpr std::size_t MaxPath = 512;
constexpr std::size_t MaxManifestBytes = 128 + LevelMaxSections * (MaxPath + 4);

const SectionWorldInfo& worldInfo(SectionWorld world)
{
    const auto worlds = sectionWorldCatalog();
    const auto found = std::find_if(worlds.begin(), worlds.end(), [world](const auto& item) { return item.id == world; });
    if (found == worlds.end()) throw std::runtime_error("Unknown level world");
    return *found;
}

bool printable(std::string_view text)
{
    return std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
}

// Only the accepted library layout is legal. In particular, neither drafts nor
// relative traversals can be smuggled into a published sequence.
SectionCategory referenceCategory(const std::filesystem::path& reference, SectionWorld world)
{
    const auto text = reference.generic_string();
    if (text.empty() || text.size() > MaxPath || !printable(text) || text.find('\\') != std::string::npos
        || reference.is_absolute() || reference.has_root_path() || reference.extension() != ".vxs") {
        throw std::runtime_error("Level sections must use relative accepted .vxs paths");
    }
    std::vector<std::string> parts;
    for (const auto& part : reference) {
        if (part.empty() || part == "." || part == "..") throw std::runtime_error("Level section paths cannot contain traversals");
        parts.push_back(part.string());
    }
    if (parts.size() != 3 || parts[0] != worldInfo(world).folder || parts[2] == ".vxs") {
        throw std::runtime_error("Level sections must come from the selected world's accepted library");
    }
    for (const auto& category : sectionCategoryCatalog()) {
        if (parts[1] == category.folder) return category.id;
    }
    throw std::runtime_error("Unknown category in level section path");
}

void validateFields(const LevelDefinition& definition, bool draft)
{
    worldInfo(definition.world);
    if (definition.number < 1 || definition.number > LevelMaxNumber) throw std::runtime_error("Level number must be between 1 and 999");
    if (definition.name.empty() || definition.name.size() > MaxName || !printable(definition.name)) {
        throw std::runtime_error("Level names must contain 1 to 64 printable characters");
    }
    if ((!draft && definition.sections.empty()) || definition.sections.size() > LevelMaxSections) {
        throw std::runtime_error("A level needs between 1 and 64 sections");
    }
    for (const auto& reference : definition.sections) referenceCategory(reference, definition.world);
}

std::filesystem::path resolveReference(const std::filesystem::path& reference,
                                       const std::filesystem::path& root)
{
    if (root.empty() || !std::filesystem::is_directory(root)) throw std::runtime_error("Section library directory is missing");
    const auto canonicalRoot = std::filesystem::canonical(root);
    auto current = canonicalRoot;
    for (const auto& part : reference) {
        current /= part;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(current))) {
            throw std::runtime_error("Level section references cannot follow symbolic links");
        }
    }
    if (!std::filesystem::is_regular_file(current)) throw std::runtime_error("Level section is missing: " + reference.generic_string());
    const auto relative = std::filesystem::canonical(current).lexically_relative(canonicalRoot);
    if (relative.empty() || *relative.begin() == "..") throw std::runtime_error("Level section escaped its library");
    return current;
}

SectionDocument readReference(const LevelDefinition& definition, const std::filesystem::path& reference,
                               const std::filesystem::path& root)
{
    const auto category = referenceCategory(reference, definition.world);
    auto section = loadSection(resolveReference(reference, root));
    if (section.meta.world != definition.world || section.meta.category != category) {
        throw std::runtime_error("Level section does not match its world or category folder");
    }
    if (!section.meta.tested) throw std::runtime_error("A level can only use tested, accepted sections");
    AssetCollision collision;
    collision.rebuild(section.assets);
    if (!sectionMarkerSafe(section.world, section.meta.spawn, &collision) || !sectionMarkerSafe(section.world, section.meta.finish, &collision)
        || (section.meta.checkpoint && !sectionMarkerSafe(section.world, *section.meta.checkpoint, &collision))) {
        throw std::runtime_error("Level section Start, End and checkpoint need solid footing and clear headroom");
    }
    return section;
}

void validateReferences(const LevelDefinition& definition, const std::filesystem::path& root)
{
    std::unordered_set<std::string> checked;
    for (const auto& reference : definition.sections) {
        if (checked.insert(reference.generic_string()).second) {
            // Release each snapshot before reading the next one. A full level
            // does not need to keep dozens of worlds in memory to validate.
            static_cast<void>(readReference(definition, reference, root));
        }
    }
}

std::uint32_t checksum(std::string_view bytes)
{
    std::uint32_t result = 2166136261u;
    for (const unsigned char byte : bytes) result = (result ^ byte) * 16777619u;
    return result;
}
void append(std::string& bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<char>(value >> shift));
}
void append(std::string& bytes, std::string_view value)
{
    append(bytes, static_cast<std::uint32_t>(value.size()));
    bytes += value;
}
std::string encode(const LevelDefinition& definition, bool draft)
{
    validateFields(definition, draft);
    std::string bytes(Magic.data(), Magic.size());
    append(bytes, Version);
    append(bytes, draft ? 1u : 0u);
    append(bytes, static_cast<std::uint32_t>(definition.world));
    append(bytes, static_cast<std::uint32_t>(definition.number));
    append(bytes, definition.name);
    append(bytes, static_cast<std::uint32_t>(definition.sections.size()));
    for (const auto& reference : definition.sections) append(bytes, reference.generic_string());
    append(bytes, checksum(std::string_view(bytes).substr(Magic.size())));
    return bytes;
}
struct Reader {
    std::string_view bytes;
    std::size_t cursor = 0;
    std::string_view take(std::size_t count)
    {
        if (count > bytes.size() - cursor) throw std::runtime_error("Truncated level manifest");
        const auto value = bytes.substr(cursor, count);
        cursor += count;
        return value;
    }
    std::uint32_t integer()
    {
        const auto value = take(4);
        std::uint32_t result = 0;
        for (int i = 0; i < 4; ++i) result |= static_cast<std::uint32_t>(static_cast<unsigned char>(value[i])) << (8 * i);
        return result;
    }
    std::string text(std::size_t limit)
    {
        const auto size = integer();
        if (size > limit) throw std::runtime_error("Level manifest text exceeds limit");
        return std::string(take(size));
    }
};
LevelDefinition read(const std::filesystem::path& path, bool draft)
{
    if (std::filesystem::is_symlink(path) || !std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("Level manifest is not a regular file");
    }
    const auto size = std::filesystem::file_size(path);
    if (size < 36 || size > MaxManifestBytes) throw std::runtime_error("Invalid level manifest size");
    std::ifstream input(path, std::ios::binary);
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) || input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Cannot read level manifest");
    }
    Reader reader{std::string_view(bytes).substr(0, bytes.size() - 4)};
    if (reader.take(Magic.size()) != std::string_view(Magic.data(), Magic.size())) throw std::runtime_error("Invalid level signature");
    Reader trailer{std::string_view(bytes).substr(bytes.size() - 4)};
    if (trailer.integer() != checksum(std::string_view(bytes).substr(Magic.size(), bytes.size() - Magic.size() - 4))) {
        throw std::runtime_error("Level manifest checksum mismatch");
    }
    if (reader.integer() != Version) throw std::runtime_error("Unsupported level manifest version");
    if (reader.integer() != (draft ? 1u : 0u)) throw std::runtime_error("Expected a different level manifest type");
    LevelDefinition definition;
    definition.world = static_cast<SectionWorld>(reader.integer());
    const auto number = reader.integer();
    if (number < 1 || number > static_cast<std::uint32_t>(LevelMaxNumber)) throw std::runtime_error("Invalid level number");
    definition.number = static_cast<int>(number);
    definition.name = reader.text(MaxName);
    const auto count = reader.integer();
    if (count > LevelMaxSections) throw std::runtime_error("Too many sections in level manifest");
    definition.sections.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) definition.sections.emplace_back(reader.text(MaxPath));
    if (reader.cursor != reader.bytes.size()) throw std::runtime_error("Unexpected trailing level data");
    validateFields(definition, draft);
    return definition;
}

std::string filename(int number)
{
    std::ostringstream name;
    name << "level-" << std::setw(3) << std::setfill('0') << number << ".vlevel";
    return name.str();
}
std::filesystem::path directory(const std::filesystem::path& root, std::string_view child)
{
    if (root.empty()) throw std::runtime_error("Level library needs a directory");
    std::filesystem::create_directories(root);
    const auto path = root / child;
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path))) throw std::runtime_error("Level library folders cannot be symbolic links");
    std::filesystem::create_directory(path);
    if (!std::filesystem::is_directory(path)) throw std::runtime_error("Level library path is not a directory");
    return path;
}
void atomicWrite(std::string_view bytes, const std::filesystem::path& path, bool overwrite)
{
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path))) throw std::runtime_error("Level destination cannot be a symbolic link");
    const auto pattern = (path.parent_path() / ".level-save-XXXXXX").string();
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back('\0');
    int descriptor = ::mkstemp(temporary.data());
    if (descriptor < 0) throw std::system_error(errno, std::generic_category(), "Cannot create temporary level");
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
            if (written < 0) {
                if (errno == EINTR) continue;
                throw std::system_error(errno, std::generic_category(), "Cannot write level");
            }
            if (written == 0) throw std::runtime_error("Level write made no progress");
            offset += static_cast<std::size_t>(written);
        }
        if (::fsync(descriptor) != 0) throw std::system_error(errno, std::generic_category(), "Cannot flush level");
        const int result = ::close(descriptor);
        descriptor = -1;
        if (result != 0) throw std::system_error(errno, std::generic_category(), "Cannot close level");
        if (overwrite) {
            std::filesystem::rename(temporary.data(), path);
        } else {
            if (::link(temporary.data(), path.c_str()) != 0) throw std::system_error(errno, std::generic_category(), "Level number already exists or cannot be saved");
            ::unlink(temporary.data());
        }
    } catch (...) {
        if (descriptor >= 0) ::close(descriptor);
        ::unlink(temporary.data());
        throw;
    }
}

} // namespace

void validateLevel(const LevelDefinition& definition, const std::filesystem::path& sectionRoot)
{
    validateFields(definition, false);
    validateReferences(definition, sectionRoot);
}
SectionDocument resolveLevelSection(const LevelDefinition& definition, std::size_t index,
                                    const std::filesystem::path& sectionRoot)
{
    validateFields(definition, false);
    if (index >= definition.sections.size()) throw std::runtime_error("Level section index is out of range");
    return readReference(definition, definition.sections[index], sectionRoot);
}
std::filesystem::path saveLevel(const LevelDefinition& definition, const std::filesystem::path& sectionRoot,
                                const std::filesystem::path& levelRoot, bool overwrite)
{
    validateLevel(definition, sectionRoot);
    const auto bytes = encode(definition, false);
    const auto path = directory(levelRoot, worldInfo(definition.world).folder) / filename(definition.number);
    atomicWrite(bytes, path, overwrite);
    return path;
}
LevelDefinition loadLevel(const std::filesystem::path& path) { return read(path, false); }

std::vector<LevelEntry> listLevels(const std::filesystem::path& levelRoot, std::optional<SectionWorld> filter)
{
    std::vector<LevelEntry> result;
    for (const auto& world : sectionWorldCatalog()) {
        if (filter && world.id != *filter) continue;
        const auto folder = levelRoot / world.folder;
        if (std::filesystem::is_symlink(folder) || !std::filesystem::is_directory(folder)) continue;
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.is_symlink() || !entry.is_regular_file() || entry.path().extension() != ".vlevel") continue;
            try {
                auto definition = loadLevel(entry.path());
                if (definition.world != world.id || entry.path().filename() != filename(definition.number)) continue;
                result.push_back({entry.path(), std::move(definition)});
            } catch (const std::exception&) {
                // One broken manifest cannot make the remaining levels disappear.
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.definition.world != b.definition.world) return a.definition.world < b.definition.world;
        return a.definition.number < b.definition.number;
    });
    return result;
}

std::filesystem::path saveLevelDraft(const LevelDefinition& definition, const std::filesystem::path& sectionRoot,
                                     const std::filesystem::path& levelRoot)
{
    validateFields(definition, true);
    // Autosave runs while typing and must also preserve repairable references
    // if a section has moved. Resolve snapshots only when publishing or playing.
    static_cast<void>(sectionRoot);
    const auto bytes = encode(definition, true);
    const auto path = directory(levelRoot, "drafts") / (std::string(worldInfo(definition.world).folder) + ".vlevel");
    atomicWrite(bytes, path, true);
    return path;
}
std::optional<LevelDefinition> loadLevelDraft(const std::filesystem::path& levelRoot, SectionWorld world)
{
    const auto folder = levelRoot / "drafts";
    const auto path = folder / (std::string(worldInfo(world).folder) + ".vlevel");
    if (std::filesystem::is_symlink(folder)) throw std::runtime_error("Level draft directory cannot be a symbolic link");
    if (!std::filesystem::exists(std::filesystem::symlink_status(path))) return std::nullopt;
    auto definition = read(path, true);
    if (definition.world != world) throw std::runtime_error("Level draft does not match the selected world");
    return definition;
}

} // namespace voxel
