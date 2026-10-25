#include "asset_collision.h"
#include "game.h"
#include "graphics/jungle_shapes.h"
#include "input.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
using namespace voxel;
int failures = 0;

void check(bool condition, const std::string& description)
{
    if (!condition) { std::cerr << description << '\n'; ++failures; }
}

struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory()
    {
        char pattern[] = "/tmp/voxel-jungle-assets-XXXXXX";
        if (!::mkdtemp(pattern)) throw std::runtime_error("Cannot create jungle asset test directory");
        path = pattern;
    }
    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

constexpr std::array Kinds{
    HubAssetKind::JungleMonstera, HubAssetKind::JungleCycad, HubAssetKind::BambooClump,
    HubAssetKind::FiddleheadFerns, HubAssetKind::HangingFern, HubAssetKind::MossDrape,
    HubAssetKind::PitcherPlants, HubAssetKind::SeedPodCluster, HubAssetKind::RootBridge,
    HubAssetKind::RootStump, HubAssetKind::MossyPlatform, HubAssetKind::MossyColumn,
    HubAssetKind::OvergrownArch, HubAssetKind::RuinedPillar, HubAssetKind::RuinedWall,
    HubAssetKind::RuinSlab
};

struct Bounds { glm::vec3 low, high; };
Bounds bounds(const MeshData& mesh)
{
    Bounds result{glm::vec3(std::numeric_limits<float>::max()),
                  glm::vec3(std::numeric_limits<float>::lowest())};
    for (const auto& vertex : mesh.vertices) {
        result.low = glm::min(result.low, vertex.position);
        result.high = glm::max(result.high, vertex.position);
    }
    return result;
}

bool sameMesh(const MeshData& a, const MeshData& b)
{
    if (a.indices != b.indices || a.vertices.size() != b.vertices.size()) return false;
    for (std::size_t i = 0; i < a.vertices.size(); ++i) {
        const auto& x = a.vertices[i];
        const auto& y = b.vertices[i];
        if (x.position != y.position || x.normal != y.normal || x.color != y.color
            || x.material != y.material || x.relief != y.relief) return false;
    }
    return true;
}

glm::vec3 rotate(glm::vec3 p, int facing)
{
    switch (facing) {
    case 1: return {p.z, p.y, -p.x};
    case 2: return {-p.x, p.y, -p.z};
    case 3: return {-p.z, p.y, p.x};
    default: return p;
    }
}

void leafVolume()
{
    // Check the common leaf shape directly: a crossed or double-sided card can
    // have a broad bounding box while still having no thickness or closed rim.
    MeshData mesh;
    const HubAsset asset{HubAssetKind::JungleMonstera, {0,0,0}};
    AssetBuilder builder(mesh, asset);
    jungle::leaf(builder, {0,0,0}, {0,.2f,1}, .4f, {.3f,.6f,.2f});
    using Point = std::array<int,3>;
    const auto point = [](glm::vec3 p) -> Point {
        return {int(std::lround(p.x * 1000000)), int(std::lround(p.y * 1000000)),
                int(std::lround(p.z * 1000000))};
    };
    std::map<std::array<Point,2>, int> edges;
    std::set<std::array<Point,3>> triangles;
    double volume = 0;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto a = mesh.vertices[mesh.indices[i]].position;
        const auto b = mesh.vertices[mesh.indices[i+1]].position;
        const auto c = mesh.vertices[mesh.indices[i+2]].position;
        std::array triangle{point(a), point(b), point(c)};
        for (int j = 0; j < 3; ++j) {
            std::array edge{triangle[j], triangle[(j+1)%3]};
            std::sort(edge.begin(), edge.end());
            ++edges[edge];
        }
        std::sort(triangle.begin(), triangle.end());
        check(triangles.insert(triangle).second,
              "the jungle leaf uses separate front/back surfaces, not duplicated two-sided triangles");
        volume += double(glm::dot(a, glm::cross(b,c))) / 6.0;
    }
    check(!edges.empty() && std::all_of(edges.begin(), edges.end(), [](const auto& edge) {
        return edge.second == 2;
    }), "every edge of a jungle leaf joins two faces, including its thin rim and tip");
    check(std::abs(volume) > .0001,
          "jungle leaves enclose a measurable three-dimensional volume");
}

void modelGeometry()
{
    for (const auto kind : Kinds) {
        const auto name = std::string(sectionAssetCatalog()[std::size_t(kind)].name);
        const auto original = buildHubAsset({kind, {0,0,0}, 0, 0});
        check(sameMesh(original, buildHubAsset({kind, {0,0,0}, 0, 0})), name + " is deterministic");
        bool varied = false;
        for (const auto seed : {0u, 1u, 17u, 93u, std::numeric_limits<std::uint32_t>::max()}) {
            const auto mesh = buildHubAsset({kind, {0,0,0}, 0, seed});
            const auto box = bounds(mesh);
            varied |= !sameMesh(original, mesh);
            check(mesh.indices.size() >= 36 && mesh.indices.size() <= 18000,
                  name + " has modeled detail within a practical scenery triangle budget");
            check(box.high.x - box.low.x > .1f && box.high.y - box.low.y > .1f
                  && box.high.z - box.low.z > .1f,
                  name + " has a three-dimensional silhouette");
            for (int axis = 0; axis < 3; ++axis)
                check(box.low[axis] >= -HubAssetMargin && box.high[axis] <= HubAssetMargin,
                      name + " stays inside the asset culling/collision margin for every variation");
            if (kind == HubAssetKind::HangingFern)
                check(std::abs(box.high.y) < .0001f && box.low.y < -.5f,
                      "hanging ferns meet the ceiling and grow down into the room");
            else if (kind == HubAssetKind::MossDrape)
                check(std::abs(box.low.z) < .0001f && box.high.z > .1f,
                      "moss drapes meet their supporting wall and have raised depth");
            else
                check(std::abs(box.low.y) < .0001f,
                      name + " rests exactly on its placement floor");
        }
        check(varied, name + " has a visible variation beyond its placement and facing");
    }
    std::vector<HubAsset> placements;
    for (const auto kind : Kinds) placements.push_back({kind, {0,0,0}});
    check(buildAssetDressing(placements).lights.empty(),
          "the overgrown kit does not introduce invisible light sources into dark caves");
}

void walkableShapes()
{
    struct Platform { HubAssetKind kind; float height; };
    constexpr std::array platforms{
        Platform{HubAssetKind::RootBridge,.55f}, Platform{HubAssetKind::MossyPlatform,.60f},
        Platform{HubAssetKind::MossyColumn,2.f}, Platform{HubAssetKind::RuinSlab,.26f}
    };
    const glm::vec3 origin{-12.5f, 15.25f, 18.5f};
    for (int facing = 0; facing < 4; ++facing) {
        for (const auto seed : {0u, 17u}) {
            for (const auto platform : platforms) {
                const HubAsset asset{platform.kind, origin, facing, seed};
                AssetCollision collision;
                collision.rebuild(std::span(&asset,1));
                const auto hit = collision.raycast(origin + glm::vec3(0,4,0), {0,-1,0}, 5);
                check(hit && std::abs(hit->point.y - origin.y - platform.height) < .001f
                      && hit->normal.y > .99f,
                      "parkour assets have their visible flat landing surface in every rotation and variant");
                const auto low = origin + glm::vec3(-.3f,platform.height + 1,-.3f);
                const auto high = low + glm::vec3(.6f,1.8f,.6f);
                const auto drop = collision.sweep(low,high,1,-2);
                check(std::abs(drop + 1) < .001f,
                      "a full-width player lands on the parkour model's visible top");
                const auto landed = low + glm::vec3(0,drop,0);
                check(!collision.overlaps(landed, high + glm::vec3(0,drop,0)),
                      "landing on a parkour asset does not penetrate its decoration");
            }
            const HubAsset arch{HubAssetKind::OvergrownArch, origin, facing, seed};
            AssetCollision collision;
            collision.rebuild(std::span(&arch,1));
            const auto a = origin + rotate({-.3f,.02f,-2},facing);
            const auto b = origin + rotate({.3f,1.82f,-1.4f},facing);
            const auto forward = rotate({0,0,1},facing);
            const auto axis = facing % 2 == 0 ? 2 : 0;
            const float travel = 4 * forward[axis];
            check(std::abs(collision.sweep(glm::min(a,b),glm::max(a,b),axis,travel) - travel) < .001f,
                  "the overgrown arch preserves full player clearance instead of blocking the path");
            check(collision.raycast(origin + rotate({1.25f,1,-2},facing),forward,4).has_value(),
                  "the arch's solid masonry beside the opening remains collidable");
        }
    }
}

void click(Game& game, int id)
{
    InputActions action;
    action.active = true;
    action.devClick = id;
    game.update(action);
}

void press(Game& game, bool InputActions::* key)
{
    InputActions action;
    action.active = true;
    action.*key = true;
    game.update(action);
}

void aim(Camera& camera, glm::vec3 target)
{
    const auto direction = glm::normalize(target-camera.position());
    const auto old = camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                  glm::degrees(std::asin(direction.y)-std::asin(old.y)));
}

void builderAttachments(Game& game)
{
    auto& editor = game.sectionEditor();
    struct Placement {
        HubAssetKind kind;
        glm::vec3 camera, target;
        int rotation, expectedFacing, surfaceAxis;
        bool minimum;
        float surface;
    };
    const std::array cases{
        Placement{HubAssetKind::MossyPlatform,{12.5f,15,12.5f},{12.5f,8.5f,12.5f},0,0,1,true,9},
        Placement{HubAssetKind::MossyPlatform,{12.5f,15,12.5f},{12.5f,8.5f,12.5f},1,1,1,true,9},
        Placement{HubAssetKind::HangingFern,{2.5f,52,2.5f},{2.5f,57.5f,2.5f},0,0,1,false,57},
        Placement{HubAssetKind::HangingFern,{2.5f,52,2.5f},{2.5f,57.5f,2.5f},1,1,1,false,57},
        Placement{HubAssetKind::MossDrape,{-26,18.5f,6.5f},{-32.5f,18.5f,6.5f},0,1,0,true,-32},
        Placement{HubAssetKind::MossDrape,{26,18.5f,6.5f},{32.5f,18.5f,6.5f},0,3,0,false,32}
    };
    for (const auto& placement : cases) {
        if (editor.screen() != SectionEditorScreen::Palette) press(game,&InputActions::devPalette);
        const auto& items = editor.paletteItems();
        const auto found = std::find_if(items.begin(),items.end(),[&](const auto& item) {
            return item.asset == placement.kind;
        });
        if (found == items.end()) { check(false,"attachment test asset is in the palette"); continue; }
        click(game,SectionUiPaletteItemBase+int(found-items.begin()));
        press(game,&InputActions::devPalette);
        for (int turns = 0; editor.facing() != placement.rotation && turns < 4; ++turns)
            press(game,&InputActions::devRotate);
        game.camera.setPosition(placement.camera);
        aim(game.camera,placement.target);
        InputActions refresh;
        refresh.active = true;
        game.update(refresh);
        const auto preview = editor.placementPreview();
        check(preview && preview->kind == placement.kind && preview->facing == placement.expectedFacing,
              "floor, ceiling and wall jungle models produce correctly oriented creative previews");
        if (!preview) continue;
        const auto box = bounds(buildHubAsset(*preview));
        const auto face = placement.minimum ? box.low[placement.surfaceAxis] : box.high[placement.surfaceAxis];
        check(std::abs(face-placement.surface) < .002f,
              "the actual jungle mesh meets the clicked block face without hovering or sinking");
        const auto before = editor.assets().size();
        press(game,&InputActions::place);
        check(editor.assets().size() == before+1 && editor.assets().back() == *preview,
              "placing a jungle model keeps the preview geometry and attachment");
        if (editor.assets().size() == before+1) {
            press(game,&InputActions::undo);
            check(editor.assets().size() == before,"undo removes the complete newly attached jungle model");
        }
    }
}

void catalogAndSave(const TemporaryDirectory& temp)
{
    const auto catalog = sectionAssetCatalog();
    check(catalog.size() == HubAssetCount, "the saved asset ID table includes the whole jungle kit");
    std::set<std::string_view> names;
    for (std::size_t i = 0; i < Kinds.size(); ++i) {
        const auto kind = Kinds[i];
        check(std::size_t(kind) == std::size_t(HubAssetKind::TrialSpawner) + i + 1,
              "new scenery appends IDs without changing older saved assets");
        const auto& entry = catalog[std::size_t(kind)];
        check(entry.kind == kind && !entry.name.empty() && !entry.group.empty()
              && names.insert(entry.name).second && isCreativeAsset(kind),
              "each jungle asset has its own named creative catalog entry");
    }

    Options options;
    options.dev = true;
    options.sectionLibrary = (temp.path / "library").string();
    Game game(options);
    auto& editor = game.sectionEditor();
    click(game, SectionUiWorldBase);
    click(game, SectionUiLibraryNew);
    click(game, SectionUiPresetBase + 1);
    click(game, SectionUiCreate);
    InputActions palette;
    palette.active = true;
    palette.devPalette = true;
    game.update(palette);
    click(game, SectionUiPaletteTabBase + 1);
    check(editor.screen() == SectionEditorScreen::Palette && editor.paletteTab() == 1,
          "the creative Assets menu is reachable in a lush section");
    for (const auto kind : Kinds) {
        const auto& items = editor.paletteItems();
        const auto found = std::find_if(items.begin(),items.end(),[&](const auto& item) {
            return item.asset == kind;
        });
        check(found != items.end(), "every jungle model is exposed through the actual creative palette");
        if (found == items.end()) continue;
        click(game, SectionUiPaletteItemBase + int(found - items.begin()));
        check(editor.hotbar()[editor.selectedSlot()].asset == kind,
              "selecting a jungle palette entry equips the matching model");
    }
    builderAttachments(game);

    std::vector<HubAsset> placements;
    for (std::size_t i = 0; i < Kinds.size(); ++i) {
        for (int facing = 0; facing < 4; ++facing)
            placements.push_back({Kinds[i], {-24.f + float(i%4)*15.f, 15.f + facing*8.f,
                                  -24.f + float(i/4)*15.f}, facing, std::uint32_t(93 + i)});
    }
    SectionMetadata metadata;
    metadata.name = "Overgrown asset roundtrip";
    const auto path = temp.path / "jungle.vxs";
    saveSection(game.world, metadata, placements, path);
    const auto loaded = loadSection(path);
    check(loaded.assets == placements && loaded.meta.name == metadata.name
          && loaded.meta.world == SectionWorld::LushCaves && loaded.world.sameContents(game.world),
          "section files preserve every new asset kind, rotation and variation without shifting old IDs");
}
} // namespace

int main()
{
    try {
        TemporaryDirectory temp;
        leafVolume();
        modelGeometry();
        walkableShapes();
        catalogAndSave(temp);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return failures ? 1 : 0;
}
