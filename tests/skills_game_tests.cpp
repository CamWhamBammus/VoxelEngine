#include "game.h"
#include "heartwood.h"
#include "input.h"

#include <iostream>
#include <stdexcept>

using namespace voxel;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void press(Game& game, bool InputActions::*action)
{
    InputActions input; input.active = true; input.*action = true; game.update(input,.1f);
}
void position(Game& game, glm::vec3 feet)
{
    check(game.player.setFeetPosition(game.world,feet), "journal approach must fit the player");
    game.camera.setPosition(game.player.eyePosition());
}
}
int main()
{
    try {
        Game game(Options{});
        check(!game.skillsOpen() && !game.nearSkillTree(), "arrival is outside the journal interaction range");
        press(game,&InputActions::interact);
        check(!game.skillsOpen(), "E away from the tree cannot open the journal");
        position(game,heartwoodLayout().approach);
        check(game.nearSkillTree(), "the seed is reachable from its approach");
        const auto wall=glm::ivec3(glm::floor(glm::mix(game.player.eyePosition(),heartwoodLayout().station,.5f)));
        const auto before=game.world.get(wall);
        game.world.set(wall,Block::Stone);
        check(!game.nearSkillTree(), "a wall prevents using the seed through it");
        game.world.set(wall,before);
        const auto equipment=game.equipment;
        const auto standing=game.player.feetPosition();
        press(game,&InputActions::interact);
        check(game.skillsOpen() && game.menuOpen(), "E opens the skill journal");
        InputActions choose; choose.active=true; choose.skillTab=2; choose.skillSelection=4;
        choose.movement={1,1}; choose.jump=true; choose.selectedTool=3;
        choose.attack=choose.upgrade=choose.inventory=choose.devToggle=true;
        game.update(choose,.1f);
        check(game.skillJournal().tab==2 && game.skillJournal().selected==4, "tabs and skills can be selected");
        check(game.player.feetPosition()==standing && game.equipment==equipment,
              "preview selection and Enter never move the player or change equipment");
        check(!game.devMode() && !game.inventoryOpen(), "the journal owns modal actions");
        choose.active=false; choose.skillTab=0; choose.skillSelection=0;
        game.update(choose,.1f);
        check(game.skillJournal().tab==2 && game.skillJournal().selected==4, "unfocused input cannot change the page");
        InputActions keyboard; keyboard.active=true; keyboard.skillTabStep=1; keyboard.skillMove={1,1};
        game.update(keyboard,.1f);
        check(game.skillJournal().tab==3 && game.skillJournal().selected==4, "keyboard tabs and grid navigation work together");
        press(game,&InputActions::closeMenu);
        check(!game.skillsOpen() && !game.menuOpen(), "Escape closes the book");
        press(game,&InputActions::interact);
        check(game.skillsOpen() && game.skillJournal().tab==3, "reopening keeps the current page");
        press(game,&InputActions::interact);
        check(!game.skillsOpen(), "E also closes the book");
        press(game,&InputActions::interact);
        position(game,heartwoodLayout().approach+glm::vec3(0,0,10));
        InputActions tick; tick.active=true; game.update(tick);
        check(!game.skillsOpen(), "moving away externally closes the journal");
        check(game.equipment==equipment, "browsing all placeholder content leaves the profile unchanged");
        std::cout << "Skill journal game checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
