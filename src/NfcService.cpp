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
  MSG_WRITE_NDEF,
} MSG_TYPE;

static MSG_TYPE msg_type = MSG_UNDEFINED;

NfcService* NfcService::sInstance = NULL;
NfcManager* NfcService::sNfcManager = NULL;
MessageHandler* NfcService::sMsgHandler = NULL;

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
  ALOGD("%s enter", __func__);
  IP2pDevice* pIP2pDevice = reinterpret_cast<IP2pDevice*>(pDevice);

  if (pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET ||
      pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_INITIATOR) {
    if(pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET) {
      if (pIP2pDevice->doConnect()) {
        ALOGD("Connected to device!");
      }
      else {
        ALOGE("Cannot connect remote Target. Polling loop restarted.");
      }
    }

    INfcManager* pINfcManager = NfcService::getNfcManager();
    bool ret = pINfcManager->doCheckLlcp();
    if(ret == true) {
      ret = pINfcManager->doActivateLlcp();
      if(ret == true) {
        ALOGD("Target Activate LLCP OK");
      } else {
        ALOGE("doActivateLLcp failed");
      }
    } else {
      ALOGE("doCheckLLcp failed");
    }
  } else {
    ALOGE("com_android_nfc_NfcService: Unknown LLCP P2P mode");
    //stop();
  }
}

void *pollingThreadFunc(void *arg)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(arg);

  // TODO : check if check tag presence here is correct
  // For android. it use startPresenceChecking API in INfcTag.java
  while (pINfcTag->presenceCheck()) {
    sleep(1);
  }

  pINfcTag->disconnect();
  return NULL;
}

void NfcService::handleNdefTag(void* pTag)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(pTag);
  sMsgHandler->processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, pINfcTag);

  pthread_t tid;
  pthread_create(&tid, NULL, pollingThreadFunc, pINfcTag);
}

static void *serviceThreadFunc(void *arg)
{
  pthread_setname_np(pthread_self(), "NFCService thread");

  NfcService* service = reinterpret_cast<NfcService*>(arg);

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
        service->handleNdefTag(nfcTag);
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
      case MSG_WRITE_NDEF:
        NfcService::handleWriteNdefResponse(sToken);
        break;
      default:
        ALOGE("NFCService bad message");
        abort();
    }
  }

  return NULL;
}

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

void NfcService::initialize(NfcManager* pNfcManager, MessageHandler* msgHandler)
{
  if(sem_init(&thread_sem, 0, 0) == -1)
  {
    ALOGE("init_nfc_service Semaphore creation failed");
    abort();
  }

  if(pthread_create(&thread_id, NULL, serviceThreadFunc, this) != 0)
  {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }

  sMsgHandler = msgHandler;
  sNfcManager = pNfcManager;
}

INfcManager* NfcService::getNfcManager()
{
  return reinterpret_cast<INfcManager*>(NfcService::sNfcManager);
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
  sMsgHandler->processResponse(NFC_REQUEST_CONNECT, token, NULL);
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
    sMsgHandler->processResponse(NFC_REQUEST_READ_NDEF, token, pNdefMessage);
  } else {
    //TODO can we notify null ndef?
  }

  delete pNdefMessage;
}

bool NfcService::handleWriteNdefRequest(NdefMessage& ndef, int token)
{
  ALOGD("%s enter token=%d", __func__, token);

  //TODO try not to do it in main thread.
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  bool result = pINfcTag->writeNdef(ndef);

  msg_type = MSG_WRITE_NDEF;
  sToken = token;
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleWriteNdefResponse(int token)
{
  sMsgHandler->processResponse(NFC_REQUEST_WRITE_NDEF, token, NULL);
}
