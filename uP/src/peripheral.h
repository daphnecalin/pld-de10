#ifndef PERIPHERAL_H_
#define PERIPHERAL_H_

#include "../libraries/pico-ssd1306/ssd1306.h"
#include "../libraries/pico-ssd1306/textRenderer/TextRenderer.h"

#define I2C_PORT i2c_default
#define I2C_PIN_SDA PICO_DEFAULT_I2C_SDA_PIN
#define I2C_PIN_SCL PICO_DEFAULT_I2C_SCL_PIN

#define BUTTON_UP_PIN 10
#define BUTTON_DOWN_PIN 9
#define BUTTON_RIGHT_PIN 6
#define BUTTON_LEFT_PIN 5

int button_init(void);
void drawText5x8(const char *text, pico_ssd1306::SSD1306 display);
int peripheral_init(void);

#endif