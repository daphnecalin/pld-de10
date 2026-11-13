#include "peripheral.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"
#include <pico/time.h>

static const uint btn_pins[hw::BTN_COUNT] = {hw::BTN_DOWN_PIN, hw::BTN_UP_PIN, hw::BTN_LEFT_PIN,
                                             hw::BTN_RIGHT_PIN};

static volatile bool pressed_flag[hw::BTN_COUNT];

void hw::drawText5x8(const char *text, pico_ssd1306::SSD1306 *display)
{
  drawText(display, font_5x8, text, 0, 0);
}

static int64_t debounce_alarm(alarm_id_t id, void *user_data)
{
  int btn_id = (int)(intptr_t)user_data;
  uint pin = btn_pins[btn_id];

  if (!gpio_get(pin))
  {
    pressed_flag[btn_id] = true;
  }

  gpio_acknowledge_irq(pin, GPIO_IRQ_EDGE_FALL);
  gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL, true);
  return 0;
}

static void btn_callback(uint gpio, uint32_t events)
{
  // btn index e.g. btn_DOWN's id is 0
  int id = -1;
  for (int i = 0; i < hw::BTN_COUNT; i++)
    if (btn_pins[i] == gpio)
      id = i;
  if (id < 0)
  {
    return;
  }

  // disable interrupts until the debounce period is over
  gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, false);
  // set an alarm that runs after debounce period, pass the btn index
  add_alarm_in_ms(hw::DEBOUNCE_MS, debounce_alarm, (void *)(intptr_t)id, false);
}

// Set GPIO pins for button navigation
int hw::btn_init(void)
{
  uint32_t btn_mask = (1 << BTN_DOWN_PIN) | (1 << BTN_UP_PIN) |
                      (1 << BTN_LEFT_PIN) | (1 << BTN_RIGHT_PIN);
  gpio_init_mask(btn_mask);
  gpio_set_dir_in_masked(btn_mask);

  gpio_set_irq_enabled_with_callback(btn_pins[0], GPIO_IRQ_EDGE_FALL, true,
                                     &btn_callback);
  for (int i = 1; i < BTN_COUNT; i++)
    gpio_set_irq_enabled(btn_pins[i], GPIO_IRQ_EDGE_FALL, true);

  return PICO_OK;
}

// check if a btn was pressed and return it to false once checked
bool hw::btn_pressed(int id)
{
  if (id >= BTN_COUNT || id < 0)
  {
    return false;
  }
  if (pressed_flag[id])
  {
    pressed_flag[id] = false;
    return true;
  }
  return false;
}

int hw::screen_init(void)
{
  i2c_init(I2C_PORT, 400000); // Use i2c port with baud rate of 400khz
  // Set pins for I2C operation
  gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_PIN_SDA);
  gpio_pull_up(I2C_PIN_SCL);
  return PICO_OK;
}

int hw::peripheral_init(void)
{
  // TODO: add error handling
  btn_init();
  screen_init();
  stdio_init_all();
  return PICO_OK;
}

// Example: https://github.com/raspberrypi/pico-examples/blob/master/spi/spi_master_slave/spi_master/spi_master.c
int hw::spi_setup(void)
{
  // set up spi module connected to de10
  // set up gpio interrupt for rising edge to initiate transmission
  spi_init(spi_default, 1000 * 1000);
  gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

  // the chip select needs to be driven manually
  // set high while idling
  gpio_init(hw::SPI_PIN_CS);
  gpio_set_dir(hw::SPI_PIN_CS, GPIO_OUT);
  gpio_put(hw::SPI_PIN_CS, 1);

  gpio_init(hw::DATA_READY_PIN);
  gpio_set_input_enabled(hw::DATA_READY_PIN);
  gpio_add_raw_irq_handler(hw::DATA_READY_PIN, transfer::data_ready);
  gpio_set_irq_enabled(hw::DATA_READY_PIN, GPIO_IRQ_EDGE_RISE, true);

  return PICO_OK;
}