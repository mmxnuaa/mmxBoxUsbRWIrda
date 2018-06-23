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
        )

ADD_DEFINITIONS(-std=c++11)

add_executable(webSvr
        #        jni/main.cpp
        #        jni/mylib.c
        jni/webServer.cpp)

TARGET_LINK_LIBRARIES(usbRW usbhost)
TARGET_LINK_LIBRARIES(webSvr uWS ssl z crypto)
