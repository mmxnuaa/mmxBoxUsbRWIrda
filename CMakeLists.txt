cmake_minimum_required(VERSION 3.9)
project(usbRW)

set(CMAKE_CXX_STANDARD 11)

LINK_DIRECTORIES(jni)

add_executable(usbRW
#        jni/main.cpp
#        jni/mylib.c
        jni/usbRW.cpp)

TARGET_LINK_LIBRARIES(usbRW usbhost)
