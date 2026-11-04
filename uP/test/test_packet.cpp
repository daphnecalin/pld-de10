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

TEST_CASE("Packet::parse rejects bad data") {

    // modify fixture data and check that it's rejected
    pld_test::Fixture f = fixtures[0];

    SUBCASE("incorrect start") {
        f.frame[0] = 0xFF;
        CHECK_FALSE(Packet::parse(f.frame));
    }

    SUBCASE("incorrect protocol") {
        f.frame[1] = 0xFF;
        CHECK_FALSE(Packet::parse(f.frame));
    }

    SUBCASE("bad checksum") {
        f.frame[packet::FRAME_BYTES - 1] -= 1;
        CHECK_FALSE(Packet::parse(f.frame));
    }

    SUBCASE("byte not within representable range") {
        size_t offset = 4;
        f.frame[offset + 1] = 5;
        CHECK_FALSE(Packet::parse(f.frame));
    }

    SUBCASE("invalid width") {
        // calculate offset of 13th signal and change width to a huge number
        packet::Frame& frame = f.frame;
        size_t offset = 4;
        size_t sig_count = 0;
        while(sig_count < 13 && offset + 2 < packet::FRAME_BYTES - 1) {
            uint8_t width = frame[offset + 1];
            uint8_t bytes = (width + 7) / 8;
            offset += 2 + bytes;
            sig_count++;
        }

        // now that offset is calculated, change width to be huge
        frame[offset + 1] = 200;
        // then recalculate checksum

        // and verify it doesn't parse
        CHECK_FALSE(Packet::parse(frame));
    }
}