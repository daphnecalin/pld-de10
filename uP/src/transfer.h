#pragma once

#include "core/packet.h"

// Default Raspberry Pi Pico SPI0 wiring. Override these definitions in the
// board project if the backpack PCB uses different GPIOs.
#ifndef PLD_SPI_MISO_PIN
#define PLD_SPI_MISO_PIN 16
#endif
#ifndef PLD_SPI_CS_PIN
#define PLD_SPI_CS_PIN 17
#endif
#ifndef PLD_SPI_SCK_PIN
#define PLD_SPI_SCK_PIN 18
#endif
#ifndef PLD_SPI_MOSI_PIN
#define PLD_SPI_MOSI_PIN 19
#endif
#ifndef PLD_DATA_READY_PIN
#define PLD_DATA_READY_PIN 20
#endif
#ifndef PLD_COMMAND_DONE_PIN
#define PLD_COMMAND_DONE_PIN 21
#endif
#ifndef PLD_COMMAND_STATUS0_PIN
#define PLD_COMMAND_STATUS0_PIN 22
#endif
#ifndef PLD_COMMAND_STATUS1_PIN
#define PLD_COMMAND_STATUS1_PIN 23
#endif
#ifndef PLD_COMMAND_STATUS2_PIN
#define PLD_COMMAND_STATUS2_PIN 24
#endif

namespace transfer
{
    enum class CommandStatus : uint8_t
    {
        None = 0,
        Ok = 1,
        BadFrame = 2,
        BadId = 3,
        BadWidth = 4,
        BadValue = 5,
        BadOpcode = 6
    };

    void spi_setup(void);
    packet::Frame spi_receive(void);
    CommandStatus spi_command(const packet::ControlCommand &command);
    CommandStatus set_control(uint8_t id, uint8_t width, uint32_t value);
    CommandStatus clear_control_override(uint8_t id, uint8_t width);
}
