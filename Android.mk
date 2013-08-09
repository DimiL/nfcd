# Copyright 2006 The Android Open Source Project

#ifneq ($(TARGET_PROVIDES_NFCD),true) #{

LOCAL_PATH := $(call my-dir)

# Build nfcd
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    src/nfcd.cpp \
    lib-nci/NativeNfcManager.cpp \
    lib-nci/NativeLlcpConnectionlessSocket.cpp \
    lib-nci/NativeLlcpSocket.cpp \
    lib-nci/NativeLlcpServiceSocket.cpp \
    lib-nci/NativeNfcSecureElement.cpp \
    lib-nci/NativeP2pDevice.cpp \
    lib-nci/NativeNfcTag.cpp \
    

LOCAL_C_INCLUDES += \
    external/libnfc-nxp/src \
    external/libnfc-nxp/inc \
    external/stlport/stlport \
    external/openssl/include \
    external/jansson/android \
    external/jansson/src \
    bionic \
    bionic/linker \
    system/nfcd/lib-nci \

LOCAL_SHARED_LIBRARIES += \
    libcutils \

LOCAL_MODULE := nfcd
LOCAL_MODULE_TAGS := debug

LOCAL_CFLAGS := -DDEBUG -DPLATFORM_ANDROID -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DHAVE_DLFCN_H=1 -DSILENT=1 -DNO_SIGNALS=1 -DNO_EXECUTE_PERMISSION=1 -D_GNU_SOURCE -D_REENTRANT -DUSE_MMAP -DUSE_MUNMAP -D_FILE_OFFSET_BITS=64 -DNO_UNALIGNED_ACCESS


include $(BUILD_EXECUTABLE)

#endif #} TARGET_PROVIDES_NFCD
