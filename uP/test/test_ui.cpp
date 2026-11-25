#include "doctest/doctest.h"

#include <iterator>

#include "custom_screen.h"
#include "menus.h"
#include "settings.h"
#include "ui.h"

using namespace ui;

/* *********************  Fake Tables  ************************/

namespace {

constexpr uint8_t VISIBLE_ROWS = 3;

enum FakeScreens : ScreenId {
    FAKE_ROOT,   // one of each item kind
    FAKE_LONG,   // more items than VISIBLE_ROWS, for scrolling
    FAKE_DEEP,   // submenu that points to itself, for stack overflow
    FAKE_EMPTY,  // list with no items
    FAKE_CUSTOM, // custom screen
    FAKE_COUNT
};

enum FakeActions : ActionId { FAKE_ACTION_A };

constexpr Item ROOT_ITEMS[] = {
    {ItemKind::Submenu, "Long",   FAKE_LONG},
    {ItemKind::Submenu, "Deep",   FAKE_DEEP},
    {ItemKind::Submenu, "Empty",  FAKE_EMPTY},
    {ItemKind::Submenu, "Custom", FAKE_CUSTOM},
    {ItemKind::Action,  "Act",    FAKE_ACTION_A},
    {ItemKind::Setting, "Trig",   SETTING_TRIGGER_EDGE},
};

constexpr Item LONG_ITEMS[] = {
    {ItemKind::Action, "L0", FAKE_ACTION_A},
    {ItemKind::Action, "L1", FAKE_ACTION_A},
    {ItemKind::Action, "L2", FAKE_ACTION_A},
    {ItemKind::Action, "L3", FAKE_ACTION_A},
    {ItemKind::Action, "L4", FAKE_ACTION_A},
    {ItemKind::Action, "L5", FAKE_ACTION_A},
    {ItemKind::Action, "L6", FAKE_ACTION_A},
};

constexpr Item DEEP_ITEMS[] = {
    {ItemKind::Submenu, "Deeper", FAKE_DEEP},
};

constexpr Screen FAKE_SCREENS[] = {
    {FAKE_ROOT,   ScreenKind::List,   "Root",   ROOT_ITEMS, std::size(ROOT_ITEMS)},
    {FAKE_LONG,   ScreenKind::List,   "Long",   LONG_ITEMS, std::size(LONG_ITEMS)},
    {FAKE_DEEP,   ScreenKind::List,   "Deep",   DEEP_ITEMS, std::size(DEEP_ITEMS)},
    {FAKE_EMPTY,  ScreenKind::List,   "Empty",  nullptr,    0},
    {FAKE_CUSTOM, ScreenKind::Custom, "Custom", nullptr,    0},
};
static_assert(std::size(FAKE_SCREENS) == FAKE_COUNT);

// records calls, consumes whatever `consume` says
struct FakeCustom : CustomScreen {
    bool consume = false;
    int inputs = 0;
    int enters = 0;
    int exits = 0;
    Input last = Input::Up;

    bool handle_input(Input in) override {
        inputs++;
        last = in;
        return consume;
    }
    void on_enter() override { enters++; }
    void on_exit() override { exits++; }
};

// fresh Settings + Menu over the fake tables for each test
struct Fixture {
    Settings settings;
    FakeCustom custom;
    Menu menu{FAKE_SCREENS, FAKE_COUNT, FAKE_ROOT, settings, VISIBLE_ROWS};

    Fixture() { menu.register_custom(FAKE_CUSTOM, &custom); }
};

} // namespace

/* *********************  Settings  ************************/

TEST_CASE("settings start at defaults") {
}

TEST_CASE("settings cycle wraps to first choice") {
}

TEST_CASE("settings value_text matches current choice") {
}

TEST_CASE("settings reset_defaults restores every setting") {
}

/* *********************  Tables  ************************/

TEST_CASE("real SCREENS: list screens have items, custom screens don't") {
}

/* *********************  Cursor  ************************/

TEST_CASE_FIXTURE(Fixture, "starts on root with cursor 0") {
}

TEST_CASE_FIXTURE(Fixture, "down/up move the cursor") {
}

TEST_CASE_FIXTURE(Fixture, "cursor wraps at both ends") {
}

TEST_CASE_FIXTURE(Fixture, "up/down/select do nothing on an empty screen") {
}

/* *********************  Scroll  ************************/

TEST_CASE_FIXTURE(Fixture, "moving past visible_rows advances scroll") {
}

TEST_CASE_FIXTURE(Fixture, "wrapping to the top resets scroll") {
}

/* *********************  Stack  ************************/

TEST_CASE_FIXTURE(Fixture, "selecting a submenu pushes it with cursor 0") {
}

TEST_CASE_FIXTURE(Fixture, "back pops and restores cursor and scroll") {
}

TEST_CASE_FIXTURE(Fixture, "back on root does nothing") {
}

TEST_CASE_FIXTURE(Fixture, "push beyond MAX_DEPTH is ignored") {
}

/* *********************  Items  ************************/

TEST_CASE_FIXTURE(Fixture, "action item returns Action event without changing state") {
}

TEST_CASE_FIXTURE(Fixture, "setting item cycles value and returns SettingChanged") {
}

TEST_CASE_FIXTURE(Fixture, "item_value is nullptr for non-setting items") {
}

/* *********************  Aliases  ************************/

TEST_CASE_FIXTURE(Fixture, "Right acts as Select on list screens") {
}

TEST_CASE_FIXTURE(Fixture, "Left acts as Back on list screens") {
}

/* *********************  Custom Screens  ************************/

TEST_CASE_FIXTURE(Fixture, "entering a custom screen calls on_enter once") {
}

TEST_CASE_FIXTURE(Fixture, "custom screen sees input before Menu") {
}

TEST_CASE_FIXTURE(Fixture, "consumed Left/Back does not pop a custom screen") {
}

TEST_CASE_FIXTURE(Fixture, "unconsumed Left/Back pops a custom screen and calls on_exit") {
}

/* *********************  Dirty Flag  ************************/

TEST_CASE_FIXTURE(Fixture, "dirty is set at startup") {
}

TEST_CASE_FIXTURE(Fixture, "state-changing input sets dirty") {
}

TEST_CASE_FIXTURE(Fixture, "no-op input does not set dirty") {
}
