#include "ui.h"

#include "custom_screen.h"

namespace ui {

Menu::Menu(const Screen* table, uint8_t screen_count, ScreenId root,
           Settings& settings, uint8_t visible_rows)
    : table_(table),
      screen_count_(screen_count),
      settings_(settings),
      visible_rows_(visible_rows) {
    // TODO: push the root screen
    (void)root;
}

/* *********************  Input  ************************/

Event Menu::handle_input(Input in) {
    // TODO:
    //  - custom screen on top: offer `in` to its registered handler first
    //    (consumed -> mark dirty, return no event)
    //  - list screens: alias Right -> Select, Left -> Back
    //  - custom screens: unconsumed Left -> Back
    //  - dispatch Up/Down/Select/Back
    (void)in;
    return {};
}

void Menu::register_custom(ScreenId id, CustomScreen* screen) {
    // TODO
    (void)id;
    (void)screen;
}

/* *********************  Render Queries  ************************/

ScreenId Menu::current_screen() const {
    // TODO
    return 0;
}

ScreenKind Menu::current_kind() const {
    // TODO
    return ScreenKind::List;
}

const char* Menu::title() const {
    // TODO
    return "";
}

uint8_t Menu::item_count() const {
    // TODO
    return 0;
}

const char* Menu::item_label(uint8_t i) const {
    // TODO
    (void)i;
    return "";
}

const char* Menu::item_value(uint8_t i) const {
    // TODO
    (void)i;
    return nullptr;
}

uint8_t Menu::cursor() const {
    // TODO
    return 0;
}

uint8_t Menu::scroll() const {
    // TODO
    return 0;
}

uint8_t Menu::visible_rows() const {
    return visible_rows_;
}

/* *********************  Redraw Tracking  ************************/

bool Menu::dirty() const {
    return dirty_;
}

void Menu::clear_dirty() {
    dirty_ = false;
}

void Menu::mark_dirty() {
    dirty_ = true;
}

/* *********************  Private Helpers  ************************/

const Screen& Menu::top_screen() const {
    // TODO
    return table_[0];
}

Menu::Frame& Menu::top_frame() {
    // TODO
    return stack_[0];
}

const Menu::Frame& Menu::top_frame() const {
    // TODO
    return stack_[0];
}

void Menu::push(ScreenId id) {
    // TODO: ignore when full, call on_enter for custom screens
    (void)id;
}

void Menu::pop() {
    // TODO: no-op on root, call on_exit for custom screens
}

void Menu::move_cursor(int delta) {
    // TODO: wrap at both ends, no-op on empty screens
    (void)delta;
}

void Menu::fix_scroll() {
    // TODO: keep cursor within [scroll, scroll + visible_rows)
}

Event Menu::select_item() {
    // TODO: Submenu -> push, Action -> event, Setting -> cycle + event
    return {};
}

} // namespace ui
