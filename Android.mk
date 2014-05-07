# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

#ifneq ($(TARGET_PROVIDES_NFCD),true) #{

LOCAL_PATH := $(call my-dir)

# Build nfcd
include $(CLEAR_VARS)

NFC_PROTOCOL := nci

LOCAL_SRC_FILES := \
    src/nfcd.cpp \
    src/NfcService.cpp \
    src/NfcIpcSocket.cpp \
    src/IpcSocketListener.cpp \
    src/NfcUtil.cpp \
    src/MessageHandler.cpp \
    src/SessionId.cpp \
    src/P2pLinkManager.cpp \
    src/snep/SnepServer.cpp \
    src/snep/SnepClient.cpp \
    src/snep/SnepMessage.cpp \
    src/snep/SnepMessenger.cpp \
    src/handover/HandoverClient.cpp \
    src/handover/HandoverServer.cpp \

NCI_SRC_FILES := \
    src/nci/NfcManager.cpp \
    src/nci/LlcpConnectionlessSocket.cpp \
    src/nci/LlcpSocket.cpp \
    src/nci/LlcpServiceSocket.cpp \
    src/nci/NfcSecureElement.cpp \
    src/nci/P2pDevice.cpp \
    src/nci/NfcTagManager.cpp \
    src/nci/Mutex.cpp \
    src/nci/CondVar.cpp \
    src/nci/PowerSwitch.cpp \
    src/nci/NfcTag.cpp \
    src/nci/PeerToPeer.cpp \
    src/nci/Pn544Interop.cpp \
    src/nci/IntervalTimer.cpp \
    src/nci/SecureElement.cpp \
    src/nci/RouteDataSet.cpp \
    src/nci/HostAidRouter.cpp

INTERFACE_SRC_FILES := \
    src/interface/DeviceHost.cpp \
    src/interface/NdefMessage.cpp \
    src/interface/NdefRecord.cpp

ifeq ($(NFC_PROTOCOL),nci)
LOCAL_SRC_FILES += $(NCI_SRC_FILES)
endif

LOCAL_SRC_FILES += $(INTERFACE_SRC_FILES)

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/src \
    external/stlport/stlport \
    external/openssl/include \
    bionic

ifeq ($(NFC_PROTOCOL),nci)
VOB_COMPONENTS := external/libnfc-nci/src
NFA := $(VOB_COMPONENTS)/nfa
NFC := $(VOB_COMPONENTS)/nfc

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/src/nci \
    $(LOCAL_PATH)/src/interface \
    $(LOCAL_PATH)/src/snep \
    $(LOCAL_PATH)/src/handover \
    $(NFA)/include \
    $(NFA)/brcm \
    $(NFC)/include \
    $(NFC)/brcm \
    $(NFC)/int \
    $(VOB_COMPONENTS)/hal/include \
    $(VOB_COMPONENTS)/hal/int \
    $(VOB_COMPONENTS)/include \
    $(VOB_COMPONENTS)/gki/ulinux \
    $(VOB_COMPONENTS)/gki/common
endif

LOCAL_SHARED_LIBRARIES += \
    libicuuc \
    libnativehelper \
    libcutils \
    libutils \
    liblog \
    libstlport \
    libcrypto \
    libbinder

ifeq ($(NFC_PROTOCOL),nci)
LOCAL_SHARED_LIBRARIES += \
    libnfc-nci
endif

LOCAL_MODULE := nfcd
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DDEBUG -DPLATFORM_ANDROID -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DHAVE_DLFCN_H=1 -DSILENT=1 -DNO_SIGNALS=1 -DNO_EXECUTE_PERMISSION=1 -D_GNU_SOURCE -D_REENTRANT -DUSE_MMAP -DUSE_MUNMAP -D_FILE_OFFSET_BITS=64 -DNO_UNALIGNED_ACCESS

include $(BUILD_EXECUTABLE)

#endif #} TARGET_PROVIDES_NFCD
