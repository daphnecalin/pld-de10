#pragma once
#include "packet.cpp"

namespace transfer {
    // set up spi peripheral on gpios
    void spi_setup(void);
    // interrupt handler?
    void data_ready(void);
    // receive frame over spi line after interrupt handler
    packet::Frame spi_receive(void);
}