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
#include "SnepClient.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include "utils/Log.h"

using namespace android;

static pthread_t thread_id;
static sem_t thread_sem;

static int sToken;

static NfcEventType msg_type = MSG_UNDEFINED;

NfcService* NfcService::sInstance = NULL;
NfcManager* NfcService::sNfcManager = NULL;
MessageHandler* NfcService::sMsgHandler = NULL;
List<NfcEvent*> NfcService::mQueue;

void NfcService::nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice)
{
  ALOGD("%s enter", __func__);
  NfcEvent *event = new NfcEvent();
  event->type = MSG_LLCP_LINK_ACTIVATION;
  event->data = pDevice;
  mQueue.push_back(event);

  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice)
{
  ALOGD("%s enter", __func__);
  NfcEvent *event = new NfcEvent();
  event->type = MSG_LLCP_LINK_DEACTIVATION;
  event->data = pDevice;
  mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_NDEF_TAG(void* pTag)
{
  ALOGD("%s enter", __func__);
  NfcEvent *event = new NfcEvent();
  event->type = MSG_TAG_DISCOVERED;
  event->data = pTag;
  mQueue.push_back(event);
  ALOGD("%s pushes to Q, queue.size()=%d event=%p",__func__, mQueue.size(), event);
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

static void NfcService_MSG_LLCP_LINK_DEACTIVATION(NfcEvent* event)
{
  ALOGD("%s enter", __func__);

  void* pDevice = event->data;
  IP2pDevice* pIP2pDevice = reinterpret_cast<IP2pDevice*>(pDevice);

  if (pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET) {
    pIP2pDevice->doDisconnect();
  }
}

static void NfcService_MSG_LLCP_LINK_ACTIVATION(NfcEvent* event)
{
  ALOGD("%s enter", __func__);
  void* pDevice = event->data;
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

static void *pollingThreadFunc(void *arg)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(arg);

  // TODO : check if check tag presence here is correct
  // For android. it use startPresenceChecking API in INfcTag.java
  while (pINfcTag->presenceCheck()) {
    sleep(1);
  }

  pINfcTag->disconnect();
  NfcEvent *event = new NfcEvent();
  event->type = MSG_TAG_LOST;
  event->data = NULL;
  NfcService* service = NfcService::Instance();
  List<NfcEvent*>& queue = service->mQueue;
  queue.push_back(event);
  sem_post(&thread_sem);
  return NULL;
}

void NfcService::handleTagDiscovered(NfcEvent* event)
{
  void* pTag = event->data;
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(pTag);
  sMsgHandler->processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, pINfcTag);

  pthread_t tid;
  pthread_create(&tid, NULL, pollingThreadFunc, pINfcTag);
}

void NfcService::handleTagLost(NfcEvent* event)
{
  sMsgHandler->processNotification(NFC_NOTIFICATION_TECH_LOST, NULL);
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

    // Using reference here to prvent creating another copy of List.
    List<NfcEvent*>& queue = service->mQueue;

    while (!queue.empty()) {
      NfcEvent* event = *queue.begin();
      queue.erase(queue.begin());
      NfcEventType eventType = event->type;

      ALOGD("NFCService msg=%d", eventType);
      switch(eventType) {
        case MSG_LLCP_LINK_ACTIVATION:
          NfcService_MSG_LLCP_LINK_ACTIVATION(event);
          break;
        case MSG_LLCP_LINK_DEACTIVATION:
          NfcService_MSG_LLCP_LINK_DEACTIVATION(event);
          break;
        case MSG_TAG_DISCOVERED:
          service->handleTagDiscovered(event);
          break;
        case MSG_TAG_LOST:
          service->handleTagLost(event);
          break;
        case MSG_READ_NDEF_DETAIL:
          NfcService::handleReadNdefDetailResponse(event);
        case MSG_READ_NDEF:
          NfcService::handleReadNdefResponse(event);
          break;
        case MSG_WRITE_NDEF:
          NfcService::handleWriteNdefResponse(event);
          break;
        case MSG_CLOSE:
          NfcService::handleCloseResponse(event);
          break;
        case MSG_SOCKET_CONNECTED:
          NfcService::sMsgHandler->processNotification(NFC_NOTIFICATION_INITIALIZED , NULL);
          break;
        case MSG_PUSH_NDEF:
          NfcService::handlePushNdefResponse(event);          
        default:
          ALOGE("NFCService bad message");
          abort();
      }

      //TODO delete event->data?
      delete event;
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
  sMsgHandler->processResponse(NFC_RESPONSE_GENERAL, token, NFC_ERROR_SUCCESS,  NULL);
  return status;
}

bool NfcService::handleReadNdefDetailRequest(int token)
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_READ_NDEF_DETAIL;
  event->token = token;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleReadNdefDetailResponse(NfcEvent* event)
{
  int token = event->token;
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  NdefDetail* pNdefDetail = pINfcTag->ReadNdefDetail();

  if (pNdefDetail != NULL) {
    sMsgHandler->processResponse(NFC_RESPONSE_READ_NDEF_DETAILS, token, NFC_ERROR_SUCCESS, pNdefDetail);
  } else {
    //TODO can we notify null ndef detail?
  }

  delete pNdefDetail;
}

bool NfcService::handleReadNdefRequest(int token)
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_READ_NDEF;
  event->token = token;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleReadNdefResponse(NfcEvent* event)
{
  int token = event->token;
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  NdefMessage* pNdefMessage = pINfcTag->findAndReadNdef();

  ALOGD("pNdefMessage=%p",pNdefMessage);
  if (pNdefMessage != NULL) {
    sMsgHandler->processResponse(NFC_RESPONSE_READ_NDEF, token, NFC_ERROR_SUCCESS, pNdefMessage);
  } else {
    //TODO can we notify null ndef?
  }

  delete pNdefMessage;
}

bool NfcService::handleWriteNdefRequest(NdefMessage* ndef, int token)
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_WRITE_NDEF;
  event->token = token;
  event->data = ndef;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleWriteNdefResponse(NfcEvent* event)
{
  int token = event->token;
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(event->data);
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  bool result = pINfcTag->writeNdef(*ndef);
  delete ndef;
  sMsgHandler->processResponse(NFC_RESPONSE_GENERAL, token, NFC_ERROR_SUCCESS, NULL);
}

void NfcService::handleCloseRequest()
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_CLOSE;
  mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::handleCloseResponse(NfcEvent* event)
{
  sMsgHandler->processResponse(NFC_RESPONSE_GENERAL, 0, NFC_ERROR_SUCCESS, NULL);
}

void NfcService::onSocketConnected()
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_SOCKET_CONNECTED;
  mQueue.push_back(event);
  sem_post(&thread_sem);
}

bool NfcService::handlePushNdefRequest(NdefMessage* ndef, int token)
{
  ALOGD("%s enter token=%d", __func__, token);
  NfcEvent *event = new NfcEvent();
  event->type = MSG_PUSH_NDEF;
  event->token = token;
  event->data = ndef;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handlePushNdefResponse(NfcEvent* event)
{
  int token = event->token;
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(event->data);

  // TODO : Do we need create a thread to do this ? And can we use the same snep client each time ?
  SnepClient snep;
  snep.connect();
  snep.put(*ndef);
  snep.close();

  delete ndef;
  sMsgHandler->processResponse(NFC_RESPONSE_GENERAL, token, NFC_ERROR_SUCCESS, NULL);
}
