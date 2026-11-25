#pragma once

#include "ui.h"

// the application's actual screens and actions
namespace ui::app {

// must match the order of SCREENS
enum ScreenIds : ScreenId {
    SCREEN_MAIN,
    SCREEN_SETTINGS,
    SCREEN_ANALYZE,
    SCREEN_COUNT
};

enum ActionIds : ActionId {
    ACTION_RESET_SETTINGS,
    ACTION_COUNT
};

static_assert(SCREEN_COUNT <= Menu::MAX_SCREENS, "raise Menu::MAX_SCREENS");

extern const Screen SCREENS[SCREEN_COUNT];

} // namespace ui::app
