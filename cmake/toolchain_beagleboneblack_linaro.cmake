# this one is important
SET(CMAKE_SYSTEM_NAME BeagleBoneBlack-linux-arm-eabi)
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
SET(CMAKE_C_COMPILER   /opt/toolchains/gcc-linaro-arm-linux-gnueabihf-4.9-2014.09_linux/bin/arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER /opt/toolchains/gcc-linaro-arm-linux-gnueabihf-4.9-2014.09_linux/bin/arm-linux-gnueabihf-g++)


# where is the target environment 
SET(CMAKE_FIND_ROOT_PATH  /opt/toolchains/gcc-linaro-arm-linux-gnueabihf-4.9-2014.09_linux/)

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
