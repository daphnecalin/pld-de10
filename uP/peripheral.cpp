#include "peripheral.h"

void drawText5x8(const char *text)
{
    drawText(&display, font_5x8, text, 0, 0);
}

// Set GPIO pins for button navigation
int button_init(void)
{
    gpio_init(BUTTON_DOWN_PIN);
    gpio_init(BUTTON_UP_PIN);
    gpio_set_dir(BUTTON_DOWN_PIN, GPIO_IN);
    gpio_set_dir(BUTTON_UP_PIN, GPIO_IN);
    // TODO: choose pull-up or pull-down
    return PICO_OK;
}
