#include "pause_menu.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace voxel {
namespace {
template<class T, std::size_t N>
T cycle(T value, const std::array<T, N>& choices, int direction)
{
    if (direction > 0) {
        const auto next = std::upper_bound(choices.begin(), choices.end(), value);
        return next == choices.end() ? choices.front() : *next;
    }
    const auto next = std::lower_bound(choices.begin(), choices.end(), value);
    return next == choices.begin() ? choices.back() : *std::prev(next);
}
}

PauseMenu::PauseMenu(GraphicsSettings settings) : settings_(settings), draft_(settings)
{
    if (!validGraphicsSettings(settings)) throw std::invalid_argument("Invalid graphics settings");
}

void PauseMenu::open()
{
    if (screen_ != PauseScreen::Closed) return;
    screen_ = PauseScreen::Home;
    selected_ = 0;
    message_.clear();
}

void PauseMenu::close()
{
    draft_ = settings_;
    screen_ = PauseScreen::Closed;
    selected_ = 0;
    message_.clear();
}

void PauseMenu::openGraphics()
{
    draft_ = settings_;
    screen_ = PauseScreen::Graphics;
    selected_ = 0;
    message_.clear();
}

void PauseMenu::moveSelection(int direction)
{
    if (screen_ == PauseScreen::Closed || !direction) return;
    const int count = screen_ == PauseScreen::Home ? 3 : 11;
    selected_ = (selected_ + (direction > 0 ? 1 : count - 1)) % count;
}

void PauseMenu::changeSelected(int direction)
{
    if (screen_ != PauseScreen::Graphics || !direction || selected_ >= 8) return;
    switch (selected_) {
    case 0: {
        auto preset = graphicsPreset(draft_);
        if (preset == GraphicsPreset::Custom)
            preset = direction > 0 ? GraphicsPreset::Low : GraphicsPreset::High;
        else preset = cycle(preset, std::array{GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High}, direction);
        applyGraphicsPreset(draft_, preset);
        break;
    }
    case 1: draft_.renderScale = cycle(draft_.renderScale, std::array{.5f, .65f, .75f, .85f, 1.f}, direction); break;
    case 2: draft_.msaa = cycle(draft_.msaa, std::array{0, 2, 4}, direction); break;
    case 3: draft_.lighting = cycle(draft_.lighting, std::array{LightingQuality::Low, LightingQuality::Medium, LightingQuality::High}, direction); break;
    case 4: draft_.detail = cycle(draft_.detail, std::array{DetailDistance::Near, DetailDistance::Medium, DetailDistance::Far}, direction); break;
    case 5: draft_.fullscreen = !draft_.fullscreen; break;
    case 6: draft_.vsync = !draft_.vsync; break;
    case 7: draft_.fpsLimit = cycle(draft_.fpsLimit, std::array{0, 30, 60, 90, 120, 144}, direction); break;
    }
    message_.clear();
}

PauseCommand PauseMenu::activateSelected()
{
    if (screen_ == PauseScreen::Home) {
        constexpr std::array actions{PauseUiResume, PauseUiGraphics, PauseUiQuit};
        return dispatch(actions[selected_]);
    }
    if (screen_ == PauseScreen::Graphics) {
        if (selected_ < 8) return dispatch(PauseUiIncreaseBase + selected_);
        constexpr std::array actions{PauseUiDefaults, PauseUiBack, PauseUiApply};
        return dispatch(actions[selected_ - 8]);
    }
    return PauseCommand::None;
}

PauseCommand PauseMenu::dispatch(int action)
{
    if (screen_ == PauseScreen::Closed) return PauseCommand::None;
    if (action == PauseUiResume) { close(); return PauseCommand::Resume; }
    if (screen_ == PauseScreen::Home) {
        if (action == PauseUiGraphics) openGraphics();
        else if (action == PauseUiQuit) return PauseCommand::Quit;
        return PauseCommand::None;
    }
    if (action == PauseUiBack) {
        draft_ = settings_;
        screen_ = PauseScreen::Home;
        selected_ = 1;
        message_.clear();
    } else if (action == PauseUiApply) {
        return PauseCommand::Apply;
    } else if (action == PauseUiDefaults) {
        draft_ = {};
        selected_ = 8;
        message_.clear();
    } else if (action >= PauseUiRowBase && action < PauseUiRowBase + 8) {
        selected_ = action - PauseUiRowBase;
    } else if (action >= PauseUiDecreaseBase && action < PauseUiDecreaseBase + 8) {
        selected_ = action - PauseUiDecreaseBase;
        changeSelected(-1);
    } else if (action >= PauseUiIncreaseBase && action < PauseUiIncreaseBase + 8) {
        selected_ = action - PauseUiIncreaseBase;
        changeSelected(1);
    }
    return PauseCommand::None;
}

void PauseMenu::completeApply(GraphicsSettings settings, std::string message)
{
    if (!validGraphicsSettings(settings)) throw std::invalid_argument("Invalid graphics settings");
    settings_ = draft_ = settings;
    message_ = std::move(message);
}

} // namespace voxel
