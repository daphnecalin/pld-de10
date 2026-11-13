#pragma once
#include "core/packet.h"

namespace transfer {
// interrupt handler?
void data_ready(void);
// receive frame over spi line after interrupt handler
packet::Frame spi_receive(void);
} // namespace transfer