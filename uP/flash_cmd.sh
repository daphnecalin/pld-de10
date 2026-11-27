#!/bin/bash

sh build.sh
lsblk
udisksctl mount -b /dev/sda1
cp build/main.uf2 /run/media/$USER/RPI-RP2/
