#include "doctest.h"
#include "debug_packets.h"

// CHECK(expr) records failure and continues
// REQUIRE(expr) records failure then stops
// CAPTURE and INFO attach additional info to a failure

using packet::Packet;
using pld_test::fixtures;

TEST_CASE("Packet::parse matches generated fixtures") {
    // loop over fixtures
    for(size_t i = 0; i < fixtures.size(); i++) {
        CAPTURE(i);
        const auto& fx = fixtures[i];
        // parse packet
        std::optional<Packet> p = Packet::parse(fx.frame);
        REQUIRE(p.has_value());

        // compare values
        CHECK(p->sequence_number == fx.sample_number);
        REQUIRE(p->signal_count == fx.signals.size());
        for(size_t j = 0; j < p->signal_count; j++) {
            CHECK(p->signals[j].id == fx.signals[j].id);
            CHECK(p->signals[j].width == fx.signals[j].width);
            CHECK(p->signals[j].value == fx.signals[j].value);
        }
    }
}

// for recalculating checksum
void checksum(packet::Frame& frame) {
    uint8_t sum = 0;
    for(size_t i = 0; i < packet::FRAME_BYTES - 1; i++)
        sum ^= frame[i];

    frame[packet::FRAME_BYTES - 1] = sum;
}

TEST_CASE("Packet::parse rejects bad data") {

    // modify fixture data and check that it's rejected
    pld_test::Fixture f = fixtures[0];

    SUBCASE("incorrect start") {
        f.frame[0] = 0xFF;
        checksum(f.frame);
        CHECK_FALSE(Packet::parse(f.frame));
    }

    SUBCASE("incorrect protocol") {
        f.frame[1] = 0xFF;
        checksum(f.frame);
        CHECK_FALSE(Packet::parse(f.frame));
    }

    SUBCASE("bad checksum") {
        f.frame[packet::FRAME_BYTES - 1] -= 1;
        CHECK_FALSE(Packet::parse(f.frame));
    }

    SUBCASE("byte not within representable range") {
        size_t offset = 4;
        f.frame[offset + 1] = 5;
        checksum(f.frame);
        CHECK_FALSE(Packet::parse(f.frame));
    }
}