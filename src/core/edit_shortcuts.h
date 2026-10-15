#pragma once

namespace voxel {

struct EditShortcutKeys {
    bool z = false;
    bool y = false;
    bool modifier = false;
    bool shift = false;
};

struct EditShortcutActions { bool undo = false; bool redo = false; };

class EditShortcuts {
public:
    EditShortcutActions update(EditShortcutKeys keys)
    {
        // Only a physical letter-key press creates an action. Releasing Shift
        // after redo must never turn the held Z key into a fresh undo action.
        const bool zPressed = keys.z && !previous_.z;
        const bool yPressed = keys.y && !previous_.y;
        previous_ = keys;
        return {keys.modifier && zPressed && !keys.shift,
                keys.modifier && (yPressed || (zPressed && keys.shift))};
    }
private:
    EditShortcutKeys previous_;
};

} // namespace voxel
