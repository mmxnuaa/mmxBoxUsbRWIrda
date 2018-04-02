LOCAL_PATH := $(call my-dir)


#include $(CLEAR_VARS)
#LOCAL_MODULE    :=  test  # name your module here.
#LOCAL_SRC_FILES := mylib.c
#include $(BUILD_SHARED_LIBRARY)


#LOCAL_PATH:= $(call my-dir) # Get the local path of the project.
include $(CLEAR_VARS) # Clear all the variables with a prefix "LOCAL_"

LOCAL_SRC_FILES := main.cpp # Indicate the source code.
LOCAL_MODULE := hello # The name of the binary.
LOCAL_LDLIBS := -ltest
LOCAL_LDFLAGS += -L$(LOCAL_PATH)/../
include $(BUILD_EXECUTABLE) # Tell ndk-build that we want to build a native executable.