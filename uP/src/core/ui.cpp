#include "ui.h"
#include "custom_screen.h"

#include <cassert>

namespace ui {

Menu::Menu(const Screen* table, uint8_t screen_count, ScreenId root,
           Settings& settings, uint8_t visible_rows)
    : table_(table),
      screen_count_(screen_count),
      settings_(settings),
      visible_rows_(visible_rows) {
    // custom_ is indexed by ScreenId, so every id must fit
    assert(screen_count_ <= MAX_SCREENS);
    push(root);
}

/* *********************  Input  ************************/

Event Menu::handle_input(Input in) {
    const Screen& screen = top_screen();

    if(screen.kind == ScreenKind::Custom) {
        // give input to the custom screen's handler first
        CustomScreen* handler = custom_[screen.id];
        if(handler != nullptr && handler->handle_input(in)) {
            mark_dirty();
            return {};
        }
        // unconsumed: only back/left does anything (pops the screen)
        if(in != Input::Left && in != Input::Back) return {};
    }

    // list screens alias left -> back and right -> select
    switch(in) {
        case Input::Up:
            move_cursor(-1);
            return {};

        case Input::Down:
            move_cursor(1);
            return {};

        case Input::Left:
        case Input::Back:
            pop();
            return {};

        case Input::Right:
        case Input::Select:
            return select_item();
    }

    return {};
}

void Menu::register_custom(ScreenId id, CustomScreen* screen) {
    assert(id < screen_count_);
    if(id >= screen_count_) return;
    custom_[id] = screen;
}

/* *********************  Render Queries  ************************/

ScreenId Menu::current_screen() const {
    return top_screen().id;
}

ScreenKind Menu::current_kind() const {
    return top_screen().kind;
}

const char* Menu::title() const {
    return top_screen().title;
}

uint8_t Menu::item_count() const {
    return top_screen().item_count;
}

const char* Menu::item_label(uint8_t i) const {
    const Screen& s = top_screen();
    if(i >= s.item_count) return "";
    return s.items[i].label;
}

const char* Menu::item_value(uint8_t i) const {
    const Screen& s = top_screen();
    // make sure item is a setting
    if(i >= s.item_count) return nullptr;
    if(s.items[i].kind != ItemKind::Setting) return nullptr;
    // get settingid
    SettingId id = static_cast<SettingId>(s.items[i].target);
    return settings_.value_text(id);
}

uint8_t Menu::cursor() const {
    return top_frame().cursor;
}

uint8_t Menu::scroll() const {
    return top_frame().scroll;
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
    return table_[top_frame().screen];
}

Menu::Frame& Menu::top_frame() {
    return stack_[depth_ - 1];
}

const Menu::Frame& Menu::top_frame() const {
    return stack_[depth_ - 1];
}

void Menu::push(ScreenId id) {
    // a bad id means a typo in a table's target
    assert(id < screen_count_);
    if(id >= screen_count_) return;
    if(depth_ == MAX_DEPTH) return;
    // call on_enter for custom screens
    if(table_[id].kind == ScreenKind::Custom && custom_[id] != nullptr) {
        custom_[id]->on_enter();
    }
    // then push new frame to stack
    stack_[depth_] = {id, 0, 0};
    depth_++;

    mark_dirty();
}

void Menu::pop() {
    if(depth_ == 1) return;
    // call on_exit for custom screens
    ScreenId id = top_screen().id;
    if(top_screen().kind == ScreenKind::Custom && custom_[id] != nullptr) {
        custom_[id]->on_exit();
    }
    // decrement depth, no need to remove entry technically
    depth_--;

    mark_dirty();
}

// delta is expected to be +1 or -1
void Menu::move_cursor(int delta) {
    const uint8_t count = top_screen().item_count;
    if(count == 0) return;

    Frame& f = top_frame();
    // adding count before the modulo keeps the value non-negative,
    // so moving up from 0 wraps to count - 1
    const uint8_t next = static_cast<uint8_t>((f.cursor + delta + count) % count);
    // single-item list: wrapping lands on the same item, nothing to redraw
    if(next == f.cursor) return;

    f.cursor = next;
    fix_scroll();
    mark_dirty();
}

void Menu::fix_scroll() {
    if(visible_rows_ == 0) return;

    Frame& f = top_frame();
    if(f.cursor < f.scroll) {
        // cursor went above the window (moving up, or wrapping to the top)
        f.scroll = f.cursor;
    } else if(f.cursor >= f.scroll + visible_rows_) {
        // cursor went below the window (moving down, or wrapping to the bottom),
        // so make it the last visible row
        f.scroll = f.cursor - visible_rows_ + 1;
    }
}

Event Menu::select_item() {
    const Screen& screen = top_screen();
    if(screen.item_count == 0) return {};
    const Item& item = screen.items[top_frame().cursor];
    switch(item.kind) {
        // push submenu (screen id from target)
        case ItemKind::Submenu:
            push(item.target);
            return {};

        // let main.cpp handle the action
        case ItemKind::Action:
            return {EventType::Action, item.target};

        // call settings cycle, return dirty
        case ItemKind::Setting:
            settings_.cycle(static_cast<SettingId>(item.target));
            mark_dirty();
            return {EventType::SettingChanged, item.target};
    }

    return {};
}

} // namespace ui
