#!/bin/bash

export PICO_SDK_PATH=[your sdk path]

case "${1:-fw}" in
    fw) cmake -DPICO_BOARD=adafruit_feather_rp2040 -S . -B build && cmake --build build ;;
    test) cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test --output-on-failure ;;
    *) echo "usage: $0 [fw|test]"; exit 1;;
esac