#include "doctest/doctest.h"

#include <iterator>
#include <ostream>

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
    // setup: a fresh Settings
    Settings s;
    // check: for every SettingId, get() == SETTING_DEFS[id].default_index
    for(int i = 0; i < SETTING_COUNT; i++) {
        CHECK(s.get(static_cast<SettingId>(i)) == SETTING_DEFS[i].default_index);
    }
}

TEST_CASE("settings cycle wraps to first choice") {
    // setup: a fresh Settings, pick a setting and its choice_count from SETTING_DEFS
    Settings s;
    // act:   cycle() choice_count times, checking get() advances by 1 each time
    for(int i = 0; i < SETTING_COUNT; i++) {
        uint8_t def = SETTING_DEFS[i].default_index;
        uint8_t lim = SETTING_DEFS[i].choice_count;
        for(int j = 0; j < lim + 1; j++) {
            CHECK(s.get(static_cast<SettingId>(i)) == (def + j) % lim);
            s.cycle(static_cast<SettingId>(i));
        }
    }
}

TEST_CASE("settings value_text matches current choice") {
    // setup: a fresh Settings
    // check: value_text(id) == SETTING_DEFS[id].choices[get(id)]
    // act:   cycle(id)
    // check: same comparison still holds with the new index
    // note:  compare C strings with std::string(a) == b, or CHECK(doctest::String(a) == b)
}

TEST_CASE("settings reset_defaults restores every setting") {
    // setup: a fresh Settings, cycle every setting once so none are at default
    // act:   reset_defaults()
    // check: every setting is back to its default_index
}

/* *********************  Tables  ************************/

TEST_CASE("real SCREENS: list screens have items, custom screens don't") {
    // loop over app::SCREENS[0 .. app::SCREEN_COUNT)
    // check: List   -> items != nullptr and item_count > 0
    // check: Custom -> items == nullptr and item_count == 0
    // (order vs ScreenIds is already checked by static_assert in menus.cpp)
}

/* *********************  Cursor  ************************/

TEST_CASE_FIXTURE(Fixture, "starts on root with cursor 0") {
    // check: current_screen() == FAKE_ROOT, cursor() == 0, scroll() == 0, depth() == 1
    // check: title() and item_count() match the FAKE_ROOT entry
}

TEST_CASE_FIXTURE(Fixture, "down/up move the cursor") {
    // act:   press(Input::Down) -> check cursor() == 1
    // act:   press(Input::Down) -> check cursor() == 2
    // act:   press(Input::Up)   -> check cursor() == 1
}

TEST_CASE_FIXTURE(Fixture, "cursor wraps at both ends") {
    // act:   press(Input::Up) from cursor 0
    // check: cursor() == ROOT_ITEM_COUNT - 1
    // act:   press(Input::Down)
    // check: cursor() == 0
}

TEST_CASE_FIXTURE(Fixture, "up/down/select do nothing on an empty screen") {
    // setup: select_root_item(ITEM_EMPTY), check current_screen() == FAKE_EMPTY
    // act:   press Up, Down, Select
    // check: still on FAKE_EMPTY, cursor() == 0, every event == Event{}
}

/* *********************  Scroll  ************************/

TEST_CASE_FIXTURE(Fixture, "moving past visible_rows advances scroll") {
    // setup: select_root_item(ITEM_LONG)
    // act:   press(Input::Down, VISIBLE_ROWS - 1) -> check scroll() == 0 (cursor on last visible row)
    // act:   press(Input::Down)                   -> check scroll() == 1
    // check: cursor is always inside [scroll(), scroll() + VISIBLE_ROWS)
}

TEST_CASE_FIXTURE(Fixture, "wrapping to the top resets scroll") {
    // setup: select_root_item(ITEM_LONG)
    // act:   press(Input::Up) to wrap to the bottom
    // check: cursor() == LONG_COUNT - 1, scroll() == LONG_COUNT - VISIBLE_ROWS
    // act:   press(Input::Down) to wrap to the top
    // check: cursor() == 0, scroll() == 0
}

/* *********************  Stack  ************************/

TEST_CASE_FIXTURE(Fixture, "selecting a submenu pushes it with cursor 0") {
    // setup: select_root_item(ITEM_LONG)
    // check: current_screen() == FAKE_LONG, depth() == 2, cursor() == 0, scroll() == 0
    // check: the Select event == Event{} (submenus don't produce events)
}

TEST_CASE_FIXTURE(Fixture, "back pops and restores cursor and scroll") {
    // setup: select_root_item(ITEM_LONG), then press Down enough to scroll
    //        (remember the root cursor = ITEM_LONG)
    // act:   press(Input::Back)
    // check: current_screen() == FAKE_ROOT, depth() == 1, cursor() == ITEM_LONG
    // optional: re-enter FAKE_LONG and check it starts fresh at cursor 0 (new frame)
}

TEST_CASE_FIXTURE(Fixture, "back on root does nothing") {
    // setup: move the cursor somewhere non-zero, menu.clear_dirty()
    // act:   press(Input::Back)
    // check: still FAKE_ROOT, depth() == 1, cursor unchanged, dirty() == false
}

TEST_CASE_FIXTURE(Fixture, "push beyond MAX_DEPTH is ignored") {
    // setup: select_root_item(ITEM_DEEP) -> depth() == 2
    // act:   press(Input::Select, n) with n large enough to exceed Menu::MAX_DEPTH
    // check: depth() == Menu::MAX_DEPTH, still on FAKE_DEEP
    // act:   press(Input::Back, Menu::MAX_DEPTH - 1)
    // check: back on FAKE_ROOT with depth() == 1
}

/* *********************  Items  ************************/

TEST_CASE_FIXTURE(Fixture, "action item returns Action event without changing state") {
    // act:   ev = select_root_item(ITEM_ACT)
    // check: ev == Event{EventType::Action, FAKE_ACTION_A}
    // check: still FAKE_ROOT, depth() == 1, cursor() == ITEM_ACT
}

TEST_CASE_FIXTURE(Fixture, "setting item cycles value and returns SettingChanged") {
    // setup: remember settings.get(SETTING_TRIGGER_EDGE)
    // act:   ev = select_root_item(ITEM_TRIG)
    // check: ev == Event{EventType::SettingChanged, SETTING_TRIGGER_EDGE}
    // check: settings.get() advanced by 1
    // check: menu.item_value(ITEM_TRIG) == settings.value_text(SETTING_TRIGGER_EDGE)
}

TEST_CASE_FIXTURE(Fixture, "item_value is nullptr for non-setting items") {
    // check: item_value(i) == nullptr for ITEM_LONG / ITEM_ACT (submenu, action)
    // check: item_value(ITEM_TRIG) != nullptr
    // check: item_value(ROOT_ITEM_COUNT) == nullptr (out of range)
}

/* *********************  Aliases  ************************/

TEST_CASE_FIXTURE(Fixture, "Right acts as Select on list screens") {
    // setup: cursor on ITEM_LONG (cursor 0, so no setup needed)
    // act:   press(Input::Right)
    // check: current_screen() == FAKE_LONG
}

TEST_CASE_FIXTURE(Fixture, "Left acts as Back on list screens") {
    // setup: select_root_item(ITEM_LONG)
    // act:   press(Input::Left)
    // check: current_screen() == FAKE_ROOT
}

/* *********************  Custom Screens  ************************/

TEST_CASE_FIXTURE(Fixture, "entering a custom screen calls on_enter once") {
    // act:   select_root_item(ITEM_CUSTOM)
    // check: current_screen() == FAKE_CUSTOM, current_kind() == ScreenKind::Custom
    // check: custom.enters == 1, custom.exits == 0
}

TEST_CASE_FIXTURE(Fixture, "custom screen sees input before Menu") {
    // setup: select_root_item(ITEM_CUSTOM)
    // act:   press Up, Down, Right (custom.consume == false)
    // check: custom.inputs == 3, custom.last == Input::Right
    // check: still on FAKE_CUSTOM (unconsumed non-back inputs are ignored)
}

TEST_CASE_FIXTURE(Fixture, "consumed Left/Back does not pop a custom screen") {
    // setup: select_root_item(ITEM_CUSTOM), custom.consume = true, menu.clear_dirty()
    // act:   press(Input::Left), press(Input::Back)
    // check: still FAKE_CUSTOM, depth() == 2, custom.exits == 0
    // check: dirty() == true (consumed input means the custom view changed)
}

TEST_CASE_FIXTURE(Fixture, "unconsumed Left/Back pops a custom screen and calls on_exit") {
    // use SUBCASE("Left") and SUBCASE("Back") so both inputs get tested
    // setup: select_root_item(ITEM_CUSTOM), custom.consume = false
    // act:   press the input
    // check: current_screen() == FAKE_ROOT, depth() == 1
    // check: custom.exits == 1, custom.inputs == 1 (handler still saw it first)
}

/* *********************  Dirty Flag  ************************/

TEST_CASE_FIXTURE(Fixture, "dirty is set at startup") {
    // check: dirty() == true before anything else
    // act:   clear_dirty() -> check dirty() == false
}

TEST_CASE_FIXTURE(Fixture, "state-changing input sets dirty") {
    // for each of: Down (cursor move), Select on a submenu (push), Back (pop),
    //              Select on ITEM_TRIG (setting change)
    // setup: menu.clear_dirty()
    // act:   the input
    // check: dirty() == true
    // (SUBCASEs work well here, one per input)
}

TEST_CASE_FIXTURE(Fixture, "no-op input does not set dirty") {
    // for each of: Back on root, Up/Down on FAKE_EMPTY, Up/Down/Right on FAKE_CUSTOM
    //              (unconsumed), Select at full depth on FAKE_DEEP
    // setup: navigate there, then menu.clear_dirty()
    // act:   the input
    // check: dirty() == false
}
