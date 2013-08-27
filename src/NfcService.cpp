/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "MessageHandler.h"
#include "NativeNfcTag.h"
#include "NfcGonkMessage.h"
#include "NfcService.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include <utils/Log.h>

static pthread_t thread_id;
static sem_t thread_sem;

static void* linkDevice;
static void* nfcTag;

typedef struct tagInfo {
  NativeNfcTag* msg;
  char* requestId;
} tag;

tag gTag;

typedef enum {
  MSG_UNDEFINED = 0,
  MSG_LLCP_LINK_ACTIVATION,
  MSG_LLCP_LINK_DEACTIVATION,
  MSG_NDEF_TAG,
  MSG_SE_FIELD_ACTIVATED,
  MSG_SE_FIELD_DEACTIVATED,
  MSG_SE_NOTIFY_TRANSACTION_LISTENERS
} MSG_TYPE;

static MSG_TYPE msg_type = MSG_UNDEFINED;

void NfcService::nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice)
{
  msg_type = MSG_LLCP_LINK_ACTIVATION;
  linkDevice = pDevice;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice)
{
  msg_type = MSG_LLCP_LINK_DEACTIVATION;
  linkDevice = pDevice;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_NDEF_TAG(void* pTag)
{
  msg_type = MSG_NDEF_TAG;
  nfcTag = pTag;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_SE_FIELD_ACTIVATED()
{
  msg_type = MSG_SE_FIELD_ACTIVATED;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_SE_FIELD_DEACTIVATED()
{
  msg_type = MSG_SE_FIELD_DEACTIVATED;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_SE_NOTIFY_TRANSACTION_LISTENERS()
{
  msg_type = MSG_SE_NOTIFY_TRANSACTION_LISTENERS;
  sem_post(&thread_sem);
}

static void NfcService_MSG_LLCP_LINK_ACTIVATION(void* pDevice)
{
  ALOGD("NfcService_MSG_LLCP_LINK_ACTIVATION");
}

static void NfcService_MSG_NDEF_TAG(void* pTag)
{
  ALOGD("NfcService_MSG_NDEF_TAG");
  NativeNfcTag* pNativeNfcTag = reinterpret_cast<NativeNfcTag*>(pTag);
  MessageHandler::processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, pNativeNfcTag);

  gTag.msg = pNativeNfcTag;
}

static void *serviceThreadFunc(void *arg)
{
  pthread_setname_np(pthread_self(), "NFCService thread");

  ALOGD("NFCService started");
  while(true) {
    if(sem_wait(&thread_sem)) {
      ALOGE("NFCService: Failed to wait for semaphore");
      abort();
    }

    switch(msg_type) {
      case MSG_LLCP_LINK_ACTIVATION:
        NfcService_MSG_LLCP_LINK_ACTIVATION(linkDevice);
        break;
      case MSG_LLCP_LINK_DEACTIVATION:
        break;
      case MSG_NDEF_TAG:
        NfcService_MSG_NDEF_TAG(nfcTag);
        break;
      case MSG_SE_FIELD_ACTIVATED:
        break;
      case MSG_SE_FIELD_DEACTIVATED:
        break;
      case MSG_SE_NOTIFY_TRANSACTION_LISTENERS:
        break;
      default:
        ALOGE("NFCService bad message");
        abort();
    }
  }

  return NULL;
}

NfcService* NfcService::sInstance = NULL;
NfcManager* NfcService::sNfcManager = NULL;

NfcService* NfcService::Instance() {
    if (!sInstance)
        sInstance = new NfcService();
    return sInstance;
}

NfcService::NfcService()
{
}

NfcService::~NfcService()
{
}

void NfcService::initialize(NfcManager* pNfcManager)
{
  if(sem_init(&thread_sem, 0, 0) == -1)
  {
    ALOGE("init_nfc_service Semaphore creation failed");
    abort();
  }

  if(pthread_create(&thread_id, NULL, serviceThreadFunc, NULL) != 0)
  {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }

  sNfcManager = pNfcManager;
}

int NfcService::handleConnect(int technology, int token)
{
  NativeNfcTag* pNativeNfcTag = reinterpret_cast<NativeNfcTag*>(sNfcManager->getNativeStruct("NativeNfcTag"));
  int status = pNativeNfcTag->connectWithStatus(technology);
  MessageHandler::processResponse(NFC_REQUEST_CONNECT, token, NULL);
  return status;
}

bool NfcService::handleReadNdef(int token)
{
  NativeNfcTag* pNativeNfcTag = reinterpret_cast<NativeNfcTag*>(sNfcManager->getNativeStruct("NativeNfcTag"));
  NdefMessage* pNdefMessage = pNativeNfcTag->findAndReadNdef();

  if (pNdefMessage != NULL) {
    MessageHandler::processResponse(NFC_REQUEST_READ_NDEF, token, pNdefMessage);
  } else {
  }
 
  delete pNdefMessage;

  return true;
}
