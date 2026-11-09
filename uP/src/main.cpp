// Created: 10/24/26
// Updated: 10/26/26
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <pico/time.h>

#include "peripheral.h"

constexpr int DEBOUNCE_TIME_US = 40000;

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"

// more docs: https://ssd1306.harbys.me/

/* *********************  Type Definitions  ************************/

typedef enum state_t { MENU, ANALYZE, SETTINGS } state_t;

/* *********************  Type Definitions  ************************/

/* *********************  Global Variables  ************************/

pico_ssd1306::SSD1306 display =
    pico_ssd1306::SSD1306(I2C_PORT, 0x3C, pico_ssd1306::Size::W128xH64);
state_t state = MENU;

/* *********************  Global Variables  ************************/

/* *********************  Functions  ************************/

/* *********************  Functions  ************************/

/* *********************  Program Code  ************************/

int main(void) {
  peripheral_init();

  int cursor_pos = 0;
  bool up_b, down_b, left_b, right_b = 0;
  absolute_time_t last_up_t, last_down_t, last_left_t, last_right_t = 0;

  while (true) {
    display.clear();

    // get buttons every 40 ms
    if (get_absolute_time() - last_up_t > DEBOUNCE_TIME_US) {
      up_b = gpio_get(BUTTON_UP_PIN);
      last_up_t = get_absolute_time();
    }
    if (get_absolute_time() - last_down_t > DEBOUNCE_TIME_US) {
      down_b = gpio_get(BUTTON_DOWN_PIN);
      last_down_t = get_absolute_time();
    }
    if (get_absolute_time() - last_left_t > DEBOUNCE_TIME_US) {
      left_b = gpio_get(BUTTON_LEFT_PIN);
      last_left_t = get_absolute_time();
    }
    if (get_absolute_time() - last_right_t > DEBOUNCE_TIME_US) {
      right_b = gpio_get(BUTTON_RIGHT_PIN);
      last_right_t = get_absolute_time();
    }

    if (state == MENU) {

      if (up_b == 0) {
        drawText5x8("Please Leave Design 1!\n", display);
      } else {
        drawText5x8("Logic Analyzer\n", display);
      }

      drawText(&display, font_5x8, "Settings\n", 0, 0);
    } else if (state == ANALYZE) {
    } else if (state == SETTINGS) {
    }
    display.sendBuffer(); // Send buffer to device and show on screen
  }
}

/* *********************  Program Code  ************************/