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
#include "NfcUtil.h"
#include "SnepClient.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include "utils/Log.h"

using namespace android;

typedef enum {
  MSG_UNDEFINED = 0,
  MSG_LLCP_LINK_ACTIVATION,
  MSG_LLCP_LINK_DEACTIVATION,
  MSG_TAG_DISCOVERED,
  MSG_TAG_LOST,
  MSG_SE_FIELD_ACTIVATED,
  MSG_SE_FIELD_DEACTIVATED,
  MSG_SE_NOTIFY_TRANSACTION_LISTENERS,
  MSG_READ_NDEF_DETAIL,
  MSG_READ_NDEF,
  MSG_WRITE_NDEF,
  MSG_CLOSE,
  MSG_SOCKET_CONNECTED,
  MSG_PUSH_NDEF,
  MSG_NDEF_TAG_LIST,
  MSG_CONFIG,
  MSG_MAKE_NDEF_READONLY,
} NfcEventType;

struct NfcEvent {
  NfcEventType type;
  void *data;
};

static pthread_t thread_id;
static sem_t thread_sem;

static int sToken;

static NfcEventType msg_type = MSG_UNDEFINED;

NfcService* NfcService::sInstance = NULL;
NfcManager* NfcService::sNfcManager = NULL;

void NfcService::nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice)
{
  ALOGD("%s enter", __func__);
  NfcEvent *event = new NfcEvent();
  event->type = MSG_LLCP_LINK_ACTIVATION;
  event->data = pDevice;
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice)
{
  ALOGD("%s enter", __func__);
  NfcEvent *event = new NfcEvent();
  event->type = MSG_LLCP_LINK_DEACTIVATION;
  event->data = pDevice;
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::nfc_service_send_MSG_TAG(void* pTag)
{
  ALOGD("%s enter", __func__);
  NfcEvent *event = new NfcEvent();
  event->type = MSG_TAG_DISCOVERED;
  event->data = pTag;
  NfcService::Instance()->mQueue.push_back(event);
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

void NfcService::handleLlcpLinkDeactivation(NfcEvent* event)
{
  ALOGD("%s enter", __func__);

  void* pDevice = event->data;
  IP2pDevice* pIP2pDevice = reinterpret_cast<IP2pDevice*>(pDevice);

  if (pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET) {
    pIP2pDevice->doDisconnect();
  }
}

void NfcService::handleLlcpLinkActivation(NfcEvent* event)
{
  ALOGD("%s enter", __func__);
  void* pDevice = event->data;
  IP2pDevice* pIP2pDevice = reinterpret_cast<IP2pDevice*>(pDevice);

  if (pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET ||
      pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_INITIATOR) {
    if (pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET) {
      if (pIP2pDevice->doConnect()) {
        ALOGD("Connected to device!");
      }
      else {
        ALOGE("Cannot connect remote Target. Polling loop restarted.");
      }
    }

    INfcManager* pINfcManager = NfcService::getNfcManager();
    bool ret = pINfcManager->doCheckLlcp();
    if (ret == true) {
      ret = pINfcManager->doActivateLlcp();
      if (ret == true) {
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

  TechDiscoveredEvent* data = new TechDiscoveredEvent();
  data->techCount = 1;
  uint8_t techs[] = { NFC_TECH_P2P };
  data->techList = &techs;
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);
  delete data;
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
  std::vector<TagTechnology>& techList = pINfcTag->getTechList();
  std::vector<NfcTechnology> gonkTechList;
  int techCount = techList.size();

  for(int i = 0; i < techCount; i++) {
    gonkTechList.push_back(NfcUtil::convertTagTechToGonkFormat(techList[i]));
  }

  TechDiscoveredEvent* data = new TechDiscoveredEvent();
  data->techCount = techCount;
  data->techList = &gonkTechList.front();
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);
  delete data;

  pthread_t tid;
  pthread_create(&tid, NULL, pollingThreadFunc, pINfcTag);
}

void NfcService::handleTagLost(NfcEvent* event)
{
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_LOST, NULL);
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
          service->handleLlcpLinkActivation(event);
          break;
        case MSG_LLCP_LINK_DEACTIVATION:
          service->handleLlcpLinkDeactivation(event);
          break;
        case MSG_TAG_DISCOVERED:
          service->handleTagDiscovered(event);
          break;
        case MSG_TAG_LOST:
          service->handleTagLost(event);
          break;
        case MSG_CONFIG:
          service->handleConfigResponse(event);
          break;
        case MSG_READ_NDEF_DETAIL:
          service->handleReadNdefDetailResponse(event);
        case MSG_READ_NDEF:
          service->handleReadNdefResponse(event);
          break;
        case MSG_WRITE_NDEF:
          service->handleWriteNdefResponse(event);
          break;
        case MSG_CLOSE:
          service->handleCloseResponse(event);
          break;
        case MSG_SOCKET_CONNECTED:
          service->mMsgHandler->processNotification(NFC_NOTIFICATION_INITIALIZED , NULL);
          break;
        case MSG_PUSH_NDEF:
          service->handlePushNdefResponse(event);
          break;
        case MSG_MAKE_NDEF_READONLY:
          service->handleMakeNdefReadonlyResponse(event);
          break;
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
  if (sem_init(&thread_sem, 0, 0) == -1)
  {
    ALOGE("init_nfc_service Semaphore creation failed");
    abort();
  }

  if (pthread_create(&thread_id, NULL, serviceThreadFunc, this) != 0)
  {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }

  mMsgHandler = msgHandler;
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

int NfcService::handleConnect(int technology)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  int status = pINfcTag->connectWithStatus(technology);
  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS,  NULL);
  return status;
}

bool NfcService::handleConfigRequest()
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_CONFIG;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

bool NfcService::handleReadNdefDetailRequest()
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_READ_NDEF_DETAIL;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleConfigResponse(NfcEvent* event)
{
  mMsgHandler->processResponse(NFC_RESPONSE_CONFIG, NFC_ERROR_SUCCESS, NULL);
}

void NfcService::handleReadNdefDetailResponse(NfcEvent* event)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  NdefDetail* pNdefDetail = pINfcTag->ReadNdefDetail();

  if (pNdefDetail != NULL) {
    mMsgHandler->processResponse(NFC_RESPONSE_READ_NDEF_DETAILS, NFC_ERROR_SUCCESS, pNdefDetail);
  } else {
    //TODO can we notify null ndef detail?
  }

  delete pNdefDetail;
}

bool NfcService::handleReadNdefRequest()
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_READ_NDEF;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleReadNdefResponse(NfcEvent* event)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  NdefMessage* pNdefMessage = pINfcTag->findAndReadNdef();

  ALOGD("pNdefMessage=%p",pNdefMessage);
  if (pNdefMessage != NULL) {
    mMsgHandler->processResponse(NFC_RESPONSE_READ_NDEF, NFC_ERROR_SUCCESS, pNdefMessage);
  } else {
    //TODO can we notify null ndef?
  }

  delete pNdefMessage;
}

bool NfcService::handleWriteNdefRequest(NdefMessage* ndef)
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_WRITE_NDEF;
  event->data = ndef;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleWriteNdefResponse(NfcEvent* event)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(event->data);
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  bool result = pINfcTag->writeNdef(*ndef);
  delete ndef;
  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS, NULL);
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
  // TODO : If we call tag disconnect here, will keep trggering tag discover notification
  //        Need to check with DT what should we do here

  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS, NULL);
}

void NfcService::onSocketConnected()
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_SOCKET_CONNECTED;
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

bool NfcService::handlePushNdefRequest(NdefMessage* ndef)
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_PUSH_NDEF;
  event->data = ndef;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handlePushNdefResponse(NfcEvent* event)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(event->data);

  // TODO : Do we need create a thread to do this ? And can we use the same snep client each time ?
  SnepClient snep;
  snep.connect();
  snep.put(*ndef);
  snep.close();

  delete ndef;
  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS, NULL);
}

bool NfcService::handleMakeNdefReadonlyRequest()
{
  NfcEvent *event = new NfcEvent();
  event->type = MSG_MAKE_NDEF_READONLY;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleMakeNdefReadonlyResponse(NfcEvent* event)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface("NativeNfcTag"));
  bool result = pINfcTag->makeReadOnly();

  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS, NULL);
}
