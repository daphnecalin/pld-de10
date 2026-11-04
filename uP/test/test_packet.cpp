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
    f.frame[0] = 0xFF;
    INFO("rejects incorrect start");
    std::optional<Packet> p = Packet::parse(f.frame);
    REQUIRE(!p.has_value());

    f.frame[0] = packet::START_MARKER;
    f.frame[1] = 0x10;
    INFO("rejects incorrect protocol");
    p = Packet::parse(f.frame);
    REQUIRE(!p.has_value());
}