// Created: 10/24/26
// Updated: 10/26/26
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "boards/adafruit_feather_rp2040.h"

#define I2C_PORT i2c_default
#define I2C_PIN_SDA PICO_DEFAULT_I2C_SDA_PIN
#define I2C_PIN_SCL PICO_DEFAULT_I2C_SCL_PIN

#define BUTTON_UP_PIN 5
#define BUTTON_DOWN_PIN 9

// more docs: https://ssd1306.harbys.me/
#include "libraries/pico-ssd1306/ssd1306.h"
#include "libraries/pico-ssd1306/textRenderer/TextRenderer.h"

/* *********************  Type Definitions  ************************/

typedef enum state_t
{
    MENU,
    ANALYZE,
    SETTINGS
} state_t;

/* *********************  Type Definitions  ************************/

/* *********************  Global Variables  ************************/

pico_ssd1306::SSD1306 display = pico_ssd1306::SSD1306(I2C_PORT, 0x3C, pico_ssd1306::Size::W128xH64);

state_t state = menu;

/* *********************  Global Variables  ************************/

/* *********************  Functions not in a header file yet  ************************/

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

/* *********************  Functions not in a header file yet  ************************/

/* *********************  Program Code  ************************/

int main(void)
{
    i2c_init(I2C_PORT, 400000); // Use i2c port with baud rate of 400khz
    // Set pins for I2C operation
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);

    button_init();

    int cursor_pos = 0;

    while (true)
    {
        display.clear();
        if (state_t == MENU)
        {
            drawText5x8("Please Leave Design 1!\n");
            drawText5x8("Logic Analyzer\n");
            drawText(&display, font_5x8, "Settings\n", 0, 0);
        }
        display.sendBuffer(); // Send buffer to device and show on screen
    }
}

/* *********************  Program Code  ************************/