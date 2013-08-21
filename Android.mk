# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

#ifneq ($(TARGET_PROVIDES_NFCD),true) #{

LOCAL_PATH := $(call my-dir)

# Build nfcd
include $(CLEAR_VARS)

VOB_COMPONENTS := external/libnfc-nci/src
NFA := $(VOB_COMPONENTS)/nfa
NFC := $(VOB_COMPONENTS)/nfc

LOCAL_SRC_FILES := \
    src/nfcd.cpp \
    src/DeviceHost.cpp \
    src/NfcService.cpp \
    src/NfcIpcSocket.cpp \
    src/NfcUtil.cpp \
    src/MessageHandler.cpp \
    libnci/NativeNfcManager.cpp \
    libnci/NativeLlcpConnectionlessSocket.cpp \
    libnci/NativeLlcpSocket.cpp \
    libnci/NativeLlcpServiceSocket.cpp \
    libnci/NativeNfcSecureElement.cpp \
    libnci/NativeP2pDevice.cpp \
    libnci/NativeNfcTag.cpp \
    libnci/Mutex.cpp \
    libnci/CondVar.cpp \
    libnci/PowerSwitch.cpp \
    libnci/NfcTag.cpp \
    libnci/PeerToPeer.cpp \
    libnci/Pn544Interop.cpp \
    libnci/IntervalTimer.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/libnci \
    $(LOCAL_PATH)/src \
    $(NFA)/include \
    $(NFA)/brcm \
    $(NFC)/include \
    $(NFC)/brcm \
    $(NFC)/int \
    $(VOB_COMPONENTS)/hal/include \
    $(VOB_COMPONENTS)/hal/int \
    $(VOB_COMPONENTS)/include \
    $(VOB_COMPONENTS)/gki/ulinux \
    $(VOB_COMPONENTS)/gki/common \
    external/stlport/stlport \
    external/openssl/include \
    external/jansson/android \
    external/jansson/src \
    bionic

LOCAL_SHARED_LIBRARIES += \
    libicuuc \
    libnativehelper \
    libcutils \
    libutils \
    liblog \
    libnfc-nci \
    libstlport \
    libjansson

LOCAL_MODULE := nfcd
LOCAL_MODULE_TAGS := debug

LOCAL_CFLAGS := -DDEBUG -DPLATFORM_ANDROID -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DHAVE_DLFCN_H=1 -DSILENT=1 -DNO_SIGNALS=1 -DNO_EXECUTE_PERMISSION=1 -D_GNU_SOURCE -D_REENTRANT -DUSE_MMAP -DUSE_MUNMAP -D_FILE_OFFSET_BITS=64 -DNO_UNALIGNED_ACCESS

include $(BUILD_EXECUTABLE)

#endif #} TARGET_PROVIDES_NFCD
