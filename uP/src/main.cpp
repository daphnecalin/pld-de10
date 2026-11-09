// Created: 10/24/26
// Updated: 10/26/26
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <pico/time.h>
#include <stdio.h>

#include "peripheral.h"

constexpr int DEBOUNCE_TIME_US = 40000;

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"

// more docs: https://ssd1306.harbys.me/

/* *********************  Type Definitions  ************************/

typedef enum state_t { MENU, ANALYZE, SETTINGS } state_t;
enum menu_items { MENU_ANALYZE, MENU_SETTINGS, MENUITEM_CT };

/* *********************  Type Definitions  ************************/

/* *********************  Global Variables  ************************/

state_t state = MENU;

/* *********************  Global Variables  ************************/

/* *********************  Functions  ************************/

/* *********************  Functions  ************************/

/* *********************  Program Code  ************************/

int main(void) {
  peripheral_init();

  pico_ssd1306::SSD1306 display =
      pico_ssd1306::SSD1306(I2C_PORT, 0x3C, pico_ssd1306::Size::W128xH64);

  int cursor_pos = 0;
  state_t next_state;

  while (true) {
    display.clear();

    if (state == MENU) {
      if (button_pressed(BTN_DOWN)) {
        cursor_pos = (cursor_pos + 1) % MENUITEM_CT;
      }
      if (button_pressed(BTN_UP)) {
        printf("I am going up");
        cursor_pos = (cursor_pos - 1 + MENUITEM_CT) % MENUITEM_CT;
      }

      if (cursor_pos == MENU_ANALYZE) {
        next_state = ANALYZE;
      } else if (cursor_pos == MENU_SETTINGS) {

        next_state = SETTINGS;
      }

      if (button_pressed(BTN_RIGHT)) {
        printf("I am going right");
        state = next_state;
      } else {
        drawText(&display, font_5x8, "Analyze\n", 10, 0);
        drawText(&display, font_5x8, "Settings\n", 10, 10);

        uint anchor_y = 10 * cursor_pos;
        drawText(&display, font_5x8, ">", 0, anchor_y);
      }
    } else if (state == ANALYZE) {
      printf("I am analyzing");
      drawText(&display, font_5x8, "I am the analyze menu :)\n", 10, 0);
      drawText(&display, font_5x8, "Press LEFT to go back\n", 10, 10);
      if (button_pressed(BTN_LEFT)) {
        state = MENU;
      }
    } else if (state == SETTINGS) {
      printf("%d", gpio_get(7));
      drawText(&display, font_5x8, "I am the settings menu :)\n", 10, 0);
      drawText(&display, font_5x8, "Press LEFT to go back\n", 10, 10);
      if (button_pressed(BTN_LEFT)) {
        printf("I am left");
        state = MENU;
      }
    }
    display.sendBuffer(); // Send buffer to device and show on screen
  }
}

/* *********************  Program Code  ************************/