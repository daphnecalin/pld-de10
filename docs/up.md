# uP Development Environment Setup (Linux/WSL)

This is tested on Ubuntu 22.04 LTS.

Adapted from: https://wellys.com/posts/rp2040_c_linux/.

Anything in [] should be replaced with a value specific to your system.

0) Download the required dependencies.
```
    sudo apt install git
    sudo apt install cmake gcc-arm-none-eabi build-essential
    sudo apt install pkg-config
    sudo apt install minicom
    sudo apt install gdb-multiarch
    sudo apt install automake autoconf texinfo libtool libftdi-dev libusb-1.0-0-dev
```

1) Clone the pico SDK to a known location and initialize it.
```
    git clone -b master https://github.com/raspberrypi/pico-sdk.git
    cd pico-sdk
    git submodule update --init
``` 

2) Make a copy of build_template.sh called build.sh. Set the environment variable PICO_SDK_PATH to the folder you cloned the pico SDK to.

```
    cp build_template.sh build.sh
    nano build.sh
./build.sh
    ...
    export PICO_SDK_PATH=[path to sdk folder]
    ...
```

3) To build the project run the following command. If there are any issues with the build, try deleting the build folder and running the script again.
```
    sh build.sh
```