#ifndef PERIPHERAL_H_
#define PERIPHERAL_H_

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"

constexpr i2c_inst_t *I2C_PORT = i2c_default;
constexpr int I2C_PIN_SDA = PICO_DEFAULT_I2C_SDA_PIN;
constexpr int I2C_PIN_SCL = PICO_DEFAULT_I2C_SCL_PIN;

constexpr int BUTTON_UP_PIN = 10;
constexpr int BUTTON_DOWN_PIN = 9;
constexpr int BUTTON_RIGHT_PIN = 6;
constexpr int BUTTON_LEFT_PIN = 5;

enum button_id
{
    BTN_DOWN,
    BTN_UP,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_COUNT
};

int button_init(void);
bool button_pressed(int id);
void drawText5x8(const char *text, pico_ssd1306::SSD1306 *display);
int peripheral_init(void);

#endif