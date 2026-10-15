#include "core/edit_shortcuts.h"
#include <iostream>

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
    return failures ? 1 : 0;
}
