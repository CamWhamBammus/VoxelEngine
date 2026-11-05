#include "graphics/graphics_menu_layout.h"
#include "pause_menu.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

namespace {
int failures = 0;
void check(bool value, const char* message)
{
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main()
{
    using namespace voxel;
    struct View { int width, height; float dpi; };
    constexpr View views[] = {{1280,720,1}, {2560,1440,2}, {640,480,1}, {320,240,2},
                              {1440,900,1.5f}, {480,900,1}, {3840,2160,2}};
    for (const auto view : views) for (const auto screen : {PauseScreen::Home, PauseScreen::Graphics}) {
        const auto layout = pauseMenuLayout(screen, view.width, view.height, view.dpi);
        check(layout.scale > 0, "A drawable screen has a positive scale");
        const auto panel = layout.panel;
        check(panel.x >= 0 && panel.y >= 0 && panel.x + panel.width <= layout.width
                  && panel.y + panel.height <= layout.height, "The complete panel fits every framebuffer");
        check(layout.controls.size() == (screen == PauseScreen::Home ? 3u : 35u),
              "Every visible control has a pointer target");
        std::set<int> selections;
        for (std::size_t index = 0; index < layout.controls.size(); ++index) {
            const auto& control = layout.controls[index];
            const auto& rect = control.rect;
            selections.insert(control.selection);
            check(rect.x >= panel.x && rect.y >= panel.y && rect.x + rect.width <= panel.x + panel.width
                      && rect.y + rect.height <= panel.y + panel.height, "Controls stay inside the panel");
            const float px = (rect.x + rect.width * .5f) * layout.scale;
            const float py = (rect.y + rect.height * .5f) * layout.scale;
            check(layout.hit(px, py) == control.action, "A scaled control center hits its displayed action");
            for (std::size_t other = index + 1; other < layout.controls.size(); ++other) {
                const auto& b = layout.controls[other].rect;
                check(!(rect.x < b.x + b.width && b.x < rect.x + rect.width
                            && rect.y < b.y + b.height && b.y < rect.y + rect.height),
                      "Distinct pointer targets never overlap");
            }
        }
        check(selections.size() == (screen == PauseScreen::Home ? 3u : 11u),
              "Every keyboard selection also has a mouse target");
        check(!layout.hit(0,0) && !layout.hit(-1,-1) && !layout.hit(view.width,view.height),
              "Clicking outside the panel is harmless");
        check(!layout.hit(std::numeric_limits<float>::quiet_NaN(),0)
                  && !layout.hit(0,std::numeric_limits<float>::infinity()), "Nonfinite pointers are ignored");
    }
    for (float dpi : {0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        const auto invalid = pauseMenuLayout(PauseScreen::Graphics,1280,720,dpi);
        const auto normal = pauseMenuLayout(PauseScreen::Graphics,1280,720,1);
        check(invalid.scale == normal.scale && invalid.controls.size() == normal.controls.size(),
              "Invalid content scales use a stable default");
    }
    for (const auto screen : {PauseScreen::Closed,PauseScreen::Home,PauseScreen::Graphics}) {
        const auto minimized = pauseMenuLayout(screen,0,720,1);
        check(minimized.controls.empty() && !minimized.hit(0,0), "A minimized window has no active controls");
    }
    const auto closed = pauseMenuLayout(PauseScreen::Closed,1280,720,1);
    check(closed.controls.empty() && !closed.hit(640,360), "The closed menu cannot consume a pointer");
    if (!failures) std::cout << "Graphics menu layout checks passed\n";
    return failures ? 1 : 0;
}
