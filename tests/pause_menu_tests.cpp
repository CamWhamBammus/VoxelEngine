#include "pause_menu.h"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    using namespace voxel;
    int failures = 0;
    const auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; ++failures; }
    };
    const GraphicsSettings defaults;
    PauseMenu menu;
    check(menu.screen() == PauseScreen::Closed && menu.settings() == defaults
          && menu.draft() == defaults && !menu.dirty(), "The pause menu starts closed with the active settings");
    for (const int action : {PauseUiResume, PauseUiGraphics, PauseUiQuit, PauseUiBack, PauseUiApply,
                            PauseUiDefaults, PauseUiIncreaseBase, PauseUiRowBase}) {
        check(menu.dispatch(action) == PauseCommand::None && menu.screen() == PauseScreen::Closed,
              "A closed menu cannot run stale pointer actions");
    }
    menu.moveSelection(1);
    menu.changeSelected(-1);
    check(menu.activateSelected() == PauseCommand::None && menu.selected() == 0 && !menu.dirty(),
          "Navigation cannot mutate a closed menu");
    menu.open();
    check(menu.screen() == PauseScreen::Home && menu.selected() == 0, "Opening selects Resume");
    menu.moveSelection(-1);
    check(menu.selected() == 2 && menu.activateSelected() == PauseCommand::Quit,
          "Keyboard navigation wraps to Quit and requests it without closing directly");
    check(menu.screen() == PauseScreen::Home, "The caller handles quitting and any save prompt");
    menu.moveSelection(1);
    check(menu.selected() == 0, "Home navigation wraps back to Resume");
    menu.moveSelection(1);
    check(menu.activateSelected() == PauseCommand::None && menu.screen() == PauseScreen::Graphics
          && menu.selected() == 0, "Enter on Graphics opens the quality page");
    menu.changeSelected(1);
    const auto lowDraft = menu.draft();
    check(graphicsPreset(lowDraft) == GraphicsPreset::Low && menu.dirty() && menu.settings() == defaults,
          "Adjusting a quality preset changes only the draft");
    menu.open();
    check(menu.screen() == PauseScreen::Graphics && menu.draft() == lowDraft,
          "Repeated pause requests do not discard an open page's draft");
    check(menu.dispatch(PauseUiApply) == PauseCommand::Apply && menu.settings() == defaults
          && menu.draft() == lowDraft, "Apply requests work from the caller before committing menu state");
    menu.completeApply(lowDraft, "Saved");
    check(menu.settings() == lowDraft && menu.draft() == lowDraft && !menu.dirty()
          && menu.message() == "Saved" && menu.screen() == PauseScreen::Graphics,
          "Completed Apply commits the reported runtime settings and keeps the page open");
    menu.dispatch(PauseUiIncreaseBase + 2);
    check(menu.dirty() && menu.message().empty(), "Changing a value clears feedback from the previous Apply");
    check(menu.dispatch(PauseUiBack) == PauseCommand::None && menu.screen() == PauseScreen::Home
          && menu.selected() == 1 && menu.settings() == lowDraft && menu.draft() == lowDraft && !menu.dirty(),
          "Back discards unapplied changes and returns selection to Graphics");
    menu.dispatch(PauseUiGraphics);
    menu.dispatch(PauseUiIncreaseBase + 1);
    check(menu.dispatch(PauseUiResume) == PauseCommand::Resume && menu.screen() == PauseScreen::Closed
          && menu.draft() == lowDraft && menu.settings() == lowDraft && menu.message().empty(),
          "Resume from Graphics discards the draft and preserves applied settings");
    menu.open();
    check(menu.activateSelected() == PauseCommand::Resume && menu.screen() == PauseScreen::Closed,
          "Enter on Resume requests return to gameplay");

    // Independent expectations for each user-visible setting in both directions.
    std::array<GraphicsSettings, 8> next, previous;
    next.fill(defaults);
    previous.fill(defaults);
    applyGraphicsPreset(next[0], GraphicsPreset::Low);
    applyGraphicsPreset(previous[0], GraphicsPreset::Medium);
    next[1].renderScale = .5f; previous[1].renderScale = .85f;
    next[2].msaa = 0; previous[2].msaa = 2;
    next[3].lighting = LightingQuality::Low; previous[3].lighting = LightingQuality::Medium;
    next[4].detail = DetailDistance::Near; previous[4].detail = DetailDistance::Medium;
    next[5].fullscreen = previous[5].fullscreen = true;
    next[6].vsync = previous[6].vsync = false;
    next[7].fpsLimit = 30; previous[7].fpsLimit = 144;
    for (int row = 0; row < 8; ++row) {
        for (const int direction : {-1, 1}) {
            const auto& expected = direction > 0 ? next[row] : previous[row];
            PauseMenu keyboard;
            keyboard.openGraphics();
            for (int i = 0; i < row; ++i) keyboard.moveSelection(1);
            keyboard.changeSelected(direction);
            check(keyboard.selected() == row && keyboard.draft() == expected && keyboard.settings() == defaults,
                  "Keyboard arrows adjust the chosen row without changing another setting");
            PauseMenu pointer;
            pointer.openGraphics();
            pointer.dispatch(PauseUiRowBase + row);
            check(pointer.selected() == row && pointer.draft() == defaults,
                  "Clicking a setting label selects it without changing its value");
            check(pointer.dispatch((direction > 0 ? PauseUiIncreaseBase : PauseUiDecreaseBase) + row)
                      == PauseCommand::None && pointer.draft() == expected,
                  "Pointer arrows and keyboard arrows choose the same values");
        }
        PauseMenu enter;
        enter.openGraphics();
        enter.dispatch(PauseUiRowBase + row);
        check(enter.activateSelected() == PauseCommand::None && enter.draft() == next[row],
              "Enter advances every editable setting");
    }

    // A custom CLI scale should move to the adjacent choice instead of jumping
    // backwards or resetting unrelated fields when the graphics page opens.
    auto custom = defaults;
    custom.renderScale = .8f;
    custom.fullscreen = true;
    custom.vsync = false;
    custom.fpsLimit = 120;
    for (const int direction : {-1, 1}) {
        PauseMenu customMenu(custom);
        customMenu.openGraphics();
        check(customMenu.draft() == custom && graphicsPreset(customMenu.draft()) == GraphicsPreset::Custom,
              "Opening Graphics preserves custom launch settings");
        customMenu.dispatch(PauseUiRowBase + 1);
        customMenu.changeSelected(direction);
        check(customMenu.draft().renderScale == (direction > 0 ? .85f : .75f),
              "Custom render scales advance to the nearest choice in the requested direction");
        customMenu.openGraphics();
        customMenu.changeSelected(direction);
        check(graphicsPreset(customMenu.draft()) == (direction > 0 ? GraphicsPreset::Low : GraphicsPreset::High)
              && customMenu.draft().fullscreen && !customMenu.draft().vsync && customMenu.draft().fpsLimit == 120,
              "Cycling away from Custom chooses a quality preset and preserves display choices");
    }

    PauseMenu buttons(custom);
    buttons.openGraphics();
    buttons.moveSelection(-1);
    check(buttons.selected() == 10 && buttons.activateSelected() == PauseCommand::Apply,
          "Keyboard navigation reaches Apply by wrapping upwards");
    buttons.moveSelection(-1);
    check(buttons.selected() == 9 && buttons.activateSelected() == PauseCommand::None
          && buttons.screen() == PauseScreen::Home, "Keyboard navigation reaches Back");
    buttons.openGraphics();
    buttons.moveSelection(-1);
    buttons.moveSelection(-1);
    buttons.moveSelection(-1);
    check(buttons.selected() == 8 && buttons.activateSelected() == PauseCommand::None
          && buttons.draft() == defaults && buttons.settings() == custom && buttons.dirty(),
          "Reset Defaults stages quality and display defaults until Apply");
    const auto defaultDraft = buttons.draft();
    buttons.changeSelected(1);
    check(buttons.draft() == defaultDraft, "Left and right do not accidentally activate footer buttons");
    buttons.close();
    check(buttons.draft() == custom && !buttons.dirty(), "Closing discards an unapplied Reset Defaults");

    PauseMenu retry;
    retry.openGraphics();
    retry.changeSelected(1);
    const auto applied = retry.draft();
    check(retry.dispatch(PauseUiApply) == PauseCommand::Apply, "First Apply requests runtime and persistence work");
    retry.completeApply(applied, "Applied, but settings could not be saved.");
    check(retry.settings() == applied && !retry.dirty() && !retry.message().empty()
          && retry.dispatch(PauseUiApply) == PauseCommand::Apply,
          "A save failure accurately reports active settings and permits retry without another edit");
    retry.completeApply(applied, "Saved");
    check(retry.message() == "Saved" && retry.settings() == applied,
          "A successful retry replaces the failure message");
    auto badSettings = applied;
    badSettings.msaa = 16;
    bool rejected = false;
    try { retry.completeApply(badSettings, "Invalid"); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && retry.settings() == applied && retry.draft() == applied && retry.message() == "Saved",
          "A rejected Apply cannot partially change settings or lose the previous message");
    rejected = false;
    try { PauseMenu invalid(badSettings); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "The menu cannot begin with invalid settings");

    for (const auto screen : {PauseScreen::Home, PauseScreen::Graphics}) {
        PauseMenu stable;
        stable.open();
        if (screen == PauseScreen::Graphics) stable.openGraphics();
        for (const int action : {-1, 0, 7, 99, 108, 199, 208, 299, 308, std::numeric_limits<int>::max()}) {
            check(stable.dispatch(action) == PauseCommand::None && stable.screen() == screen
                  && stable.selected() == 0 && stable.settings() == defaults && stable.draft() == defaults,
                  "Out-of-range pointer actions cannot change menu state");
        }
        stable.moveSelection(0);
        stable.changeSelected(0);
        check(stable.selected() == 0 && !stable.dirty(), "Zero navigation input is a no-op");
        stable.moveSelection(std::numeric_limits<int>::min());
        check(stable.selected() == (screen == PauseScreen::Home ? 2 : 10),
              "Extreme negative navigation remains in bounds");
        stable.moveSelection(std::numeric_limits<int>::max());
        check(stable.selected() == 0, "Extreme positive navigation remains in bounds");
    }

    PauseMenu hover(custom);
    hover.open();
    hover.completeApply(custom, "Saved");
    const std::array homeActions{PauseUiResume, PauseUiGraphics, PauseUiQuit};
    for (int row = 0; row < 3; ++row) {
        hover.selectAction(homeActions[row]);
        check(hover.selected() == row && hover.screen() == PauseScreen::Home
              && hover.settings() == custom && hover.draft() == custom && hover.message() == "Saved",
              "Hover selects each home button without activating it or clearing feedback");
    }
    for (const int action : {PauseUiBack, PauseUiApply, PauseUiDefaults, PauseUiRowBase,
                            PauseUiDecreaseBase, PauseUiIncreaseBase, -1, 0, std::numeric_limits<int>::max()}) {
        hover.selectAction(action);
        check(hover.selected() == 2 && hover.screen() == PauseScreen::Home && hover.message() == "Saved",
              "Home hover ignores graphics controls and invalid actions");
    }
    hover.openGraphics();
    hover.completeApply(custom, "Applied, but settings could not be saved.");
    for (int row = 0; row < 8; ++row) {
        for (const int base : {PauseUiRowBase, PauseUiDecreaseBase, PauseUiIncreaseBase}) {
            hover.selectAction(base + row);
            check(hover.selected() == row && hover.screen() == PauseScreen::Graphics
                  && hover.draft() == custom && hover.settings() == custom
                  && hover.message() == "Applied, but settings could not be saved.",
                  "Hovering a label, value, or arrow selects its row without changing settings or errors");
        }
    }
    const std::array footerActions{PauseUiDefaults, PauseUiBack, PauseUiApply};
    for (int button = 0; button < 3; ++button) {
        hover.selectAction(footerActions[button]);
        check(hover.selected() == button + 8 && hover.screen() == PauseScreen::Graphics
              && hover.draft() == custom && hover.message() == "Applied, but settings could not be saved.",
              "Hovering footer buttons never resets, leaves, or applies the page");
    }
    for (const int action : {PauseUiResume, PauseUiGraphics, PauseUiQuit, 108, 208, 308, -1, 0,
                            std::numeric_limits<int>::max()}) {
        hover.selectAction(action);
        check(hover.selected() == 10 && hover.screen() == PauseScreen::Graphics,
              "Graphics hover ignores hidden home buttons and invalid controls");
    }
    hover.selectAction(PauseUiIncreaseBase + 1);
    hover.changeSelected(1);
    const auto hoveredDraft = hover.draft();
    hover.selectAction(PauseUiBack);
    check(hover.dirty() && hover.draft() == hoveredDraft && hover.settings() == custom,
          "Hovering Back preserves pending changes until the player activates it");
    hover.selectAction(PauseUiRowBase + 2);
    check(hover.activateSelected() == PauseCommand::None && hover.draft().msaa == 0,
          "Keyboard activation follows the most recently hovered row");
    hover.close();
    for (const int action : {PauseUiResume, PauseUiGraphics, PauseUiQuit, PauseUiBack, PauseUiApply,
                            PauseUiDefaults, PauseUiRowBase, PauseUiDecreaseBase, PauseUiIncreaseBase}) {
        hover.selectAction(action);
        check(hover.selected() == 0 && hover.screen() == PauseScreen::Closed && hover.draft() == custom,
              "A closed menu ignores stale hover actions");
    }
    return failures ? 1 : 0;
}
