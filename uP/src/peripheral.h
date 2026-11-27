#ifndef PERIPHERAL_H_
#define PERIPHERAL_H_

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"
#include <hardware/spi.h>

#define SPI_PORT spi_default
namespace hw
{

    constexpr i2c_inst_t *I2C_PORT = i2c_default;
    constexpr int I2C_PIN_SDA = PICO_DEFAULT_I2C_SDA_PIN;
    constexpr int I2C_PIN_SCL = PICO_DEFAULT_I2C_SCL_PIN;

    constexpr int SPI_PIN_CS = 12;

    constexpr int BTN_UP_PIN = 10; // TODO may be wrong
    constexpr int BTN_DOWN_PIN = 9;
    constexpr int BTN_RIGHT_PIN = 8;
    constexpr int BTN_LEFT_PIN = 7;

    constexpr int DATA_READY_PIN = 11;
    constexpr int DEBOUNCE_MS = 20;

    enum btn_id
    {
        BTN_DOWN,
        BTN_UP,
        BTN_LEFT,
        BTN_RIGHT,
        BTN_COUNT
    };

    int btn_init(void);
    bool btn_pressed(int id);
    void drawText5x8(const char *text, pico_ssd1306::SSD1306 *display);
    int peripheral_init(void);
    int screen_init(void);
    int spi_setup(void);
} // namespace hw
#endif