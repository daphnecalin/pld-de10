#include "packet.h"

namespace packet {
    std::optional<Packet> parse(const Frame& frame) {
        // check start marker and protocol version
        if(frame[0] != 0xA5 || frame[1] != PROTOCOL_VER)
            return std::nullopt;

        // check checksum
        uint8_t sum = 0;
        for(size_t i = 0; i < FRAME_BYTES - 1; i++)
            sum ^= frame[i];

        if(sum != frame[FRAME_BYTES - 1])
            return std::nullopt;

        Packet p{};
        p.sequence_number = frame[2] | (frame[3] << 8);

        uint8_t offset = 4;
        // make sure we don't exceed max signal count
        // and that offset stays within frame_bytes
        while(p.signal_count < MAX_SIGNALS && offset + 2 < FRAME_BYTES - 1) {
            uint8_t id = frame[offset];
            uint8_t width = frame[offset + 1];
            // if width == 0, hit padding
            if(width == 0) break;
            // else, calculate number of bytes and create signal
            uint8_t bytes = (width + 7) / 8; // old school ceil
            // make sure bytes is within representable range and that
            // this signal does not extend past the end
            if(bytes > sizeof(Signal_Data) || offset + 2 + bytes > FRAME_BYTES - 1)
                return std::nullopt;

            // otherwise, create value and place into packet
            Signal_Data value = 0;
            for(size_t i = 0; i < bytes; i++)
                value |= Signal_Data(frame[offset + 2 + i]) << (8 * i);

            p.signals[p.signal_count] = {id, width, value};
            p.signal_count++;
            offset += 2 + bytes;
        }
        return p;
    }
}