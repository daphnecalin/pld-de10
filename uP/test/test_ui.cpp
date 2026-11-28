#include "doctest/doctest.h"

#include <initializer_list>
#include <iterator>
#include <ostream>
#include <string>

#include "custom_screen.h"
#include "menus.h"
#include "settings.h"
#include "ui.h"

using namespace ui;

/* *********************  Operators  ************************/

// doctest finds these through ADL, so they live in namespace ui.
// operator== lets tests compare whole events: CHECK(ev == Event{EventType::Action, 0})
// operator<< makes failures print "{Action, 0}" instead of "{?}"
namespace ui {

inline bool operator==(const Event& a, const Event& b) {
    return a.type == b.type && a.id == b.id;
}

inline bool operator!=(const Event& a, const Event& b) {
    return !(a == b);
}

inline std::ostream& operator<<(std::ostream& os, EventType t) {
    switch(t) {
        case EventType::None:           return os << "None";
        case EventType::Action:         return os << "Action";
        case EventType::SettingChanged: return os << "SettingChanged";
    }
    return os << "EventType(" << static_cast<int>(t) << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Event& e) {
    return os << "{" << e.type << ", " << static_cast<int>(e.id) << "}";
}

inline std::ostream& operator<<(std::ostream& os, ScreenKind k) {
    switch(k) {
        case ScreenKind::List:   return os << "List";
        case ScreenKind::Custom: return os << "Custom";
    }
    return os << "ScreenKind(" << static_cast<int>(k) << ")";
}

inline std::ostream& operator<<(std::ostream& os, Input in) {
    switch(in) {
        case Input::Up:     return os << "Up";
        case Input::Down:   return os << "Down";
        case Input::Left:   return os << "Left";
        case Input::Right:  return os << "Right";
        case Input::Select: return os << "Select";
        case Input::Back:   return os << "Back";
    }
    return os << "Input(" << static_cast<int>(in) << ")";
}

} // namespace ui

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

// position of each item in ROOT_ITEMS, so tests don't use magic numbers
enum RootItem : uint8_t {
    ITEM_LONG,
    ITEM_DEEP,
    ITEM_EMPTY,
    ITEM_CUSTOM,
    ITEM_ACT,
    ITEM_TRIG,
    ROOT_ITEM_COUNT
};

constexpr Item ROOT_ITEMS[] = {
    {ItemKind::Submenu, "Long",   FAKE_LONG},
    {ItemKind::Submenu, "Deep",   FAKE_DEEP},
    {ItemKind::Submenu, "Empty",  FAKE_EMPTY},
    {ItemKind::Submenu, "Custom", FAKE_CUSTOM},
    {ItemKind::Action,  "Act",    FAKE_ACTION_A},
    {ItemKind::Setting, "Trig",   SETTING_TRIGGER_EDGE},
};
static_assert(std::size(ROOT_ITEMS) == ROOT_ITEM_COUNT);

constexpr Item LONG_ITEMS[] = {
    {ItemKind::Action, "L0", FAKE_ACTION_A},
    {ItemKind::Action, "L1", FAKE_ACTION_A},
    {ItemKind::Action, "L2", FAKE_ACTION_A},
    {ItemKind::Action, "L3", FAKE_ACTION_A},
    {ItemKind::Action, "L4", FAKE_ACTION_A},
    {ItemKind::Action, "L5", FAKE_ACTION_A},
    {ItemKind::Action, "L6", FAKE_ACTION_A},
};
constexpr uint8_t LONG_COUNT = std::size(LONG_ITEMS);
static_assert(LONG_COUNT > VISIBLE_ROWS, "FAKE_LONG must be able to scroll");

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
// (doctest constructs a new Fixture per TEST_CASE_FIXTURE and per SUBCASE run)
// inside a test body, use `menu`, `settings`, `custom` directly
//
// the fixture does NOT clear the dirty flag after construction;
// call menu.clear_dirty() yourself before checking dirty behavior
struct Fixture {
    Settings settings;
    FakeCustom custom;
    Menu menu{FAKE_SCREENS, FAKE_COUNT, FAKE_ROOT, settings, VISIBLE_ROWS};

    Fixture() { menu.register_custom(FAKE_CUSTOM, &custom); }

    // press `in` `times` times, return the event from the last press
    Event press(Input in, int times = 1) {
        Event ev{};
        for(int i = 0; i < times; i++) ev = menu.handle_input(in);
        return ev;
    }

    // from the root screen, move the cursor onto `item` and press Select
    // returns the Select event (Action/SettingChanged, or None for submenus)
    Event select_root_item(RootItem item) {
        REQUIRE(menu.current_screen() == FAKE_ROOT);
        for(int i = 0; i < ROOT_ITEM_COUNT && menu.cursor() != item; i++) {
            menu.handle_input(Input::Down);
        }
        REQUIRE(menu.cursor() == item);
        return menu.handle_input(Input::Select);
    }

    // number of screens on the stack (1 = only the root)
    uint8_t depth() const { return menu.depth(); }
};

} // namespace

/* *********************  Settings  ************************/

TEST_CASE("settings start at defaults") {
    Settings s;
    // check every id is at default
    for(int i = 0; i < SETTING_COUNT; i++) {
        CHECK(s.get(static_cast<SettingId>(i)) == SETTING_DEFS[i].default_index);
    }
}

TEST_CASE("settings cycle wraps to first choice") {
    Settings s;
    // cycle options + 1 times and check the value is right
    for(int i = 0; i < SETTING_COUNT; i++) {
        uint8_t def = SETTING_DEFS[i].default_index;
        uint8_t lim = SETTING_DEFS[i].choice_count;
        SettingId id = static_cast<SettingId>(i);
        for(int j = 0; j < lim + 1; j++) {
            CHECK(s.get(id) == (def + j) % lim);
            s.cycle(id);
        }
    }
}

TEST_CASE("settings value_text matches current choice") {
    Settings s;
    // check value_text against setting_defs
    for(int i = 0; i < SETTING_COUNT; i++) {
        uint8_t lim = SETTING_DEFS[i].choice_count;
        SettingId id = static_cast<SettingId>(i);
        for(int j = 0; j < lim; j++) {
            CHECK(std::string(s.value_text(id)) ==
                  std::string(SETTING_DEFS[i].choices[s.get(id)]));
            s.cycle(id);
        }
    }
}

TEST_CASE("settings reset_defaults restores every setting") {
    Settings s;
    // cycle every setting once
    for(int i = 0; i < SETTING_COUNT; i++) {
        s.cycle(static_cast<SettingId>(i));
    }

    s.reset_defaults();
    // then check that they're all defaults
    for(int i = 0; i < SETTING_COUNT; i++) {
        CHECK(s.get(static_cast<SettingId>(i)) == SETTING_DEFS[i].default_index);
    }
}

/* *********************  Tables  ************************/

TEST_CASE("real SCREENS: list screens have items, custom screens don't") {
    // order is already checked by static_assert in menus.cpp, so just check shape
    for(int i = 0; i < app::SCREEN_COUNT; i++) {
        const Screen& s = app::SCREENS[i];
        CAPTURE(i);
        if(s.kind == ScreenKind::List) {
            CHECK(s.items != nullptr);
            CHECK(s.item_count > 0);
        } else {
            CHECK(s.items == nullptr);
            CHECK(s.item_count == 0);
        }
    }
}

/* *********************  Cursor  ************************/

TEST_CASE_FIXTURE(Fixture, "starts on root with cursor 0") {
    CHECK(menu.current_screen() == FAKE_ROOT);
    CHECK(menu.cursor() == 0);
    CHECK(menu.scroll() == 0);
    CHECK(depth() == 1);
    // render queries should match the root's table entry
    CHECK(std::string(menu.title()) == "Root");
    CHECK(menu.item_count() == ROOT_ITEM_COUNT);
}

TEST_CASE_FIXTURE(Fixture, "down/up move the cursor") {
    press(Input::Down);
    CHECK(menu.cursor() == 1);
    press(Input::Down);
    CHECK(menu.cursor() == 2);
    press(Input::Up);
    CHECK(menu.cursor() == 1);
}

TEST_CASE_FIXTURE(Fixture, "cursor wraps at both ends") {
    // up from the top wraps to the bottom
    press(Input::Up);
    CHECK(menu.cursor() == ROOT_ITEM_COUNT - 1);
    // down from the bottom wraps to the top
    press(Input::Down);
    CHECK(menu.cursor() == 0);
}

TEST_CASE_FIXTURE(Fixture, "up/down/select do nothing on an empty screen") {
    select_root_item(ITEM_EMPTY);
    REQUIRE(menu.current_screen() == FAKE_EMPTY);
    // none of these should move anything or produce an event
    for(Input in : {Input::Up, Input::Down, Input::Select}) {
        CAPTURE(in);
        CHECK(press(in) == Event{});
        CHECK(menu.current_screen() == FAKE_EMPTY);
        CHECK(menu.cursor() == 0);
    }
}

/* *********************  Scroll  ************************/

TEST_CASE_FIXTURE(Fixture, "moving past visible_rows advances scroll") {
    select_root_item(ITEM_LONG);
    REQUIRE(menu.current_screen() == FAKE_LONG);

    // cursor on the last visible row, no scroll yet
    press(Input::Down, VISIBLE_ROWS - 1);
    CHECK(menu.cursor() == VISIBLE_ROWS - 1);
    CHECK(menu.scroll() == 0);

    // one more moves past the window
    press(Input::Down);
    CHECK(menu.cursor() == VISIBLE_ROWS);
    CHECK(menu.scroll() == 1);

    // cursor stays inside the window the rest of the way down
    for(int i = VISIBLE_ROWS + 1; i < LONG_COUNT; i++) {
        press(Input::Down);
        CHECK(menu.cursor() >= menu.scroll());
        CHECK(menu.cursor() < menu.scroll() + VISIBLE_ROWS);
    }

    // moving up inside the window doesn't scroll
    uint8_t scroll = menu.scroll();
    press(Input::Up);
    CHECK(menu.scroll() == scroll);

    // moving up past the top of the window scrolls up with the cursor
    press(Input::Up, VISIBLE_ROWS - 1);
    CHECK(menu.scroll() == menu.cursor());
}

TEST_CASE_FIXTURE(Fixture, "wrapping to the top resets scroll") {
    select_root_item(ITEM_LONG);
    REQUIRE(menu.current_screen() == FAKE_LONG);

    // wrap to the bottom, last item should be the last visible row
    press(Input::Up);
    CHECK(menu.cursor() == LONG_COUNT - 1);
    CHECK(menu.scroll() == LONG_COUNT - VISIBLE_ROWS);

    // wrap back to the top
    press(Input::Down);
    CHECK(menu.cursor() == 0);
    CHECK(menu.scroll() == 0);
}

/* *********************  Stack  ************************/

TEST_CASE_FIXTURE(Fixture, "selecting a submenu pushes it with cursor 0") {
    // submenus don't produce events
    CHECK(select_root_item(ITEM_LONG) == Event{});
    CHECK(menu.current_screen() == FAKE_LONG);
    CHECK(depth() == 2);
    CHECK(menu.cursor() == 0);
    CHECK(menu.scroll() == 0);

    // leave it scrolled, then re-enter: should be a fresh frame
    press(Input::Up);
    press(Input::Back);
    press(Input::Select);
    REQUIRE(menu.current_screen() == FAKE_LONG);
    CHECK(menu.cursor() == 0);
    CHECK(menu.scroll() == 0);
}

// ITEM_CUSTOM has to be far enough down that the root scrolls to reach it,
// otherwise the scroll restore check below is meaningless
static_assert(ITEM_CUSTOM >= VISIBLE_ROWS);

TEST_CASE_FIXTURE(Fixture, "back pops and restores cursor and scroll") {
    select_root_item(ITEM_CUSTOM);
    REQUIRE(menu.current_screen() == FAKE_CUSTOM);

    // root frame should come back exactly as it was left
    press(Input::Back);
    CHECK(menu.current_screen() == FAKE_ROOT);
    CHECK(depth() == 1);
    CHECK(menu.cursor() == ITEM_CUSTOM);
    CHECK(menu.scroll() == ITEM_CUSTOM - VISIBLE_ROWS + 1);
}

TEST_CASE_FIXTURE(Fixture, "back on root does nothing") {
    press(Input::Down, 2);
    menu.clear_dirty();

    CHECK(press(Input::Back) == Event{});
    CHECK(menu.current_screen() == FAKE_ROOT);
    CHECK(depth() == 1);
    CHECK(menu.cursor() == 2);
    CHECK_FALSE(menu.dirty());
}

TEST_CASE_FIXTURE(Fixture, "push beyond MAX_DEPTH is ignored") {
    select_root_item(ITEM_DEEP);
    REQUIRE(depth() == 2);

    // FAKE_DEEP links to itself, so keep selecting past the limit
    press(Input::Select, Menu::MAX_DEPTH + 2);
    CHECK(depth() == Menu::MAX_DEPTH);
    CHECK(menu.current_screen() == FAKE_DEEP);

    // popping MAX_DEPTH - 1 times should land back on root
    press(Input::Back, Menu::MAX_DEPTH - 1);
    CHECK(menu.current_screen() == FAKE_ROOT);
    CHECK(depth() == 1);
}

/* *********************  Items  ************************/

TEST_CASE_FIXTURE(Fixture, "action item returns Action event without changing state") {
    Event ev = select_root_item(ITEM_ACT);
    CHECK(ev == Event{EventType::Action, FAKE_ACTION_A});
    // menu itself shouldn't move
    CHECK(menu.current_screen() == FAKE_ROOT);
    CHECK(depth() == 1);
    CHECK(menu.cursor() == ITEM_ACT);
}

TEST_CASE_FIXTURE(Fixture, "setting item cycles value and returns SettingChanged") {
    uint8_t before = settings.get(SETTING_TRIGGER_EDGE);
    uint8_t count = SETTING_DEFS[SETTING_TRIGGER_EDGE].choice_count;

    Event ev = select_root_item(ITEM_TRIG);
    CHECK(ev == Event{EventType::SettingChanged, SETTING_TRIGGER_EDGE});
    CHECK(settings.get(SETTING_TRIGGER_EDGE) == (before + 1) % count);

    // menu should show the new value
    REQUIRE(menu.item_value(ITEM_TRIG) != nullptr);
    CHECK(std::string(menu.item_value(ITEM_TRIG)) ==
          std::string(settings.value_text(SETTING_TRIGGER_EDGE)));
}

TEST_CASE_FIXTURE(Fixture, "item_value is nullptr for non-setting items") {
    CHECK(menu.item_value(ITEM_LONG) == nullptr); // submenu
    CHECK(menu.item_value(ITEM_ACT) == nullptr);  // action
    CHECK(menu.item_value(ITEM_TRIG) != nullptr); // setting
    // out of range
    CHECK(menu.item_value(ROOT_ITEM_COUNT) == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "item_label is empty out of range") {
    CHECK(std::string(menu.item_label(ITEM_LONG)) == "Long");
    CHECK(std::string(menu.item_label(ROOT_ITEM_COUNT)) == "");
}

/* *********************  Aliases  ************************/

TEST_CASE_FIXTURE(Fixture, "Right acts as Select on list screens") {
    // cursor already starts on ITEM_LONG
    REQUIRE(menu.cursor() == ITEM_LONG);
    press(Input::Right);
    CHECK(menu.current_screen() == FAKE_LONG);
}

TEST_CASE_FIXTURE(Fixture, "Left acts as Back on list screens") {
    select_root_item(ITEM_LONG);
    REQUIRE(menu.current_screen() == FAKE_LONG);
    press(Input::Left);
    CHECK(menu.current_screen() == FAKE_ROOT);
}

/* *********************  Custom Screens  ************************/

TEST_CASE_FIXTURE(Fixture, "entering a custom screen calls on_enter once") {
    select_root_item(ITEM_CUSTOM);
    CHECK(menu.current_screen() == FAKE_CUSTOM);
    CHECK(menu.current_kind() == ScreenKind::Custom);
    CHECK(custom.enters == 1);
    CHECK(custom.exits == 0);
}

TEST_CASE_FIXTURE(Fixture, "custom screen sees input before Menu") {
    select_root_item(ITEM_CUSTOM);
    REQUIRE(menu.current_screen() == FAKE_CUSTOM);
    // navigating the root shouldn't have reached the handler
    CHECK(custom.inputs == 0);

    press(Input::Up);
    press(Input::Down);
    press(Input::Right);
    CHECK(custom.inputs == 3);
    CHECK(custom.last == Input::Right);
    // unconsumed non-back inputs are ignored, so still on the custom screen
    CHECK(menu.current_screen() == FAKE_CUSTOM);
}

TEST_CASE_FIXTURE(Fixture, "consumed Left/Back does not pop a custom screen") {
    select_root_item(ITEM_CUSTOM);
    REQUIRE(menu.current_screen() == FAKE_CUSTOM);
    custom.consume = true;
    menu.clear_dirty();

    press(Input::Left);
    press(Input::Back);
    CHECK(menu.current_screen() == FAKE_CUSTOM);
    CHECK(depth() == 2);
    CHECK(custom.exits == 0);
    // consumed input means the custom view changed, so redraw
    CHECK(menu.dirty());
}

TEST_CASE_FIXTURE(Fixture, "unconsumed Left/Back pops a custom screen and calls on_exit") {
    // test runs once per subcase, each picks the input to press
    Input in = Input::Back;
    SUBCASE("Left") { in = Input::Left; }
    SUBCASE("Back") { in = Input::Back; }
    CAPTURE(in);

    select_root_item(ITEM_CUSTOM);
    REQUIRE(menu.current_screen() == FAKE_CUSTOM);

    press(in);
    CHECK(menu.current_screen() == FAKE_ROOT);
    CHECK(depth() == 1);
    CHECK(custom.exits == 1);
    // handler still got the input first
    CHECK(custom.inputs == 1);
}

/* *********************  Dirty Flag  ************************/

TEST_CASE_FIXTURE(Fixture, "dirty is set at startup") {
    CHECK(menu.dirty());
    menu.clear_dirty();
    CHECK_FALSE(menu.dirty());
}

TEST_CASE_FIXTURE(Fixture, "state-changing input sets dirty") {
    // each subcase gets to the right spot, clears dirty, then does one thing
    SUBCASE("cursor move") {
        menu.clear_dirty();
        press(Input::Down);
    }
    SUBCASE("push") {
        menu.clear_dirty();
        press(Input::Select); // cursor starts on ITEM_LONG
    }
    SUBCASE("pop") {
        select_root_item(ITEM_LONG);
        menu.clear_dirty();
        press(Input::Back);
    }
    SUBCASE("setting change") {
        press(Input::Down, ITEM_TRIG);
        menu.clear_dirty();
        press(Input::Select);
    }
    CHECK(menu.dirty());
}

TEST_CASE_FIXTURE(Fixture, "no-op input does not set dirty") {
    // each subcase gets to the right spot, clears dirty, then does nothing useful
    SUBCASE("back on root") {
        menu.clear_dirty();
        press(Input::Back);
    }
    SUBCASE("up/down on an empty screen") {
        select_root_item(ITEM_EMPTY);
        menu.clear_dirty();
        press(Input::Up);
        press(Input::Down);
    }
    SUBCASE("up/down on a single-item screen") {
        // FAKE_DEEP has one item, wrapping lands on the same spot
        select_root_item(ITEM_DEEP);
        menu.clear_dirty();
        press(Input::Up);
        press(Input::Down);
    }
    SUBCASE("unconsumed input on a custom screen") {
        select_root_item(ITEM_CUSTOM);
        menu.clear_dirty();
        press(Input::Up);
        press(Input::Down);
        press(Input::Right);
    }
    SUBCASE("select at full depth") {
        select_root_item(ITEM_DEEP);
        press(Input::Select, Menu::MAX_DEPTH);
        REQUIRE(depth() == Menu::MAX_DEPTH);
        menu.clear_dirty();
        press(Input::Select);
    }
    SUBCASE("action item") {
        // actions are handled by main.cpp, the menu itself doesn't change
        press(Input::Down, ITEM_ACT);
        menu.clear_dirty();
        press(Input::Select);
    }
    CHECK_FALSE(menu.dirty());
}
