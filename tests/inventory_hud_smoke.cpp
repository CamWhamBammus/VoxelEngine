#include "inventory_hud_smoke.h"
#include "graphics/hud.h"
#include "game.h"
#include "input.h"
#include "sections.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {
void require(bool condition,const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
struct State {
    std::array<GLint,9> bindings{};
    std::array<GLint,4> viewport{};
    std::array<GLboolean,5> enabled{};
    bool operator==(const State&) const = default;
};
State state()
{
    State result;
    constexpr std::array<GLenum,9> names{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,
        GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,
        GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA};
    for (std::size_t i=0;i<names.size();++i) glGetIntegerv(names[i],&result.bindings[i]);
    glGetIntegerv(GL_VIEWPORT,result.viewport.data());
    result.enabled={glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),glIsEnabled(GL_BLEND),
                    glIsEnabled(GL_SCISSOR_TEST),GL_FALSE};
    glGetBooleanv(GL_DEPTH_WRITEMASK,&result.enabled[4]);
    return result;
}
using Pixels=std::vector<unsigned char>;
std::size_t difference(const Pixels& a,const Pixels& b)
{
    require(a.size()==b.size(),"Inventory HUD image dimensions differ");
    std::size_t count=0;
    for (std::size_t i=0;i<a.size();i+=4)
        count+=std::max({std::abs(int(a[i])-int(b[i])),std::abs(int(a[i+1])-int(b[i+1])),
                        std::abs(int(a[i+2])-int(b[i+2]))})>20;
    return count;
}
}

void verifyInventoryHud(int width,int height)
{
    width=std::min(width,1280); height=std::min(height,720);
    const auto temporary=std::filesystem::temp_directory_path()/
        ("voxel-inventory-hud-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    World world;
    buildBlankSection(world);
    std::vector<ItemStack> authoredLoot;
    for (int slot=0;slot<ChestSlotCount;++slot)
        authoredLoot.push_back({static_cast<ItemId>(slot%ItemCount),3+slot});
    const std::vector<HubAsset> assets{
        {HubAssetKind::LootChest,{.5f,9,21.5f},0,0,authoredLoot},
        {HubAssetKind::ItemPickup,{4.5f,9,24.5f},0,0,{},{ItemId::Key,1}}
    };
    saveSectionDraft(world,{},assets,temporary);
    Options options; options.dev=true; options.smokeTest=true; options.sectionLibrary=temporary.string();
    Game game(options);
    Hud hud;
    const auto press=[&](bool InputActions::*key) {
        InputActions action; action.active=true; action.*key=true; game.update(action);
    };
    const auto click=[&](int id) {
        InputActions action; action.active=true; action.devClick=id; game.update(action);
    };
    const auto cleanup=[&] {
        glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glBlendEquation(GL_FUNC_ADD);
        std::error_code error; std::filesystem::remove_all(temporary,error);
    };
    try {
        click(SectionUiWorldBase); click(SectionUiLibraryDraftBase);
        press(&InputActions::devTest);
        require(game.sectionEditor().screen()==SectionEditorScreen::Test,"Inventory fixture cannot start playtest");
        game.equipment.inventory={};
        for (const auto& item:itemCatalog()) game.equipment.inventory.add(item.id,3+static_cast<int>(item.id)*4);
        const auto draw=[&](int w,int h,float scale=1.0f) {
            glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,w,h);
            glDisable(GL_SCISSOR_TEST); glClearColor(.13f,.17f,.15f,1); glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glEnable(GL_BLEND); glDepthMask(GL_TRUE);
            glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
            glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
            glEnable(GL_SCISSOR_TEST); glScissor(3,5,1,1);
            const auto before=state();
            hud.drawInventory(game,w,h,scale);
            require(state()==before,"Inventory HUD changed OpenGL state");
            Pixels pixels(static_cast<std::size_t>(w)*h*4);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            require(glGetError()==GL_NO_ERROR,"Inventory HUD generated an OpenGL error");
            return pixels;
        };
        const auto closed=draw(width,height);
        press(&InputActions::inventory);
        require(game.inventoryOpen(),"I action did not open inventory during F8 playtest");
        const auto opened=draw(width,height);
        require(difference(closed,opened)>10000,"Inventory did not open visibly");
        const auto verifyHits=[&](int w,int h,float dpi,bool chest) {
            const float scale=std::min({dpi,w/784.0f,h/530.0f});
            const float x=(w/scale-(chest ? 748 : 396))*.5f,y=(h/scale-482)*.5f;
            const auto hit=[&](float px,float py) { return Hud::inventoryClick(game,w,h,dpi,px*scale,py*scale); };
            for (int slot=0;slot<20;++slot)
                require(hit(x+54+(slot%5)*68,y+122+(slot/5)*68)==InventoryUiSlotBase+slot,
                        "Inventory slot hit area disagrees with DPI layout");
            require(hit(x+309,y+70)==InventoryUiSort,"Sort target disagrees with DPI layout");
            require(hit(x+(chest ? 748 : 396)-36,y+29)==InventoryUiClose,"Close target disagrees with DPI layout");
            if (chest) {
                for (int slot=0;slot<ChestSlotCount;++slot)
                    require(hit(x+396+72+(slot%2)*153,y+92+12+(slot/2)*26)==InventoryUiLootBase+slot,
                            "Every authored chest stack needs a visible target at normal and high DPI");
                require(hit(x+545,y+387)==InventoryUiTakeAll,"Take All target disagrees with DPI layout");
            }
            require(!hit(x-1,y-1),"Inventory accepts outside clicks");
            draw(w,h,dpi);
        };
        verifyHits(width,height,1,false); verifyHits(640,360,2,false);
        require(!Hud::inventoryClick(game,width,height,1,std::numeric_limits<float>::quiet_NaN(),0),
                "Inventory accepts invalid pointer position");
        InputActions select; select.active=true; select.inventoryClick=15; game.update(select);
        require(game.inventorySelection()==15,"Inventory selection did not inspect a slot");
        require(difference(opened,draw(width,height))>300,"Selected ore detail is not visible");
        press(&InputActions::inventorySort);
        require(game.inventorySelection()==-1,"Sort did not clear selection");
        press(&InputActions::closeMenu);
        require(!game.inventoryOpen(),"Escape action did not close inventory");

        const auto target=assets.front().position+glm::vec3(0,.5f,0);
        const auto direction=glm::normalize(target-game.camera.position()),old=game.camera.forward();
        game.camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                           glm::degrees(std::asin(direction.y)-std::asin(old.y)));
        press(&InputActions::interact);
        require(game.chestOpen(),"E action did not open test chest");
        require(game.chestContents()==authoredLoot,"Playable chest did not preserve the authored 20 stacks");
        verifyHits(width,height,1,true); verifyHits(640,360,2,true);
        const auto lastItem=authoredLoot.back().item;
        const int beforeCount=game.equipment.inventory.count(lastItem);
        InputActions takeLast; takeLast.active=true; takeLast.lootClick=ChestSlotCount-1;game.update(takeLast);
        require(game.equipment.inventory.count(lastItem)==beforeCount+authoredLoot.back().count
                && game.chestContents().back().count==0,"The twentieth loot stack cannot be taken");
        const auto chest=draw(width,height);
        // Fill every slot, making the full-pack message and retained loot visible.
        for (auto& stack:game.equipment.inventory.slots) stack={ItemId::Coal,99};
        const auto contents=game.chestContents();
        press(&InputActions::lootAll);
        require(game.chestContents()==contents,"Full pack changed chest contents");
        require(difference(chest,draw(width,height))>500,"Full inventory state did not change visibly");
        game.equipment.inventory={};
        press(&InputActions::lootAll);
        require(std::none_of(game.chestContents().begin(),game.chestContents().end(),
                            [](const auto& stack) { return stack.count>0; }),"Take All did not empty chest");
        require(difference(chest,draw(width,height))>1000,"Empty chest state did not change visibly");
        draw(640,360,2);
        press(&InputActions::closeMenu);
        require(game.player.setFeetPosition(game.world,assets.back().position+glm::vec3(0,0,3)),
                "Key prompt fixture needs an approach outside automatic pickup range");
        game.camera.setPosition(game.player.eyePosition());
        const auto keyDirection=glm::normalize(assets.back().position+PickupVisualCenter-game.camera.position());
        const auto oldDirection=game.camera.forward();
        game.camera.rotate(glm::degrees(std::atan2(keyDirection.z,keyDirection.x)-std::atan2(oldDirection.z,oldDirection.x)),
                           glm::degrees(std::asin(keyDirection.y)-std::asin(oldDirection.y)));
        InputActions active;active.active=true;game.update(active);
        require(game.lootPrompt().find("WALK OVER")!=std::string::npos
                && game.lootPrompt().ends_with("THIS SECTION ONLY"),
                "Dropped key prompt must explain automatic collection and its section-only lifetime");
        const auto keyPrompt=draw(640,360,2);
        // The final word must actually reach the framebuffer at small Retina size;
        // shortening this prompt would hide the key's section-only lifetime.
        constexpr float scale=360.0f/530.0f;
        const float font=1/scale,canvasWidth=640/scale;
        const float textWidth=(game.lootPrompt().size()*6-1)*font;
        const float left=(canvasWidth-textWidth)*.5f;
        const int startX=static_cast<int>(std::round((left+(game.lootPrompt().size()-4)*6*font)*scale));
        const int startY=static_cast<int>(std::round((530*.5f+30)*scale));
        int goldPixels=0;
        for (int y=startY;y<startY+7;++y) for (int x=startX;x<startX+23;++x) {
            const auto offset=(static_cast<std::size_t>(359-y)*640+x)*4;
            goldPixels+=keyPrompt[offset]>210 && keyPrompt[offset+1]>145 && keyPrompt[offset+2]<120;
        }
        require(goldPixels>20,"Small Retina pickup prompt clipped the final word of THIS SECTION ONLY");
        const auto before=state();
        hud.drawInventory(game,0,0);
        require(state()==before,"Minimized inventory altered OpenGL state");
    } catch (...) { cleanup(); throw; }
    cleanup();
    std::cout<<"Inventory HUD passed: 20 slots, ore/key icons, 20 authored chest stacks, full/empty states, DPI hit tests and GL state\n";
}
} // namespace voxel
