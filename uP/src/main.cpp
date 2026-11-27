// Created: 10/24/26
// Updated: 11/27/26
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <pico/time.h>
#include <cstdio>
#include <cstring>

#include "peripheral.h"
#include "transfer.h"
#include "core/packet.h"

constexpr int DEBOUNCE_TIME_US = 40000;

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"

// more docs: https://ssd1306.harbys.me/

/* *********************  Type Definitions  ************************/

typedef enum state_t
{
  MENU,
  ANALYZE,
  SETTINGS
} state_t;

enum menu_items
{
  MENU_ANALYZE,
  MENU_SETTINGS,
  MENUITEM_CT
};

/* *********************  Type Definitions  ************************/

/* *********************  Global Variables  ************************/

state_t state = MENU;

namespace
{
  packet::Packet latest_packet{};
  bool have_packet = false;
  transfer::CommandStatus latest_command_status = transfer::CommandStatus::None;

  char input_line[48]{};
  size_t input_length = 0;
}

/* *********************  Global Variables  ************************/

/* *********************  Functions  ************************/

namespace
{
  uint8_t control_width(uint8_t id)
  {
    switch (id)
    {
    case 1:
    case 2:
      return 1;
    case 4:
      return 4;
    case 5:
    case 6:
      return 2;
    case 7:
      return 3;
    default:
      return 0;
    }
  }

  uint32_t signal_value(uint8_t id)
  {
    if (!have_packet)
      return 0;
    for (uint8_t i = 0; i < latest_packet.signal_count; ++i)
    {
      if (latest_packet.signals[i].id == id)
        return latest_packet.signals[i].value;
    }
    return 0;
  }

  // Draws the analyze view into the frame buffer. The main loop owns
  // display.clear() / display.sendBuffer().
  void draw_analyze_screen(pico_ssd1306::SSD1306 &display)
  {
    char line[32];
    drawText(&display, font_5x8, "RALU DEBUG  <L:back", 0, 0);
    std::snprintf(line, sizeof(line), "IN:%lX A:%lX B:%lX", (unsigned long)signal_value(4),
                  (unsigned long)signal_value(5), (unsigned long)signal_value(6));
    drawText(&display, font_5x8, line, 0, 9);
    std::snprintf(line, sizeof(line), "SEL:%lX CIN:%lX", (unsigned long)signal_value(7),
                  (unsigned long)signal_value(2));
    drawText(&display, font_5x8, line, 0, 18);
    std::snprintf(line, sizeof(line), "OUT:%lX COUT:%lX", (unsigned long)signal_value(10),
                  (unsigned long)signal_value(3));
    drawText(&display, font_5x8, line, 0, 27);
    std::snprintf(line, sizeof(line), "REG A:%lX B:%lX", (unsigned long)signal_value(8),
                  (unsigned long)signal_value(9));
    drawText(&display, font_5x8, line, 0, 36);
    std::snprintf(line, sizeof(line), "CMD STATUS:%u", static_cast<unsigned>(latest_command_status));
    drawText(&display, font_5x8, line, 0, 45);
    drawText(&display, font_5x8, "USB: set ID VAL", 0, 54);
  }

  void handle_console_line(char *line)
  {
    unsigned id = 0;
    unsigned value = 0;
    if (std::sscanf(line, "set %u %x", &id, &value) == 2)
    {
      const uint8_t width = control_width(static_cast<uint8_t>(id));
      if (width == 0)
      {
        std::printf("Writable IDs: 1, 2, 4, 5, 6, 7\n");
        return;
      }
      latest_command_status = transfer::set_control(static_cast<uint8_t>(id), width, value);
      const bool ok = latest_command_status == transfer::CommandStatus::Ok;
      std::printf("set id=%u value=0x%X: %s (status %u)\n", id, value,
                  ok ? "OK" : "FAILED", static_cast<unsigned>(latest_command_status));
    }
    else if (std::sscanf(line, "clear %u", &id) == 1)
    {
      const uint8_t width = control_width(static_cast<uint8_t>(id));
      if (width == 0)
      {
        std::printf("Writable IDs: 1, 2, 4, 5, 6, 7\n");
        return;
      }
      latest_command_status = transfer::clear_control_override(static_cast<uint8_t>(id), width);
      const bool ok = latest_command_status == transfer::CommandStatus::Ok;
      std::printf("clear id=%u: %s\n", id, ok ? "OK" : "FAILED");
    }
    else
    {
      std::printf("Commands: set <id> <hex-value>, clear <id>\n");
    }
  }

  // Poll the PLD for a new telemetry frame and store it if valid.
  void poll_telemetry(void)
  {
    if (!gpio_get(PLD_DATA_READY_PIN))
      return;

    const packet::Frame frame = transfer::spi_receive();
    const auto parsed = packet::Packet::parse(frame);
    if (parsed)
    {
      latest_packet = *parsed;
      have_packet = true;
    }
    else
    {
      std::printf("Received invalid telemetry frame\n");
    }
  }

  // Non-blocking read of the USB console; dispatches complete lines.
  void poll_console(void)
  {
    const int ch = getchar_timeout_us(0);
    if (ch == '\r' || ch == '\n')
    {
      if (input_length != 0)
      {
        input_line[input_length] = '\0';
        handle_console_line(input_line);
        input_length = 0;
      }
    }
    else if (ch >= 0 && input_length < sizeof(input_line) - 1)
    {
      input_line[input_length++] = static_cast<char>(ch);
    }
  }
}

/* *********************  Functions  ************************/

/* *********************  Program Code  ************************/

int main(void)
{
  hw::peripheral_init();

  pico_ssd1306::SSD1306 display =
      pico_ssd1306::SSD1306(hw::I2C_PORT, 0x3C, pico_ssd1306::Size::W128xH64);

  int cursor_pos = 0;
  state_t next_state = ANALYZE;

  while (true)
  {
    display.clear();

    if (state == MENU)
    {
      if (hw::btn_pressed((hw::BTN_DOWN)))
      {
        cursor_pos = (cursor_pos + 1) % MENUITEM_CT;
      }
      if (hw::btn_pressed((hw::BTN_UP)))
      {
        cursor_pos = (cursor_pos - 1 + MENUITEM_CT) % MENUITEM_CT;
      }

      if (cursor_pos == MENU_ANALYZE)
      {
        next_state = ANALYZE;
      }
      else if (cursor_pos == MENU_SETTINGS)
      {
        next_state = SETTINGS;
      }

      if (hw::btn_pressed((hw::BTN_RIGHT)))
      {
        state = next_state;
        if (state == ANALYZE)
        {
          std::printf("RALU backpack ready\n");
          std::printf("Commands: set <id> <hex-value>, clear <id>\n");
          std::printf("Writable IDs: 1=S_L, 2=CIN, 4=Input, 5=MSA, 6=MSB, 7=MSC\n");
        }
      }
      else
      {
        drawText(&display, font_5x8, "Analyze\n", 10, 0);
        drawText(&display, font_5x8, "Settings\n", 10, 10);

        uint anchor_y = 10 * cursor_pos;
        drawText(&display, font_5x8, ">", 0, anchor_y);
      }
    }
    else if (state == ANALYZE)
    {
      poll_telemetry();
      poll_console();
      draw_analyze_screen(display);

      if (hw::btn_pressed((hw::BTN_LEFT)))
      {
        state = MENU;
      }
    }
    else if (state == SETTINGS)
    {
      drawText(&display, font_5x8, "I am the settings menu :)\n", 10, 0);
      drawText(&display, font_5x8, "Press LEFT to go back\n", 10, 10);
      if (hw::btn_pressed((hw::BTN_LEFT)))
      {
        state = MENU;
      }
    }
    display.sendBuffer(); // Send buffer to device and show on screen
  }
}

/* *********************  Program Code  ************************/
