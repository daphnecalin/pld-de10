#include "settings.h"

#include <iterator>

using std::uint8_t;

namespace ui {

/* *********************  Setting Definitions  ************************/

constexpr const char* TRIGGER_EDGE_CHOICES[] = {"Rising", "Falling", "Both"};
constexpr const char* ON_OFF_CHOICES[] = {"Off", "On"};

// must be in the same order as SettingId
constexpr SettingDef SETTING_DEFS[] = {
    {SETTING_TRIGGER_EDGE, TRIGGER_EDGE_CHOICES, std::size(TRIGGER_EDGE_CHOICES), 0},
    {SETTING_CONTINUOUS,   ON_OFF_CHOICES,       std::size(ON_OFF_CHOICES),       0},
};

static_assert(std::size(SETTING_DEFS) == SETTING_COUNT,
              "SETTING_DEFS must have one entry per SettingId");

constexpr bool setting_defs_in_order() {
    for (uint8_t i = 0; i < SETTING_COUNT; i++) {
        if (SETTING_DEFS[i].id != i) return false;
    }
    return true;
}
static_assert(setting_defs_in_order(), "SETTING_DEFS order must match SettingId");

/* *********************  Settings  ************************/

Settings::Settings() {
    reset_defaults();
}

uint8_t Settings::get(SettingId id) const {
    return values_[id];
}

void Settings::cycle(SettingId id) {
    const SettingDef& setting = SETTING_DEFS[id];
    values_[id] = (values_[id] + 1) % setting.choice_count;
}

const char* Settings::value_text(SettingId id) const {
    const SettingDef& setting = SETTING_DEFS[id];
    return setting.choices[values_[id]];
}

void Settings::reset_defaults() {
    for(uint8_t i = 0; i < SETTING_COUNT; i++) {
        const SettingDef& setting = SETTING_DEFS[i];
        values_[i] = setting.default_index;
    }
}

} // namespace ui
