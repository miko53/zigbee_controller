#!/bin/sh

mkdir build-linux-arm-eabi-linaro
cd build-linux-arm-eabi-linaro
cmake -DUSE_GPIO_OLD_API=ON -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_beagleboneblack_linaro.cmake $@ ./..

