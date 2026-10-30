#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace debug {
enum class Direction : std::uint8_t { Input, Output, Internal };
struct Signal {
    std::uint16_t id;
    std::string_view name;
    Direction direction;
    std::uint16_t width;
    std::uint32_t lsb;
    std::uint32_t msb;
};

inline constexpr std::array<Signal, 56> signals = {{
    {0, "Lab4_RALU.MSA", Direction::Input, 2, 0, 1},
    {1, "Lab4_RALU.Input", Direction::Input, 4, 2, 5},
    {2, "Lab4_RALU.CLK", Direction::Input, 1, 6, 6},
    {3, "Lab4_RALU.S_L", Direction::Input, 1, 7, 7},
    {4, "Lab4_RALU.MSB", Direction::Input, 2, 8, 9},
    {5, "Lab4_RALU.MSC", Direction::Input, 3, 10, 12},
    {6, "Lab4_RALU.CIN", Direction::Input, 1, 13, 13},
    {7, "Lab4_RALU.RegA", Direction::Output, 4, 14, 17},
    {8, "Lab4_RALU.RegB", Direction::Output, 4, 18, 21},
    {9, "Lab4_RALU.Output", Direction::Output, 4, 22, 25},
    {10, "Lab4_RALU.A_and_B", Direction::Output, 4, 26, 29},
    {11, "Lab4_RALU.A_or_B", Direction::Output, 4, 30, 33},
    {12, "Lab4_RALU.A_plus_B", Direction::Output, 4, 34, 37},
    {13, "Lab4_RALU.COUT", Direction::Output, 1, 38, 38},
    {14, "Lab4_RALU.MSA1", Direction::Internal, 1, 39, 39},
    {15, "Lab4_RALU.MSA0", Direction::Internal, 1, 40, 40},
    {16, "Lab4_RALU.LO", Direction::Internal, 1, 41, 41},
    {17, "Lab4_RALU.Input3", Direction::Internal, 1, 42, 42},
    {18, "Lab4_RALU.RegA3", Direction::Internal, 1, 43, 43},
    {19, "Lab4_RALU.RegB3", Direction::Internal, 1, 44, 44},
    {20, "Lab4_RALU.Output3", Direction::Internal, 1, 45, 45},
    {21, "Lab4_RALU.Input2", Direction::Internal, 1, 46, 46},
    {22, "Lab4_RALU.RegA2", Direction::Internal, 1, 47, 47},
    {23, "Lab4_RALU.RegB2", Direction::Internal, 1, 48, 48},
    {24, "Lab4_RALU.Output2", Direction::Internal, 1, 49, 49},
    {25, "Lab4_RALU.Input1", Direction::Internal, 1, 50, 50},
    {26, "Lab4_RALU.RegA1", Direction::Internal, 1, 51, 51},
    {27, "Lab4_RALU.RegB1", Direction::Internal, 1, 52, 52},
    {28, "Lab4_RALU.Output1", Direction::Internal, 1, 53, 53},
    {29, "Lab4_RALU.Input0", Direction::Internal, 1, 54, 54},
    {30, "Lab4_RALU.RegA0", Direction::Internal, 1, 55, 55},
    {31, "Lab4_RALU.RegB0", Direction::Internal, 1, 56, 56},
    {32, "Lab4_RALU.Output0", Direction::Internal, 1, 57, 57},
    {33, "Lab4_RALU.HI", Direction::Internal, 1, 58, 58},
    {34, "Lab4_RALU.MSB1", Direction::Internal, 1, 59, 59},
    {35, "Lab4_RALU.MSB0", Direction::Internal, 1, 60, 60},
    {36, "Lab4_RALU.MSC2", Direction::Internal, 1, 61, 61},
    {37, "Lab4_RALU.MSC1", Direction::Internal, 1, 62, 62},
    {38, "Lab4_RALU.MSC0", Direction::Internal, 1, 63, 63},
    {39, "Lab4_RALU.A_plus_B3", Direction::Internal, 1, 64, 64},
    {40, "Lab4_RALU.A_or_B3", Direction::Internal, 1, 65, 65},
    {41, "Lab4_RALU.A_and_B3", Direction::Internal, 1, 66, 66},
    {42, "Lab4_RALU.NOT_RegA3", Direction::Internal, 1, 67, 67},
    {43, "Lab4_RALU.NOT_RegA2", Direction::Internal, 1, 68, 68},
    {44, "Lab4_RALU.NOT_RegA1", Direction::Internal, 1, 69, 69},
    {45, "Lab4_RALU.NOT_RegA0", Direction::Internal, 1, 70, 70},
    {46, "Lab4_RALU.A_and_B2", Direction::Internal, 1, 71, 71},
    {47, "Lab4_RALU.A_or_B2", Direction::Internal, 1, 72, 72},
    {48, "Lab4_RALU.A_plus_B2", Direction::Internal, 1, 73, 73},
    {49, "Lab4_RALU.A_and_B1", Direction::Internal, 1, 74, 74},
    {50, "Lab4_RALU.A_or_B1", Direction::Internal, 1, 75, 75},
    {51, "Lab4_RALU.A_plus_B1", Direction::Internal, 1, 76, 76},
    {52, "Lab4_RALU.A_and_B0", Direction::Internal, 1, 77, 77},
    {53, "Lab4_RALU.A_or_B0", Direction::Internal, 1, 78, 78},
    {54, "Lab4_RALU.A_plus_B0", Direction::Internal, 1, 79, 79},
    {55, "Lab4_RALU.Cin", Direction::Internal, 1, 80, 80},
}};
inline constexpr std::size_t signal_count = 56;
inline constexpr std::size_t total_probe_bits = 81;

// Throws std::out_of_range when exceptions are enabled.
constexpr const Signal& get_signal(std::size_t id) {
    return signals.at(id);
}

// Safe for firmware built without C++ exceptions.
constexpr const Signal* try_get_signal(std::size_t id) noexcept {
    return id < signals.size() ? &signals[id] : nullptr;
}
} // namespace debug
