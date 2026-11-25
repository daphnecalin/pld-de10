#include "menus.h"

#include <iterator>

namespace ui::app {

/* *********************  Items  ************************/

constexpr Item MAIN_ITEMS[] = {
    {ItemKind::Submenu, "Analyze",  SCREEN_ANALYZE},
    {ItemKind::Submenu, "Settings", SCREEN_SETTINGS},
};

constexpr Item SETTINGS_ITEMS[] = {
    {ItemKind::Setting, "Trigger",    SETTING_TRIGGER_EDGE},
    {ItemKind::Setting, "Continuous", SETTING_CONTINUOUS},
    {ItemKind::Action,  "Reset",      ACTION_RESET_SETTINGS},
};

/* *********************  Screens  ************************/

// must be in the same order as ScreenIds
constexpr Screen SCREENS[] = {
    {SCREEN_MAIN,     ScreenKind::List,   "Main",     MAIN_ITEMS,     std::size(MAIN_ITEMS)},
    {SCREEN_SETTINGS, ScreenKind::List,   "Settings", SETTINGS_ITEMS, std::size(SETTINGS_ITEMS)},
    {SCREEN_ANALYZE,  ScreenKind::Custom, "Analyze",  nullptr,        0},
};

static_assert(std::size(SCREENS) == SCREEN_COUNT,
              "SCREENS must have one entry per ScreenIds value");

constexpr bool screens_in_order() {
    for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
        if (SCREENS[i].id != i) return false;
    }
    return true;
}
static_assert(screens_in_order(), "SCREENS order must match ScreenIds");

} // namespace ui::app
