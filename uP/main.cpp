// Created: 10/24/26
// Writes to a
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"

#define I2C_PORT i2c_default
#define I2C_PIN_SDA PICO_DEFAULT_I2C_SDA_PIN
#define I2C_PIN_SCL PICO_DEFAULT_I2C_SCL_PIN

// more docs: https://ssd1306.harbys.me/
#include "libraries/pico-ssd1306/ssd1306.h"
#include "libraries/pico-ssd1306/textRenderer/TextRenderer.h"
// Turn the led on or off
void pico_set_led(bool led_on)
{
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

int pico_led_init(void)
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

int main(void)
{
    i2c_init(I2C_PORT, 400000); // Use i2c port with baud rate of 400khz
    // Set pins for I2C operation
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);

    // Create a new display object
    pico_ssd1306::SSD1306 display = pico_ssd1306::SSD1306(I2C_PORT, 0x3C, pico_ssd1306::Size::W128xH64);

    // create a vertical line on x: 64 y:0-63
    for (int y = 0; y < 64; y++)
    {
        display.setPixel(64, y);
    }
    display.sendBuffer(); // Send buffer to device and show on screen

    // Draw some text
    // Notice how we first pass the address of display object to the function
    // drawText(&display, font_12x16, "TEST text", 0, 0);
    int x = 0;
    while (true)
    {
        // int rc = pico_led_init();
        // hard_assert(rc == PICO_OK);
        // pico_set_led(true);
        // sleep_ms(500);
        // pico_set_led(false);
        // sleep_ms(500);
        // create a vertical line on x: 64 y:0-63
        display.clear();
        for (int y = 0; y < 64; y++)
        {
            display.setPixel(x, y);
        }
        x++;
        if (x > 128)
        {
            x = 0;
        }
        display.sendBuffer(); // Send buffer to device and show on screen
    }
}