#pragma once

#include "ui.h"

namespace ui {

// interface for screens that aren't a list of items (e.g. waveform viewer)
// each implementation owns its own view state (scroll, zoom, ...)
class CustomScreen {
public:
    virtual ~CustomScreen() = default;

    // called before Menu handles the input itself
    // return true if the input was consumed, false to let Menu handle it
    // (an unconsumed Back/Left pops the screen)
    virtual bool handle_input(Input in) = 0;

    // called when the screen is pushed onto / popped off the stack
    virtual void on_enter() {}
    virtual void on_exit() {}
};

} // namespace ui
