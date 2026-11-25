#pragma once

#include <array>
#include <cstdint>

namespace ui {
using std::uint8_t;

// used as index into SettingDef
enum SettingId : uint8_t {
    SETTING_TRIGGER_EDGE,
    SETTING_CONTINUOUS,
    SETTING_COUNT //
};

// every setting is a choice from a fixed list of strings
// a bool setting is just {"Off", "On"}
struct SettingDef {
    SettingId id;
    const char* const* choices;
    uint8_t choice_count;
    uint8_t default_index;
};

extern const SettingDef SETTING_DEFS[SETTING_COUNT];

// current value (choice index) of every setting
class Settings {
public:
    Settings();

    // get value associated with setting id
    uint8_t get(SettingId id) const;
    // advance to the next choice, wrapping back to the first
    void cycle(SettingId id);
    // display string for the current choice
    const char* value_text(SettingId id) const;
    // reset all settings to their default value
    void reset_defaults();

private:
    std::array<uint8_t, SETTING_COUNT> values_{};
};
} // namespace ui
