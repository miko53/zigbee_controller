#!/bin/sh

mkdir build-linux-arm-rpi3
cd build-linux-arm-rpi3
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_raspberry_pi_3_linaro.cmake $@ ./..

