#color conversion lib
LOCAL_PATH := $(call my-dir)
$(info $(LOCAL_PATH))

include $(CLEAR_VARS)

LOCAL_MODULE    := ConnectionManager

include $(BUILD_SHARED_LIBRARY)

