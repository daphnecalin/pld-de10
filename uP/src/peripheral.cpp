#include "peripheral.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"
#include <pico/time.h>

// keep gpio pins internalvesk
#define BUTTON_UP_PIN 10
#define BUTTON_DOWN_PIN 9
#define BUTTON_RIGHT_PIN 8
#define BUTTON_LEFT_PIN 7

#define DEBOUNCE_MS 20

static const uint button_pins[BTN_COUNT] = {BUTTON_DOWN_PIN, BUTTON_UP_PIN,
                                            BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN};

static volatile bool pressed_flag[BTN_COUNT];

void drawText5x8(const char *text, pico_ssd1306::SSD1306 *display) {
  drawText(display, font_5x8, text, 0, 0);
}

static int64_t debounce_alarm(alarm_id_t id, void *user_data) {
  int btn_id = (int)(intptr_t)user_data;
  uint pin = button_pins[btn_id];

  if (!gpio_get(pin)) {
    pressed_flag[btn_id] = true;
  }

  gpio_acknowledge_irq(pin, GPIO_IRQ_EDGE_FALL);
  gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL, true);
  return 0;
}

static void button_callback(uint gpio, uint32_t events) {
  // button index e.g. BUTTON_DOWN's id is 0
  int id = -1;
  for (int i = 0; i < BTN_COUNT; i++)
    if (button_pins[i] == gpio)
      id = i;
  if (id < 0) {
    return;
  }

  // disable interrupts until the debounce period is over
  gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, false);
  // set an alarm that runs after debounce period, pass the button index
  add_alarm_in_ms(DEBOUNCE_MS, debounce_alarm, (void *)(intptr_t)id, false);
}

// Set GPIO pins for button navigation
int button_init(void) {
  uint32_t button_mask = (1 << BUTTON_DOWN_PIN) | (1 << BUTTON_UP_PIN) |
                         (1 << BUTTON_LEFT_PIN) | (1 << BUTTON_RIGHT_PIN);
  gpio_init_mask(button_mask);
  gpio_set_dir_in_masked(button_mask);

  gpio_set_irq_enabled_with_callback(button_pins[0], GPIO_IRQ_EDGE_FALL, true,
                                     &button_callback);
  for (int i = 1; i < BTN_COUNT; i++)
    gpio_set_irq_enabled(button_pins[i], GPIO_IRQ_EDGE_FALL, true);

  return PICO_OK;
}

// check if a button was pressed and return it to false once checked
bool button_pressed(int id) {
  if (id >= BTN_COUNT || id < 0) {
    return false;
  }
  if (pressed_flag[id]) {
    pressed_flag[id] = false;
    return true;
  }
  return false;
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
  stdio_init_all();
  return PICO_OK;
}
