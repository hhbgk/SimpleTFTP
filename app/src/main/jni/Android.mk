LOCAL_PATH := $(call my-dir)

MY_APP_JNI_ROOT := $(realpath $(LOCAL_PATH))

ifeq ($(TARGET_ARCH_ABI),armeabi)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
endif

include $(call all-subdir-makefiles)
