cmake_minimum_required(VERSION 3.9)
project(usbRW)

set(CMAKE_CXX_STANDARD 11)

LINK_DIRECTORIES(jni)

add_executable(usbRW
#        jni/main.cpp
#        jni/mylib.c
        jni/usbRW.cpp)

include_directories(src
        d:/work/IRRemote/NDK_PROJ/uWebSockets/src
        d:/Androidndk-arm-api23-standalone-toolchain/sysroot/usr/include
        )

ADD_DEFINITIONS(-std=c++11)

add_executable(webSvr
        #        jni/main.cpp
        #        jni/mylib.c
        jni/webServer.cpp)

TARGET_LINK_LIBRARIES(usbRW usbhost uWS ssl z crypto)
TARGET_LINK_LIBRARIES(webSvr uWS ssl z crypto)
