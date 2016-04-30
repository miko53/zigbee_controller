
cmake_minimum_required(VERSION 2.6)
project(zb_controler)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -W -Wall -Wextra -std=c99 -D_BSD_SOURCE")

add_executable(zb_controler controler.c serial.c zigbee.c zigbee_protocol.c display.c)
