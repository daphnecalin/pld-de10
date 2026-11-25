#ifndef TRANSFER_H_
#define TRANSFER_H_
#include "core/packet.h"
#include "core/record.h"
namespace transfer
{
    inline RingBuffer ringbuf;
    // interrupt handler?
    void data_ready(void);
    // receive frame over spi line after interrupt handler
    packet::Frame spi_receive(void);
} // namespace transfer

#endif