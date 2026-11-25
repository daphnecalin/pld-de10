# UI Backend Spec
What is going on with the ui!

The general gameplan is that the core of the ui is separate from the peripherals.
With a layout like this, all of the core logic for the screen is testable and modular, while the logic outside only needs to handle drawing based off the interfaces provided.

## Files
| File | Contents |
| --- | --- |
| `core/ui.h`, `core/ui.cpp` | Types and `Menu` navigation class |
| `core/settings.h`, `core/settings.cpp` | Class for global settings for the application |
| `core/menus.h`, `core/menus.cpp` | Actual screen/item tables |
| `core/custom_screen.h` | `CustomScreen` for implementing non-menu screens (e.g. waveform viewer) |
| `test/test_ui.cpp` | Tests for backend ui logic |

## Inputs

```cpp
enum class Input { Up, Down, Left, Right, Select, Back };
```

Inputs are an abstract class that map to physical buttons. Instead of the peripheral code telling the menu class to "go to this screen" or "go back a screen", the peripheral code passes an input to the menu class, and the menu class handles transitioning menus and performing actions.

Currently hardware has 4 buttons but select/back are there as placeholders, currently mapped to right/left respectively.

## Screens and items

```cpp
// in ui.h
using ScreenId = uint8_t;
using ActionId = uint8_t;
// two kinds of screens (for now): lists for menus, custom for other things
enum class ScreenKind : uint8_t { List, Custom };
// indicates what a menu option is: into a submenu, performing an action, or changing a setting
enum class ItemKind : uint8_t { Submenu, Action, Setting };

struct Item {
    ItemKind kind;
    const char* label; // text on item
    uint8_t target; // screen to go to, action to perform, setting to change
}

struct Screen {
    ScreenId id;
    ScreenKind kind;
    const char* title; // title at top of screen
    const Item* items; // nullptr for Custom, items to render on screen
    uint8_t item_count; // 0 for Custom
}
```

The table of screens is a `constexpr Screen[]` indexed into by `ScreenId`.

## Settings

All settings are done with enums, boolean settings are a 2-choice enum `{"Off", "On"}` and numeric settings aren't done for now.

```cpp
// example settings
enum SettingId : uint8_t { SETTING_TRIGGER_EDGE, SETTING_CONTINUOUS, SETTING_COUNT };

struct SettingDef {
    SettingId id;
    const char* const* choices; // display strings
    uint8_t choice_count;
    uint8_t default_index;
}

class Settings {
    std::array<uint8_t, SETTING_COUNT> values_; // current choice index per setting
public:
    uint8_t get(SettingId) const; // get current value of setting
    void cycle(SettingId); // cycle through settings, (v + 1) % choice_count
    const char* value_text(SettingId) const;
    void reset_defaults();
}
```

`Menu` holds a reference to an instance of `Settings`, `main.cpp` reads the same instance when configuring things.

Currently, settings will go back to default whenever the system is turned off/on but stretch goal could be persistence to flash.

## Events

```cpp
enum class EventType : uint8_t { None, Action, SettingChanged };
struct Event {
    EventType type;
    uint8_t id; // actionid or settingid
};
```

`Menu::handle_input()` returns an `Event`, then `main.cpp` switches on it.

## Custom screens

to do later, getting basic screens working first

## Navigation

Submenus/screen navigation is done as a stack of frames: `{ScreenId screen; uint8_t cursor; uint8_t scroll}` that hold the cursor and scroll position of wherever the last screen left off, with a fixed depth of 4 (adjustable later, but 4 submenus should be enough).

The root screen is pushed at construction and `Back` on it does nothing. Pushing when full is ignored and popping restores the previous frame's cursor and scroll.

## `Menu` public API

```cpp
class Menu {
    Menu(const Screen* table, uint8_t screen_count, ScreenId root,
         Settings& settings, uint8_t visible_rows);

    // input
    Event handle_input(Input in); // sets dirty flag
    void register_custom(ScreenId id, CustomScreen* screen);

    // render queries
    ScreenId current_screen() const;
    ScreenKind current_kind() const;
    const char* title() const;
    uint8_t item_count() const;
    const char* item_label(uint8_t i) const;
    const char* item_value(uint8_t i) const; // nullptr unless item is a setting
    uint8_t cursor() const;
    uint8_t scroll() const;
    uint8_t visible_rows() const;

    // tracking redraw
    bool dirty() const;
    void clear_dirty();
    void mark_dirty(); // for custom screens
};
```

## Main loop shape

```
read button -> map to ui::Input -> ev = menu.handle_input(in)
switch(ev.type) { Action: ...; SettingChanged: ...; }
if(menu.dirty()) {render(menu); menu.clear_dirty();}
```