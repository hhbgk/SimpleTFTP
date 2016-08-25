LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := jl_tftp
LOCAL_SRC_FILES += tftp_api_jni.c
LOCAL_SRC_FILES += common.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)