/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "MessageHandler.h"
#include "INfcTag.h"
#include "NfcGonkMessage.h"
#include "NfcService.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include <utils/Log.h>

static pthread_t thread_id;
static sem_t thread_sem;

static void* linkDevice;
static void* nfcTag;
//TODO needs better handling here.
static int sToken;

typedef enum {
  MSG_UNDEFINED = 0,
  MSG_LLCP_LINK_ACTIVATION,
  MSG_LLCP_LINK_DEACTIVATION,
  MSG_NDEF_TAG,
  MSG_SE_FIELD_ACTIVATED,
  MSG_SE_FIELD_DEACTIVATED,
  MSG_SE_NOTIFY_TRANSACTION_LISTENERS,
  MSG_READ_NDEF,
} MSG_TYPE;

static MSG_TYPE msg_type = MSG_UNDEFINED;

void NfcService::nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice)
{
  ALOGD("%s enter", __func__);
  msg_type = MSG_LLCP_LINK_ACTIVATION;
  linkDevice = pDevice;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice)
{
  ALOGD("%s enter", __func__);
  msg_type = MSG_LLCP_LINK_DEACTIVATION;
  linkDevice = pDevice;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_NDEF_TAG(void* pTag)
{
  ALOGD("%s enter", __func__);
  msg_type = MSG_NDEF_TAG;
  nfcTag = pTag;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_SE_FIELD_ACTIVATED()
{
  ALOGD("%s enter", __func__);
  msg_type = MSG_SE_FIELD_ACTIVATED;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_SE_FIELD_DEACTIVATED()
{
  ALOGD("%s enter", __func__);
  msg_type = MSG_SE_FIELD_DEACTIVATED;
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_SE_NOTIFY_TRANSACTION_LISTENERS()
{
  ALOGD("%s enter", __func__);
  msg_type = MSG_SE_NOTIFY_TRANSACTION_LISTENERS;
  sem_post(&thread_sem);
}

static void NfcService_MSG_LLCP_LINK_ACTIVATION(void* pDevice)
{
  ALOGD("NfcService_MSG_LLCP_LINK_ACTIVATION");
}

void *pollingThreadFunc(void *arg)
{
  NativeNfcTag* pNativeNfcTag = reinterpret_cast<NativeNfcTag*>(arg);

  // TODO : check if check tag presence here is correct
  // For android. it use startPresenceChecking API in INfcTag.java
  while (pINfcTag->presenceCheck()) {
    sleep(1);
  }

  pNativeNfcTag->disconnect();
  return NULL;
}

static void NfcService_MSG_NDEF_TAG(void* pTag)
{
  NativeNfcTag* pNativeNfcTag = reinterpret_cast<NativeNfcTag*>(pTag);
  MessageHandler::processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, pNativeNfcTag);

  gTag.msg = pNativeNfcTag;

  pthread_t tid;
  pthread_create(&tid, NULL, pollingThreadFunc, pNativeNfcTag);
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

    ALOGD("NFCService msg=%d", msg_type);
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
      case MSG_READ_NDEF:
        NfcService::handleReadNdefResponse(sToken);
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

bool NfcService::handleDisconnect()
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  bool result = pINfcTag->disconnect();
  return result;
}

int NfcService::handleConnect(int technology, int token)
{

  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  int status = pINfcTag->connectWithStatus(technology);
  MessageHandler::processResponse(NFC_REQUEST_CONNECT, token, NULL);
  return status;
}

bool NfcService::handleReadNdefRequest(int token)
{
  ALOGD("%s enter token=%d", __func__, token);
  msg_type = MSG_READ_NDEF;
  sToken = token;
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleReadNdefResponse(int token)
{

  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  NdefMessage* pNdefMessage = pINfcTag->findAndReadNdef();

  ALOGD("pNdefMessage=%p",pNdefMessage);
  if (pNdefMessage != NULL) {
    MessageHandler::processResponse(NFC_REQUEST_READ_NDEF, token, pNdefMessage);
  } else {
  }

  delete pNdefMessage;
}

bool NfcService::handleWriteNdef(NdefMessage& ndef, int token)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  bool result = pINfcTag->writeNdef(ndef);

  MessageHandler::processResponse(NFC_REQUEST_WRITE_NDEF, token, NULL);

  return true;
}
