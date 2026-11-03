#include "peripheral.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"
#include <pico/time.h>

void drawText5x8(const char *text, pico_ssd1306::SSD1306 display) {
  drawText(&display, font_5x8, text, 0, 0);
}

// Set GPIO pins for button navigation
int button_init(void) {
  uint32_t button_mask = (1 << BUTTON_DOWN_PIN) | (1 << BUTTON_UP_PIN) |
                         (1 << BUTTON_LEFT_PIN) | (1 << BUTTON_RIGHT_PIN);
  gpio_init_mask(button_mask);
  gpio_set_dir_in_masked(button_mask);
  return PICO_OK;
}

int screen_init(void) {
  i2c_init(I2C_PORT, 400000); // Use i2c port with baud rate of 400khz
  // Set pins for I2C operation
  gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_PIN_SDA);
  gpio_pull_up(I2C_PIN_SCL);
  return PICO_OK;
}

int peripheral_init(void) {
  // TODO: add error handling
  button_init();
  screen_init();
  return PICO_OK;
}
