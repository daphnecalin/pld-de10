#ifndef PERIPHERAL_H_
#define PERIPHERAL_H_

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"

namespace hw
{

    constexpr i2c_inst_t *I2C_PORT = i2c_default;
    constexpr int I2C_PIN_SDA = PICO_DEFAULT_I2C_SDA_PIN;
    constexpr int I2C_PIN_SCL = PICO_DEFAULT_I2C_SCL_PIN;

    constexpr int BTN_UP_PIN = 10;
    constexpr int BTN_DOWN_PIN = 9;
    constexpr int BTN_RIGHT_PIN = 6;
    constexpr int BTN_LEFT_PIN = 5;

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
    int spi_init(void);
} // namespace hw
#endif