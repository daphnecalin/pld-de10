#pragma once

#include <array>
#include <cstdint>

#include "settings.h"

namespace ui {
using std::uint8_t;

class CustomScreen;

// raw ids so Menu isn't tied to one set of screens (tests use fake tables)
// the real ids are the enums in menus.h
using ScreenId = uint8_t;
using ActionId = uint8_t;

// abstract inputs, main.cpp maps physical buttons to these
// with the current 4 buttons, list screens treat Right as Select and Left as Back
enum class Input : uint8_t { Up, Down, Left, Right, Select, Back };

enum class ScreenKind : uint8_t { List, Custom };
enum class ItemKind : uint8_t { Submenu, Action, Setting };

struct Item {
    ItemKind kind;
    const char* label;
    // ScreenId (Submenu), ActionId (Action), or SettingId (Setting)
    uint8_t target;
};

struct Screen {
    ScreenId id;
    ScreenKind kind;
    const char* title;
    const Item* items; // nullptr for Custom
    uint8_t item_count; // 0 for Custom
};

enum class EventType : uint8_t { None, Action, SettingChanged };

// returned from Menu::handle_input for main.cpp to act on
struct Event {
    EventType type = EventType::None;
    uint8_t id = 0; // ActionId or SettingId depending on type
};

class Menu {
public:
    static constexpr uint8_t MAX_DEPTH = 4;
    static constexpr uint8_t MAX_SCREENS = 16;

    Menu(const Screen* table, uint8_t screen_count, ScreenId root,
         Settings& settings, uint8_t visible_rows);

    /* input */
    Event handle_input(Input in);
    void register_custom(ScreenId id, CustomScreen* screen);

    /* render queries (all refer to the screen on top of the stack) */
    ScreenId current_screen() const;
    ScreenKind current_kind() const;
    const char* title() const;
    uint8_t item_count() const;
    const char* item_label(uint8_t i) const;
    // nullptr unless item i is a Setting
    const char* item_value(uint8_t i) const;
    uint8_t cursor() const;
    uint8_t scroll() const;
    uint8_t visible_rows() const;
    // number of screens on the stack (1 = only the root)
    uint8_t depth() const;

    /* redraw tracking */
    bool dirty() const;
    void clear_dirty();
    void mark_dirty();

private:
    struct Frame {
        ScreenId screen;
        uint8_t cursor;
        uint8_t scroll;
    };

    const Screen& top_screen() const;
    Frame& top_frame();
    const Frame& top_frame() const;

    void push(ScreenId id);
    void pop();
    void move_cursor(int delta);
    void fix_scroll();
    Event select_item();

    const Screen* table_;
    uint8_t screen_count_;
    Settings& settings_;
    uint8_t visible_rows_;

    std::array<Frame, MAX_DEPTH> stack_{};
    uint8_t depth_ = 0;

    // registered handler per ScreenId, nullptr if none
    std::array<CustomScreen*, MAX_SCREENS> custom_{};

    bool dirty_ = true; // draw once at startup
};

} // namespace ui
