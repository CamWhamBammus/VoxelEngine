#pragma once

#include "graphics_settings.h"
#include <string>

namespace voxel {

enum class PauseScreen { Closed, Home, Graphics };
enum class PauseCommand { None, Resume, Apply, Quit };
inline constexpr int PauseUiResume = 1, PauseUiGraphics = 2, PauseUiQuit = 3;
inline constexpr int PauseUiBack = 4, PauseUiApply = 5, PauseUiDefaults = 6;
inline constexpr int PauseUiRowBase = 100, PauseUiDecreaseBase = 200, PauseUiIncreaseBase = 300;

// Menu state is independent of the world. Only Apply changes the running
// settings; leaving the graphics page discards its unfinished adjustments.
class PauseMenu {
public:
    explicit PauseMenu(GraphicsSettings settings = {});
    PauseScreen screen() const { return screen_; }
    const GraphicsSettings& settings() const { return settings_; }
    const GraphicsSettings& draft() const { return draft_; }
    int selected() const { return selected_; }
    bool dirty() const { return draft_ != settings_; }
    const std::string& message() const { return message_; }
    void open();
    void close();
    void openGraphics();
    void moveSelection(int direction);
    // Hover follows visible controls without activating them or changing feedback.
    void selectAction(int action);
    void changeSelected(int direction);
    PauseCommand activateSelected();
    PauseCommand dispatch(int action);
    void completeApply(GraphicsSettings settings, std::string message);

private:
    GraphicsSettings settings_, draft_;
    PauseScreen screen_ = PauseScreen::Closed;
    int selected_ = 0;
    std::string message_;
};

} // namespace voxel
