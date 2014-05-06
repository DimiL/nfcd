/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <memory>

#include "MessageHandler.h"
#include "INfcManager.h"
#include "INfcTag.h"
#include "IP2pDevice.h"
#include "DeviceHost.h"
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

typedef enum {
  STATE_NFC_OFF = 0,
  STATE_NFC_ON_DISCOVERY_OFF,
  STATE_NFC_ON_DISCOVERY_ON,
} NfcState;

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
 : mState(STATE_NFC_OFF)
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
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_LOST, NULL);

  // TODO : Handle in SE related code in the future ?
  // Bug 961667 - [NFC] Multiple pairing when tapped phones together
  // RF field should be reset when link deactivate.
  ALOGD("%s: reset RF field", FUNC);
  sNfcManager->resetRFField();
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
          break;
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
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                      (sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
  bool result = pINfcTag->disconnect();
  return result;
}

void NfcService::handleConnect(int tech)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                      (sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));

  NfcErrorCode code = !!pINfcTag ?
                      (pINfcTag->connect(tech) ? NFC_SUCCESS : NFC_ERROR_DISCONNECT) :
                      NFC_ERROR_NOT_SUPPORTED;

  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, code, NULL);
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
  mMsgHandler->processResponse(NFC_RESPONSE_CONFIG, NFC_SUCCESS, NULL);
}

void NfcService::handleReadNdefDetailResponse(NfcEvent* event)
{
  NfcResponseType resType = NFC_RESPONSE_READ_NDEF_DETAILS;

  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                      (sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
  if (!pINfcTag) {
    mMsgHandler->processResponse(resType, NFC_ERROR_NOT_SUPPORTED, NULL);
    return;
  }

  std::auto_ptr<NdefDetail> pNdefDetail(pINfcTag->readNdefDetail());
  if (!pNdefDetail.get()) {
    mMsgHandler->processResponse(resType, NFC_ERROR_READ, NULL);
    return;
  }

  mMsgHandler->processResponse(resType, NFC_SUCCESS, pNdefDetail.get());
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
  NfcResponseType resType = NFC_RESPONSE_READ_NDEF;

  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));
  if (!pINfcTag) {
    mMsgHandler->processResponse(resType, NFC_ERROR_NOT_SUPPORTED, NULL);
    return;
  }

  std::auto_ptr<NdefMessage> pNdefMessage(pINfcTag->readNdef());
  if (!pNdefMessage.get()) {
    mMsgHandler->processResponse(resType, NFC_ERROR_READ, NULL);
    return;
  }

  mMsgHandler->processResponse(resType, NFC_SUCCESS, pNdefMessage.get());
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
  NfcResponseType resType = NFC_RESPONSE_GENERAL;
  NfcErrorCode code = NFC_SUCCESS;

  std::auto_ptr<NdefMessage> pNdef(reinterpret_cast<NdefMessage*>(event->obj));
  if (!pNdef.get()) {
    mMsgHandler->processResponse(resType, NFC_ERROR_INVALID_PARAM, NULL);
    return;
  }

  // Use single API wirte to send data.
  // nfcd check current connection is p2p or tag.
  if (mP2pLinkManager->isLlcpActive()) {
    mP2pLinkManager->push(*pNdef.get());
  } else {
    INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                        (sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));

    code = !!pINfcTag ?
           (pINfcTag->writeNdef(*pNdef.get()) ? NFC_SUCCESS : NFC_ERROR_IO) :
           NFC_ERROR_NOT_SUPPORTED;
  }

  mMsgHandler->processResponse(resType, code, NULL);
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

  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, NFC_SUCCESS, NULL);
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
  NfcResponseType resType = NFC_RESPONSE_GENERAL;

  std::auto_ptr<NdefMessage> pNdef(reinterpret_cast<NdefMessage*>(event->obj));
  if (!pNdef.get()) {
    mMsgHandler->processResponse(resType, NFC_ERROR_INVALID_PARAM, NULL);
    return;
  }

  mP2pLinkManager->push(*pNdef.get());

  mMsgHandler->processResponse(resType, NFC_SUCCESS, NULL);
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
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                      (sNfcManager->queryInterface(INTERFACE_TAG_MANAGER));

  NfcErrorCode code = !!pINfcTag ?
                      (pINfcTag->makeReadOnly() ? NFC_SUCCESS : NFC_ERROR_IO) :
                      NFC_ERROR_NOT_SUPPORTED;

  mMsgHandler->processResponse(NFC_RESPONSE_GENERAL, code, NULL);
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
  bool low = event->arg1;
  NfcErrorCode code = low ? disableDiscovery() : enableDiscovery();

  mMsgHandler->processResponse(NFC_RESPONSE_CONFIG, code, NULL);
}

bool NfcService::handleEnableRequest(bool enable)
{
  NfcEvent *event = new NfcEvent(MSG_ENABLE);
  event->arg1 = enable;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

/**
 * There are two case for enable:
 * 1. NFC is off -> enable NFC and then enable discovery.
 * 2. NFC is already on but discovery mode is off -> enable discovery.
 */
void NfcService::handleEnableResponse(NfcEvent* event)
{
  NfcErrorCode code = NFC_SUCCESS;

  bool enable = event->arg1;
  if (enable) {
    // Do different action depends on current state.
    if (mState == STATE_NFC_ON_DISCOVERY_OFF) {
      code = enableDiscovery();
    } else if (mState == STATE_NFC_OFF) {
      code = enableNfc();
      if (code != NFC_SUCCESS) {
        goto TheEnd;
      }
      code = enableDiscovery();
    }
  } else {
    code = disableNfc();
    if (code != NFC_SUCCESS) {
      goto TheEnd;
    }
    code = disableDiscovery();
  }
TheEnd:
  mMsgHandler->processResponse(NFC_RESPONSE_CONFIG, code, NULL);
}

NfcErrorCode NfcService::enableNfc()
{
  ALOGD("%s: enter", FUNC);

  if (mState != STATE_NFC_OFF) {
    return NFC_ERROR_NFC_ALREADY_ON;
  }

  if (!sNfcManager->initialize()) {
    return NFC_ERROR_INITIALIZE_FAIL;
  }

  if (mP2pLinkManager) {
    mP2pLinkManager->enableDisable(true);
  }

  mState = STATE_NFC_ON_DISCOVERY_OFF;

  ALOGD("%s: exit", FUNC);
  return NFC_SUCCESS;
}

NfcErrorCode NfcService::disableNfc()
{
  ALOGD("%s: enter", FUNC);

  if (mState == STATE_NFC_OFF) {
    ALOGW("%s: NFC is already disabled", FUNC);
    return NFC_ERROR_NFC_ALREADY_OFF;
  }

  if (mP2pLinkManager) {
    mP2pLinkManager->enableDisable(false);
  }

  if (!sNfcManager->deinitialize()) {
    return NFC_ERROR_DEINITIALIZE_FAIL;
  }

  mState = STATE_NFC_OFF;

  ALOGD("%s: exit", FUNC);
  return NFC_SUCCESS;
}

NfcErrorCode NfcService::enableDiscovery()
{
  ALOGD("%s: enter", FUNC);

  if (mState != STATE_NFC_ON_DISCOVERY_OFF) {
    return NFC_ERROR_ALREADY_DISCOVERY_ON;
  }

  if (!sNfcManager->enableDiscovery()) {
    return NFC_ERROR_FAIL_ENABLE_DISCOVERY;
  }

  mState = STATE_NFC_ON_DISCOVERY_ON;

  ALOGD("%s: exit", FUNC);
  return NFC_SUCCESS;
}

NfcErrorCode NfcService::disableDiscovery()
{
  ALOGD("%s: enter", FUNC);

  if (mState != STATE_NFC_ON_DISCOVERY_ON) {
    return NFC_ERROR_ALREADY_DISCOVERY_OFF;
  }

  if (!sNfcManager->disableDiscovery()) {
    return NFC_ERROR_FAIL_DISABLE_DISCOVERY;
  }

  mState = STATE_NFC_ON_DISCOVERY_OFF;

  ALOGD("%s: exit", FUNC);
  return NFC_SUCCESS;
}

void NfcService::onP2pReceivedNdef(NdefMessage* ndef)
{
  TechDiscoveredEvent* data = new TechDiscoveredEvent();
  data->isNewSession = false;
  data->techCount = 2;
  uint8_t techs[] = { NFC_TECH_P2P, NFC_TECH_NDEF };
  data->techList = &techs;
  data->ndefMsgCount = ndef ? 1 : 0;
  data->ndefMsg = ndef;
  mMsgHandler->processNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);
  delete data;
}
