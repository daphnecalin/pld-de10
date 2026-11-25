#include "transfer.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace transfer {
    namespace {
        constexpr uint32_t SPI_BAUD = 1'000'000;
        constexpr uint32_t COMMAND_TIMEOUT_MS = 100;
        uint8_t next_sequence = 0;

        void select_fpga() {
            gpio_put(PLD_SPI_CS_PIN, 0);
            sleep_us(2);
        }

        void deselect_fpga() {
            sleep_us(2);
            gpio_put(PLD_SPI_CS_PIN, 1);
            sleep_us(5);
        }

        CommandStatus read_command_status() {
            return static_cast<CommandStatus>(
                (gpio_get(PLD_COMMAND_STATUS0_PIN) ? 1u : 0u) |
                (gpio_get(PLD_COMMAND_STATUS1_PIN) ? 2u : 0u) |
                (gpio_get(PLD_COMMAND_STATUS2_PIN) ? 4u : 0u));
        }
    }

    void spi_setup(void) {
        spi_init(spi0, SPI_BAUD);
        spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        gpio_set_function(PLD_SPI_MISO_PIN, GPIO_FUNC_SPI);
        gpio_set_function(PLD_SPI_SCK_PIN, GPIO_FUNC_SPI);
        gpio_set_function(PLD_SPI_MOSI_PIN, GPIO_FUNC_SPI);

        gpio_init(PLD_SPI_CS_PIN);
        gpio_set_dir(PLD_SPI_CS_PIN, GPIO_OUT);
        gpio_put(PLD_SPI_CS_PIN, 1);

        gpio_init(PLD_DATA_READY_PIN);
        gpio_set_dir(PLD_DATA_READY_PIN, GPIO_IN);
        gpio_pull_down(PLD_DATA_READY_PIN);
        gpio_init(PLD_COMMAND_DONE_PIN);
        gpio_set_dir(PLD_COMMAND_DONE_PIN, GPIO_IN);
        gpio_pull_down(PLD_COMMAND_DONE_PIN);
        gpio_init(PLD_COMMAND_STATUS0_PIN);
        gpio_set_dir(PLD_COMMAND_STATUS0_PIN, GPIO_IN);
        gpio_init(PLD_COMMAND_STATUS1_PIN);
        gpio_set_dir(PLD_COMMAND_STATUS1_PIN, GPIO_IN);
        gpio_init(PLD_COMMAND_STATUS2_PIN);
        gpio_set_dir(PLD_COMMAND_STATUS2_PIN, GPIO_IN);
    }

    packet::Frame spi_receive(void) {
        while (!gpio_get(PLD_DATA_READY_PIN))
            tight_loop_contents();

        packet::Frame tx{};
        packet::Frame rx{};
        select_fpga();
        spi_write_read_blocking(spi0, tx.data(), rx.data(), rx.size());
        deselect_fpga();
        return rx;
    }

    CommandStatus spi_command(const packet::ControlCommand& command) {
        const auto frame = packet::encode_control_command(command);
        packet::ControlCommandFrame rx{};

        select_fpga();
        spi_write_read_blocking(spi0, frame.data(), rx.data(), frame.size());
        deselect_fpga();

        const absolute_time_t deadline = make_timeout_time_ms(COMMAND_TIMEOUT_MS);
        while (!gpio_get(PLD_COMMAND_DONE_PIN)) {
            if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0)
                return CommandStatus::None;
            tight_loop_contents();
        }
        return read_command_status();
    }

    CommandStatus set_control(uint8_t id, uint8_t width, uint32_t value) {
        const packet::ControlCommand command{
            packet::ControlOpcode::Set, next_sequence++, id, width, value};
        return spi_command(command);
    }

    CommandStatus clear_control_override(uint8_t id, uint8_t width) {
        const packet::ControlCommand command{
            packet::ControlOpcode::ClearOverride, next_sequence++, id, width, 0};
        return spi_command(command);
    }
}
