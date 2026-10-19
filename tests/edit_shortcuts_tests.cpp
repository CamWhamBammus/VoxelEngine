#include "input.h"
#include <iostream>
#include <limits>

int main()
{
    int failures = 0;
    voxel::EditShortcuts shortcuts;
    const auto check = [&](voxel::EditShortcutKeys keys, bool undo, bool redo) {
        const auto actions = shortcuts.update(keys);
        if (actions.undo != undo || actions.redo != redo) {
            std::cerr << "Unexpected undo/redo key transition\n";
            ++failures;
        }
    };
    check({true, false, true, true}, false, true); // Cmd/Ctrl Shift Z.
    check({true, false, true, false}, false, false); // Release Shift first.
    check({true, false, false, false}, false, false); // Release modifier.
    check({}, false, false);
    check({true, false, true, false}, true, false);
    check({true, false, true, true}, false, false); // Add Shift while Z held.
    check({}, false, false);
    check({false, true, true, false}, false, true);
    check({false, true, true, false}, false, false); // Hold/repeat.
    check({}, false, false);
    check({true, false, false, false}, false, false); // Plain Z.
    check({true, false, true, false}, false, false); // Late modifier.
    check({}, false, false);
    check({true, false, true, false}, true, false); // New key press.

    // Clipboard commands fire on the letter's physical press, never on modifier
    // changes or repeats. A held paste must not repeatedly create a build action.
    voxel::EditShortcuts clipboard;
    const auto checkClipboard = [&](voxel::EditShortcutKeys keys, bool copy, bool paste) {
        const auto actions = clipboard.update(keys);
        if (actions.copy != copy || actions.paste != paste || actions.undo || actions.redo) {
            std::cerr << "Unexpected copy/paste key transition\n";
            ++failures;
        }
    };
    checkClipboard({.modifier = true, .c = true}, true, false);
    checkClipboard({.modifier = true, .c = true}, false, false);
    checkClipboard({.c = true}, false, false);
    checkClipboard({}, false, false);
    checkClipboard({.v = true}, false, false);
    checkClipboard({.modifier = true, .v = true}, false, false); // Late modifier.
    checkClipboard({.modifier = true}, false, false);
    checkClipboard({.modifier = true, .v = true}, false, true);
    checkClipboard({.modifier = true, .v = true}, false, false);
    checkClipboard({.modifier = true, .shift = true, .v = true}, false, false);
    checkClipboard({}, false, false);
    checkClipboard({.modifier = true, .v = true}, false, true);

    voxel::HotbarScroll scroll;
    const auto wheel = [&](double offset, int expected) {
        if (scroll.update(offset) != expected) {
            std::cerr << "Unexpected hotbar scroll for " << offset << '\n';
            ++failures;
        }
    };
    wheel(1, 1); // Wheel forward now advances right through the hotbar.
    wheel(-1, -1);
    wheel(0.25, 0);
    wheel(0, 0); // Empty frames retain fractional motion without selecting a slot.
    wheel(0.25, 0);
    wheel(0.5, 1);
    wheel(-0.25, 0);
    wheel(-0.75, -1);
    wheel(2.5, 2); // Multi-tick movement, with fractional remainder preserved.
    wheel(0.5, 1);
    wheel(-2.75, -2);
    wheel(-0.25, -1);
    wheel(0.75, 0);
    wheel(-0.75, 0); // Reversing cancels sub-tick movement.
    for (int tick = 0; tick < 9; ++tick) wheel(0.1, 0);
    wheel(0.1, 1); // Floating-point rounding must not eat the tenth tick.
    wheel(100.25, voxel::HotbarScroll::MaxSteps);
    wheel(0, 0); // A trackpad fling cannot queue more whole steps for later frames.
    wheel(0.75, 1);
    wheel(-100.25, -voxel::HotbarScroll::MaxSteps);
    wheel(-0.75, -1);
    wheel(0.75, 0);
    scroll.reset(); // Menus, capture changes and focus loss all clear this state.
    wheel(0.25, 0);
    wheel(0.75, 1);
    wheel(std::numeric_limits<double>::infinity(), 0);
    wheel(std::numeric_limits<double>::quiet_NaN(), 0);
    wheel(1, 1);
    return failures ? 1 : 0;
}
