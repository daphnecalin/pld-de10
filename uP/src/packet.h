#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace packet {
// using for common things
using std::uint8_t, std::uint32_t, std::size_t;

// constexpr instead of #define is kind of awesome
// number of bytes in a frame
constexpr size_t FRAME_BYTES = 128;
// start marker of packet
constexpr uint8_t START_MARKER = 0xA5;

/**
 * The below variable is the protocol version. The name
 * "PROTOCOL_VER" comes from a shortened version of
 * protocol version. The word "protocol" remains intact
 * while the word "version" has been shorted to "ver".
 * The variable name is upper case to denote that it is a constant.
 * As it represents the protocol version, as the debug IP is updated
 * with new versions, this variable will change to whatever the most
 * recent protocol version is. This comment should hopefully
 * explain the background and meaning of the PROTOCOL_VER variable.
 */
constexpr uint8_t PROTOCOL_VER = 0x01;

// max number of signals per 128 byte packet (for now)
constexpr size_t MAX_SIGNALS = 16;

// types for ease of use
using Frame = std::array<uint8_t, FRAME_BYTES>;
using Signal_Data = std::uint32_t;
// each signal contains:
// signal id, width, and relevant values
struct Signal {
    uint8_t id;
    uint8_t width; // number of valid bits
    Signal_Data value; // little endian value
};

// every packet is made up of a sequence number
// the number of signals, and the signals themselves
struct Packet {
    uint8_t sequence_number;
    uint8_t signal_count;
    std::array<Signal, MAX_SIGNALS> signals;

    // parse frame into packet, use optional for error handling code
    static std::optional<Packet> parse(const Frame& frame);
};
}