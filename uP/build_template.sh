#!/bin/bash

export PICO_SDK_PATH=[your sdk path]
mkdir build
cmake -DPICO_BOARD=adafruit_feather_rp2040 -S . -B build
cmake --build build