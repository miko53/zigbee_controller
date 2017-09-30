
cmake_minimum_required(VERSION 2.6)
project(zb_controler)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -W -Wall -Wextra -std=gnu99")

option(TRACE_ACTIVATED OFF)

if (TRACE_ACTIVATED)
add_definitions(-DTRACE_ACTIVATED)
endif()


add_executable(zb_controler controler.c serial.c zigbee.c zigbee_protocol.c display.c gpio.c sensor.c configfile.c daemonize.c sensor_db.c webcmd.c)
add_executable(zb_test test.c sensor.c configfile.c sensor_db.c webcmd.c)

add_executable(bmp085 bmp085.c configfile.c gpio.c daemonize.c)
target_link_libraries(bmp085 m)
