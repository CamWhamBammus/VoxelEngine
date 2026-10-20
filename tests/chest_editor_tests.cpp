#include "game.h"
#include "input.h"
#include "sections.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
void aim(voxel::Camera& camera,glm::vec3 point)
{
    const auto direction=glm::normalize(point-camera.position()),old=camera.forward();
    camera.rotate(glm::degrees(std::atan2(direction.z,direction.x)-std::atan2(old.z,old.x)),
                  glm::degrees(std::asin(direction.y)-std::asin(old.y)));
}
void press(voxel::Game& game,bool voxel::InputActions::*key)
{
    voxel::InputActions input;input.active=true;input.*key=true;game.update(input);
}
void click(voxel::Game& game,int id,bool shift=false)
{
    voxel::InputActions input;input.active=true;input.devClick=id;input.sprint=shift;game.update(input);
}
void type(voxel::Game& game,const std::string& text)
{
    voxel::InputActions input;input.active=true;input.devText=text;game.update(input);
}
}
int main()
{
    using namespace voxel;
    int failures=0;
    const auto check=[&](bool value,const char* message){if(!value){std::cerr<<message<<'\n';++failures;}};
    char path[]="/tmp/voxel-chest-editor-XXXXXX";
    if(!mkdtemp(path))return 1;
    const std::filesystem::path root(path);
    try {
        World world;buildBlankSection(world);
        const std::vector<HubAsset> models{{HubAssetKind::LootChest,{5.5f,9,14.5f},0,0},
            {HubAssetKind::CrystalChest,{10.5f,9,14.5f},1,0},
            {HubAssetKind::IronboundChest,{15.5f,9,14.5f},2,0}};
        const auto draft=saveSectionDraft(world,{},models,root);
        Options options;options.dev=true;options.sectionLibrary=root.string();
        Game game(options);auto& editor=game.sectionEditor();
        click(game,SectionUiWorldBase);click(game,SectionUiLibraryDraftBase);
        const auto open=[&](int index){
            game.camera.setPosition(models[index].position+glm::vec3(0,2,3));
            aim(game.camera,models[index].position+glm::vec3(0,.5f,0));
            press(game,&InputActions::devPalette);
            check(editor.screen()==SectionEditorScreen::ChestContents,"E at a chest opens contents authoring");
        };
        open(0);
        check(editor.typing() && editor.menuOpen() && !editor.flying(),"Chest contents owns typing and pauses creative flight");
        check(editor.assets()[0].chestItems.empty(),"Placed wooden chest starts empty");
        click(game,SectionUiChestItemBase+static_cast<int>(ItemId::CopperOre));
        type(game,"27");
        check(editor.chestSlots()[0]==ItemStack{ItemId::CopperOre,27},"Chosen ore and typed quantity update staged slot");
        check(editor.assets()[0].chestItems.empty() && !editor.dirty(),"Staging leaves section and saved contents untouched");
        for(auto key:{&InputActions::devTest,&InputActions::devAccept,&InputActions::devLibrary,&InputActions::save}) {
            press(game,key);
            check(editor.screen()==SectionEditorScreen::ChestContents && editor.assets()[0].chestItems.empty(),
                  "Test/library/save shortcuts cannot discard or apply pending chest edits");
        }
        click(game,SectionUiChestPlus,true);click(game,SectionUiChestMinus);
        check(editor.chestSlots()[0].count==36,"Quantity buttons support shift-ten and single-item changes");
        click(game,SectionUiChestQuantity);type(game,"0");click(game,SectionUiChestApply);
        check(editor.screen()==SectionEditorScreen::ChestContents && editor.assets()[0].chestItems.empty(),
              "Zero quantity blocks applying the staged chest");
        click(game,SectionUiChestQuantity);type(game,"999");
        check(editor.chestQuantityText()=="99" && editor.chestSlots()[0].count==99,"Typed quantity is bounded to two digits");
        click(game,SectionUiChestPlus,true);
        check(editor.chestSlots()[0].count==99,"Quantity cannot exceed stack limit");
        click(game,SectionUiChestSlotBase+1);
        click(game,SectionUiChestItemBase+static_cast<int>(ItemId::IronOre));type(game,"12");
        press(game,&InputActions::closeMenu);
        check(editor.screen()==SectionEditorScreen::Build && editor.assets()[0].chestItems.empty() && !editor.dirty(),
              "Escape cancels every staged edit without changing history");
        open(0);
        for(int slot=0;slot<ChestSlotCount;++slot) {
            click(game,SectionUiChestSlotBase+slot);
            click(game,SectionUiChestItemBase+(slot%ItemCount));
            type(game,std::to_string(slot+1));
        }
        click(game,SectionUiChestApply);
        check(editor.screen()==SectionEditorScreen::Build && editor.assets()[0].chestItems.size()==20 && editor.dirty(),
              "Apply records a full twenty-stack chest and returns to building");
        const auto filled=editor.assets()[0].chestItems;
        for(int slot=0;slot<static_cast<int>(filled.size());++slot)
            check(filled[slot]==ItemStack{static_cast<ItemId>(slot%ItemCount),slot+1},"Applied chest preserves exact item order and counts");
        check(editor.assets()[1].chestItems.empty() && editor.assets()[2].chestItems.empty(),"Other chest placements keep independent contents");
        press(game,&InputActions::undo);
        check(editor.assets()[0].chestItems.empty(),"One undo reverses the entire contents edit");
        press(game,&InputActions::redo);
        check(editor.assets()[0].chestItems==filled,"One redo restores all authored chest items");
        press(game,&InputActions::save);
        check(loadSection(draft).assets[0].chestItems==filled,"Draft persistence preserves authored items and quantities");
        open(0);click(game,SectionUiChestClear);click(game,SectionUiChestCancel);
        check(editor.assets()[0].chestItems==filled,"Cancelling Clear All keeps the original chest intact");
        open(0);click(game,SectionUiChestSlotBase+3);click(game,SectionUiChestRemove);click(game,SectionUiChestApply);
        check(editor.assets()[0].chestItems.size()==19 && editor.assets()[0].chestItems[3]==filled[4],
              "Removing a staged slot removes only that stack");
        open(1);
        check(editor.editingChestKind()==HubAssetKind::CrystalChest,"Crystal chest opens the same content authoring workflow");
        click(game,SectionUiChestItemBase+static_cast<int>(ItemId::GlowCrystal));type(game,"8");press(game,&InputActions::upgrade);
        check(editor.assets()[1].chestItems==std::vector<ItemStack>{{ItemId::GlowCrystal,8}},"Enter applies custom contents to a crystal chest");
        open(2);
        check(editor.editingChestKind()==HubAssetKind::IronboundChest,"Ironbound chest uses the authored-content workflow");
        click(game,SectionUiChestItemBase);type(game,"18");
        press(game,&InputActions::devToggle);
        check(!game.devMode(),"Leaving the section editor safely cancels a staged chest edit");
        check(loadSection(draft).assets[2].chestItems.empty(),"Leaving never saves an unconfirmed staged chest");
        press(game,&InputActions::devToggle);click(game,SectionUiWorldBase);click(game,SectionUiLibraryDraftBase);
        game.camera.setPosition({-10.5f,13,20.5f});aim(game.camera,{-10.5f,8.5f,15.5f});
        press(game,&InputActions::devPalette);
        check(editor.screen()==SectionEditorScreen::Palette,"E away from a chest still opens creative inventory");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';++failures;}
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
