/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "MessageHandler.h"
#include "INfcManager.h"
#include "INfcTag.h"
#include "IP2pDevice.h"
#include "DeviceHost.h"
#include "NfcGonkMessage.h"
#include "NfcService.h"
#include "NfcUtil.h"
#include "NfcDebug.h"
#include "P2pLinkManager.h"

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
  MSG_LOW_POWER,
  MSG_ENABLE,
} NfcEventType;

class NfcEvent {
public:
  NfcEvent (NfcEventType type) :mType(type) {}

  NfcEventType getType() { return mType; }

  int arg1;
  int arg2;
  void* obj;

private:
  NfcEventType mType;
};

static pthread_t thread_id;
static sem_t thread_sem;

NfcService* NfcService::sInstance = NULL;
NfcManager* NfcService::sNfcManager = NULL;

NfcService::NfcService()
 : mIsEnabled(false)
{
  mP2pLinkManager = new P2pLinkManager(this);
}

NfcService::~NfcService()
{
  delete mP2pLinkManager;
}

static void *serviceThreadFunc(void *arg)
{
  pthread_setname_np(pthread_self(), "NFCService thread");
  NfcService* service = reinterpret_cast<NfcService*>(arg);
  return service->eventLoop();
}

void NfcService::initialize(NfcManager* pNfcManager, MessageHandler* msgHandler)
{
  if (sem_init(&thread_sem, 0, 0) == -1) {
    ALOGE("%s: init_nfc_service Semaphore creation failed", FUNC);
    abort();
  }

  if (pthread_create(&thread_id, NULL, serviceThreadFunc, this) != 0) {
    ALOGE("%s: init_nfc_service pthread_create failed", FUNC);
    abort();
  }

  mMsgHandler = msgHandler;
  sNfcManager = pNfcManager;
}

void NfcService::notifyLlcpLinkActivated(IP2pDevice* pDevice)
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_LLCP_LINK_ACTIVATION);
  event->obj = reinterpret_cast<void*>(pDevice);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::notifyLlcpLinkDeactivated(IP2pDevice* pDevice)
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_LLCP_LINK_DEACTIVATION);
  event->obj = reinterpret_cast<void*>(pDevice);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::notifyTagDiscovered(INfcTag* pTag)
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_TAG_DISCOVERED);
  event->obj = reinterpret_cast<void*>(pTag);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::notifyTagLost()
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_TAG_LOST);
  event->obj = NULL;
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::notifySEFieldActivated()
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_SE_FIELD_ACTIVATED);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::notifySEFieldDeactivated()
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_SE_FIELD_DEACTIVATED);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::notifySETransactionListeners()
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_SE_NOTIFY_TRANSACTION_LISTENERS);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::handleLlcpLinkDeactivation(NfcEvent* event)
{
  ALOGD("%s: enter", FUNC);

  void* pDevice = event->obj;
  IP2pDevice* pIP2pDevice = reinterpret_cast<IP2pDevice*>(pDevice);

  if (pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET) {
    pIP2pDevice->disconnect();
  }

  mP2pLinkManager->onLlcpDeactivated();
}

void NfcService::handleLlcpLinkActivation(NfcEvent* event)
{
  ALOGD("%s: enter", FUNC);
  void* pDevice = event->obj;
  IP2pDevice* pIP2pDevice = reinterpret_cast<IP2pDevice*>(pDevice);

  if (pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET ||
      pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_INITIATOR) {
    if (pIP2pDevice->getMode() == NfcDepEndpoint::MODE_P2P_TARGET) {
      if (pIP2pDevice->connect()) {
        ALOGD("%s: Connected to device!", FUNC);
      }
      else {
        ALOGE("%s: Cannot connect remote Target. Polling loop restarted.", FUNC);
      }
    }

    INfcManager* pINfcManager = NfcService::getNfcManager();
    bool ret = pINfcManager->checkLlcp();
    if (ret == true) {
      ret = pINfcManager->activateLlcp();
      if (ret == true) {
        ALOGD("%s: Target Activate LLCP OK", FUNC);
      } else {
        ALOGE("%s: doActivateLLcp failed", FUNC);
      }
    } else {
      ALOGE("%s: doCheckLLcp failed", FUNC);
    }
  } else {
    ALOGE("%s: Unknown LLCP P2P mode", FUNC);
    //stop();
  }

  mP2pLinkManager->onLlcpActivated();

  TechDiscoveredEvent* data = new TechDiscoveredEvent();
  data->isNewSession = true;
  data->techCount = 1;
  uint8_t techs[] = { NFC_TECH_P2P };
  data->techList = &techs;
  data->ndefMsgCount = 0;
  data->ndefMsg = NULL;
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);
  delete data;
  ALOGD("%s: exit", FUNC);
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

  NfcService::Instance()->notifyTagLost();
  return NULL;
}

void NfcService::handleTagDiscovered(NfcEvent* event)
{
  void* pTag = event->obj;
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(pTag);

  // To get complete tag information, need to call read ndef first.
  // In readNdef function, it will add NDEF related info in NfcTagManager.
  NdefMessage* pNdefMessage = pINfcTag->readNdef();

  // Do the following after read ndef.
  std::vector<TagTechnology>& techList = pINfcTag->getTechList();
  int techCount = techList.size();

  uint8_t* gonkTechList = new uint8_t[techCount];
  for(int i = 0; i < techCount; i++) {
    gonkTechList[i] = (uint8_t)NfcUtil::convertTagTechToGonkFormat(techList[i]);
  }

  TechDiscoveredEvent* data = new TechDiscoveredEvent();
  data->isNewSession = true;
  data->techCount = techCount;
  data->techList = gonkTechList;
  data->ndefMsgCount = pNdefMessage ? 1 : 0;
  data->ndefMsg = pNdefMessage;
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);

  delete gonkTechList;
  delete data;

  pthread_t tid;
  pthread_create(&tid, NULL, pollingThreadFunc, pINfcTag);
}

void NfcService::handleTagLost(NfcEvent* event)
{
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_LOST, NULL);
}

void* NfcService::eventLoop()
{
  ALOGD("%s: NFCService started", FUNC);
  while(true) {
    if(sem_wait(&thread_sem)) {
      ALOGE("%s: Failed to wait for semaphore", FUNC);
      abort();
    }

    while (!mQueue.empty()) {
      NfcEvent* event = *mQueue.begin();
      mQueue.erase(mQueue.begin());
      NfcEventType eventType = event->getType();

      ALOGD("%s: NFCService msg=%d", FUNC, eventType);
      switch(eventType) {
        case MSG_LLCP_LINK_ACTIVATION:
          handleLlcpLinkActivation(event);
          break;
        case MSG_LLCP_LINK_DEACTIVATION:
          handleLlcpLinkDeactivation(event);
          break;
        case MSG_TAG_DISCOVERED:
          handleTagDiscovered(event);
          break;
        case MSG_TAG_LOST:
          handleTagLost(event);
          break;
        case MSG_CONFIG:
          handleConfigResponse(event);
          break;
        case MSG_READ_NDEF_DETAIL:
          handleReadNdefDetailResponse(event);
        case MSG_READ_NDEF:
          handleReadNdefResponse(event);
          break;
        case MSG_WRITE_NDEF:
          handleWriteNdefResponse(event);
          break;
        case MSG_CLOSE:
          handleCloseResponse(event);
          break;
        case MSG_SOCKET_CONNECTED:
          mMsgHandler->processNotification(NFC_NOTIFICATION_INITIALIZED , NULL);
          break;
        case MSG_PUSH_NDEF:
          handlePushNdefResponse(event);
          break;
        case MSG_MAKE_NDEF_READONLY:
          handleMakeNdefReadonlyResponse(event);
          break;
        case MSG_LOW_POWER:
          handleEnterLowPowerResponse(event);
          break;
        case MSG_ENABLE:
          handleEnableResponse(event);
          break;
        default:
          ALOGE("%s: NFCService bad message", FUNC);
          abort();
      }

      //TODO delete event->data?
      delete event;
    }
  }
}

NfcService* NfcService::Instance() {
    if (!sInstance)
        sInstance = new NfcService();
    return sInstance;
}

INfcManager* NfcService::getNfcManager()
{
  return reinterpret_cast<INfcManager*>(NfcService::sNfcManager);
}

bool NfcService::handleDisconnect()
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
  bool result = pINfcTag->disconnect();
  return result;
}

int NfcService::handleConnect(int technology)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
  int status = pINfcTag->connect(technology);
  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS,  NULL);
  return status;
}

bool NfcService::handleConfigRequest(int powerLevel)
{
  NfcEvent *event = new NfcEvent(MSG_CONFIG);
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

bool NfcService::handleReadNdefDetailRequest()
{
  NfcEvent *event = new NfcEvent(MSG_READ_NDEF_DETAIL);
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
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
  NdefDetail* pNdefDetail = pINfcTag->readNdefDetail();

  if (pNdefDetail != NULL) {
    mMsgHandler->processResponse(NFC_RESPONSE_READ_NDEF_DETAILS, NFC_ERROR_SUCCESS, pNdefDetail);
  } else {
    //TODO can we notify null ndef detail?
  }

  delete pNdefDetail;
}

bool NfcService::handleReadNdefRequest()
{
  NfcEvent *event = new NfcEvent(MSG_READ_NDEF);
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleReadNdefResponse(NfcEvent* event)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
  NdefMessage* pNdefMessage = pINfcTag->readNdef();

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
  NfcEvent *event = new NfcEvent(MSG_WRITE_NDEF);
  event->obj = ndef;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleWriteNdefResponse(NfcEvent* event)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(event->obj);

  // Use single API wirte to send data.
  // nfcd check current connection is p2p or tag.
  if (ndef) {
    if (mP2pLinkManager->isLlcpActive()) {
      mP2pLinkManager->push(*ndef);
    } else {
      INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
      pINfcTag->writeNdef(*ndef);
    }
  } else {
    ALOGE("%s: empty NDEF message", FUNC);
  }

  delete ndef;
  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS, NULL);
}

void NfcService::handleCloseRequest()
{
  NfcEvent *event = new NfcEvent(MSG_CLOSE);
  mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::handleCloseResponse(NfcEvent* event)
{
  // TODO : If we call tag disconnect here, will keep trggering tag discover notification
  //        Need to check with DT what should we do here

  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS, NULL);
}

void NfcService::onConnected()
{
  NfcEvent *event = new NfcEvent(MSG_SOCKET_CONNECTED);
  mQueue.push_back(event);
  sem_post(&thread_sem);
}

bool NfcService::handlePushNdefRequest(NdefMessage* ndef)
{
  NfcEvent *event = new NfcEvent(MSG_PUSH_NDEF);
  event->obj = ndef;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handlePushNdefResponse(NfcEvent* event)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(event->obj);

  mP2pLinkManager->push(*ndef);

  delete ndef;
  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS, NULL);
}

bool NfcService::handleMakeNdefReadonlyRequest()
{
  NfcEvent *event = new NfcEvent(MSG_MAKE_NDEF_READONLY);
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleMakeNdefReadonlyResponse(NfcEvent* event)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
  bool result = pINfcTag->makeReadOnly();

  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_ERROR_SUCCESS, NULL);
}

bool NfcService::handleEnterLowPowerRequest(bool enter)
{
  NfcEvent *event = new NfcEvent(MSG_LOW_POWER);
  event->arg1 = enter;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleEnterLowPowerResponse(NfcEvent* event)
{
  bool enter = event->arg1;
  if (enter)
    sNfcManager->disableDiscovery();
  else
    sNfcManager->enableDiscovery();

  mMsgHandler->processResponse(NFC_RESPONSE_CONFIG, NFC_ERROR_SUCCESS, NULL);
}

bool NfcService::handleEnableRequest(bool enable)
{
  NfcEvent *event = new NfcEvent(MSG_ENABLE);
  event->arg1 = enable;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::handleEnableResponse(NfcEvent* event)
{
  bool enable = event->arg1;
  if (enable) {
    enableNfc();
  } else {
    disableNfc();
  }
  mMsgHandler->processResponse(NFC_RESPONSE_CONFIG, NFC_ERROR_SUCCESS, NULL);
}

void NfcService::enableNfc()
{
  ALOGD("%s: enter", FUNC);

  if (mIsEnabled) {
    ALOGW("%s: NFC is already enabled", FUNC);
    return;
  }

  sNfcManager->initialize();

  if (mP2pLinkManager)
    mP2pLinkManager->enableDisable(true);

  // Enable discovery MUST SNEP server is established.
  // Otherwise, P2P device will not be discovered.
  sNfcManager->enableDiscovery();

  mIsEnabled = true;

  ALOGD("%s: exit", FUNC);
}

void NfcService::disableNfc()
{
  ALOGD("%s: enter", FUNC);

  if (!mIsEnabled) {
    ALOGW("%s: NFC is already disabled", FUNC);
    return;
  }

  if (mP2pLinkManager)
    mP2pLinkManager->enableDisable(false);

  sNfcManager->disableDiscovery();

  sNfcManager->deinitialize();

  mIsEnabled = false;

  ALOGD("%s: exit", FUNC);
}

void NfcService::onP2pReceivedNdef(NdefMessage* ndef)
{
  TechDiscoveredEvent* data = new TechDiscoveredEvent();
  data->isNewSession = false;
  data->techCount = 2;
  uint8_t techs[] = { NFC_TECH_P2P, NFC_TECH_NDEF };
  data->techList = &techs;
  data->ndefMsgCount = 1;
  data->ndefMsg = ndef;
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);
  delete data;
}
