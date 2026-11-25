#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "transfer.h"
#include "packet.h"
#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"

namespace {
    using pico_ssd1306::SSD1306;
    using pico_ssd1306::font_5x8;

    packet::Packet latest_packet{};
    bool have_packet = false;
    transfer::CommandStatus latest_command_status = transfer::CommandStatus::None;

    uint8_t control_width(uint8_t id) {
        switch (id) {
            case 1:
            case 2: return 1;
            case 4: return 4;
            case 5:
            case 6: return 2;
            case 7: return 3;
            default: return 0;
        }
    }

    uint32_t signal_value(uint8_t id) {
        if (!have_packet) return 0;
        for (uint8_t i = 0; i < latest_packet.signal_count; ++i) {
            if (latest_packet.signals[i].id == id)
                return latest_packet.signals[i].value;
        }
        return 0;
    }

    void draw_screen(SSD1306& display) {
        char line[32];
        display.clear();
        pico_ssd1306::drawText(&display, font_5x8, "RALU DEBUG", 0, 0);
        std::snprintf(line, sizeof(line), "IN:%X A:%X B:%X", signal_value(4), signal_value(5), signal_value(6));
        pico_ssd1306::drawText(&display, font_5x8, line, 0, 9);
        std::snprintf(line, sizeof(line), "SEL:%X CIN:%X", signal_value(7), signal_value(2));
        pico_ssd1306::drawText(&display, font_5x8, line, 0, 18);
        std::snprintf(line, sizeof(line), "OUT:%X COUT:%X", signal_value(10), signal_value(3));
        pico_ssd1306::drawText(&display, font_5x8, line, 0, 27);
        std::snprintf(line, sizeof(line), "REG A:%X B:%X", signal_value(8), signal_value(9));
        pico_ssd1306::drawText(&display, font_5x8, line, 0, 36);
        std::snprintf(line, sizeof(line), "CMD STATUS:%u", static_cast<unsigned>(latest_command_status));
        pico_ssd1306::drawText(&display, font_5x8, line, 0, 45);
        pico_ssd1306::drawText(&display, font_5x8, "USB: set ID VAL", 0, 54);
        display.sendBuffer();
    }

    void handle_console_line(char* line) {
        unsigned id = 0;
        unsigned value = 0;
        if (std::sscanf(line, "set %u %x", &id, &value) == 2) {
            const uint8_t width = control_width(static_cast<uint8_t>(id));
            if (width == 0) {
                std::printf("Writable IDs: 1, 2, 4, 5, 6, 7\n");
                return;
            }
            latest_command_status = transfer::set_control(static_cast<uint8_t>(id), width, value);
            const bool ok = latest_command_status == transfer::CommandStatus::Ok;
            std::printf("set id=%u value=0x%X: %s (status %u)\n", id, value,
                        ok ? "OK" : "FAILED", static_cast<unsigned>(latest_command_status));
        } else if (std::sscanf(line, "clear %u", &id) == 1) {
            const uint8_t width = control_width(static_cast<uint8_t>(id));
            if (width == 0) {
                std::printf("Writable IDs: 1, 2, 4, 5, 6, 7\n");
                return;
            }
            latest_command_status = transfer::clear_control_override(static_cast<uint8_t>(id), width);
            const bool ok = latest_command_status == transfer::CommandStatus::Ok;
            std::printf("clear id=%u: %s\n", id, ok ? "OK" : "FAILED");
        } else {
            std::printf("Commands: set <id> <hex-value>, clear <id>\n");
        }
    }
}

int main() {
    stdio_init_all();
    i2c_init(i2c_default, 400'000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    sleep_ms(250);

    SSD1306 display(i2c_default, 0x3D, pico_ssd1306::Size::W128xH64);
    transfer::spi_setup();

    std::printf("RALU backpack ready\n");
    std::printf("Commands: set <id> <hex-value>, clear <id>\n");
    std::printf("Writable IDs: 1=S_L, 2=CIN, 4=Input, 5=MSA, 6=MSB, 7=MSC\n");

    char input_line[48]{};
    size_t input_length = 0;
    draw_screen(display);

    while (true) {
        if (gpio_get(PLD_DATA_READY_PIN)) {
            const packet::Frame frame = transfer::spi_receive();
            const auto parsed = packet::Packet::parse(frame);
            if (parsed) {
                latest_packet = *parsed;
                have_packet = true;
                draw_screen(display);
            } else {
                std::printf("Received invalid telemetry frame\n");
            }
        }

        const int ch = getchar_timeout_us(0);
        if (ch == '\r' || ch == '\n') {
            if (input_length != 0) {
                input_line[input_length] = '\0';
                handle_console_line(input_line);
                input_length = 0;
                draw_screen(display);
            }
        } else if (ch >= 0 && input_length < sizeof(input_line) - 1) {
            input_line[input_length++] = static_cast<char>(ch);
        }
        tight_loop_contents();
    }
}
