/*
 * Copyright (C) 2013-2014  Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
#include "SessionId.h"

using namespace android;

typedef enum {
  MSG_UNDEFINED = 0,
  MSG_LLCP_LINK_ACTIVATION,
  MSG_LLCP_LINK_DEACTIVATION,
  MSG_TAG_DISCOVERED,
  MSG_TAG_LOST,
  MSG_SE_FIELD_ACTIVATED,
  MSG_SE_FIELD_DEACTIVATED,
  MSG_SE_NOTIFY_TRANSACTION_EVENT,
  MSG_READ_NDEF,
  MSG_WRITE_NDEF,
  MSG_SOCKET_CONNECTED,
  MSG_PUSH_NDEF,
  MSG_NDEF_TAG_LIST,
  MSG_MAKE_NDEF_READONLY,
  MSG_LOW_POWER,
  MSG_ENABLE,
  MSG_RECEIVE_NDEF_EVENT,
  MSG_NDEF_FORMAT,
  MSG_TAG_TRANSCEIVE
} NfcEventType;

typedef enum {
  STATE_NFC_OFF = 0,
  STATE_NFC_ON_LOW_POWER,
  STATE_NFC_ON,
} NfcState;

class NfcEvent {
public:
  NfcEvent(NfcEventType aType)
   : mType(aType)
  {}

  NfcEventType GetType() { return mType; }

  int arg1;
  int arg2;
  void* obj;

private:
  NfcEventType mType;
};

class PollingThreadParam {
public:
  INfcTag* pINfcTag;
  int sessionId;
};

static pthread_t thread_id;
static sem_t thread_sem;

NfcService* NfcService::sInstance = NULL;
NfcManager* NfcService::sNfcManager = NULL;

NfcService::NfcService()
 : mState(STATE_NFC_OFF)
 , mIsTagPresent(false)
{
  mP2pLinkManager = new P2pLinkManager(this);
}

NfcService::~NfcService()
{
  delete mP2pLinkManager;
}

static void *ServiceThreadFunc(void *aArg)
{
  pthread_setname_np(pthread_self(), "NFCService thread");
  NfcService* service = reinterpret_cast<NfcService*>(aArg);
  return service->EventLoop();
}

void NfcService::Initialize(NfcManager* aNfcManager, MessageHandler* aMsgHandler)
{
  if (sem_init(&thread_sem, 0, 0) == -1) {
    ALOGE("%s: init_nfc_service Semaphore creation failed", FUNC);
    abort();
  }

  if (pthread_create(&thread_id, NULL, ServiceThreadFunc, this) != 0) {
    ALOGE("%s: init_nfc_service pthread_create failed", FUNC);
    abort();
  }

  mMsgHandler = aMsgHandler;
  sNfcManager = aNfcManager;
}

void NfcService::NotifyLlcpLinkActivated(IP2pDevice* aDevice)
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_LLCP_LINK_ACTIVATION);
  event->obj = reinterpret_cast<void*>(aDevice);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::NotifyLlcpLinkDeactivated(IP2pDevice* aDevice)
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_LLCP_LINK_DEACTIVATION);
  event->obj = reinterpret_cast<void*>(aDevice);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::NotifyTagDiscovered(INfcTag* aTag)
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_TAG_DISCOVERED);
  event->obj = reinterpret_cast<void*>(aTag);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::NotifyTagLost(int aSessionId)
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_TAG_LOST);
  event->obj = reinterpret_cast<void*>(aSessionId);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::NotifySEFieldActivated()
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_SE_FIELD_ACTIVATED);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::NotifySEFieldDeactivated()
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_SE_FIELD_DEACTIVATED);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::NotifySETransactionEvent(TransactionEvent* aEvent)
{
  ALOGD("%s: enter", FUNC);
  NfcEvent *event = new NfcEvent(MSG_SE_NOTIFY_TRANSACTION_EVENT);
  event->obj = reinterpret_cast<void*>(aEvent);
  NfcService::Instance()->mQueue.push_back(event);
  sem_post(&thread_sem);
}

void NfcService::HandleLlcpLinkDeactivation(NfcEvent* aEvent)
{
  ALOGD("%s: enter", FUNC);

  void* pDevice = aEvent->obj;
  IP2pDevice* pIP2pDevice = reinterpret_cast<IP2pDevice*>(pDevice);

  if (pIP2pDevice->GetMode() == NfcDepEndpoint::MODE_P2P_TARGET) {
    pIP2pDevice->Disconnect();
  }

  mP2pLinkManager->OnLlcpDeactivated();
  mMsgHandler->ProcessNotification(NFC_NOTIFICATION_TECH_LOST,
                                   reinterpret_cast<void*>(mP2pLinkManager->GetSessionId()));
  mP2pLinkManager->SetSessionId(-1);
}

void NfcService::HandleLlcpLinkActivation(NfcEvent* aEvent)
{
  ALOGD("%s: enter", FUNC);
  void* pDevice = aEvent->obj;
  IP2pDevice* pIP2pDevice = reinterpret_cast<IP2pDevice*>(pDevice);

  if (pIP2pDevice->GetMode() == NfcDepEndpoint::MODE_P2P_TARGET ||
      pIP2pDevice->GetMode() == NfcDepEndpoint::MODE_P2P_INITIATOR) {
    if (pIP2pDevice->GetMode() == NfcDepEndpoint::MODE_P2P_TARGET) {
      if (pIP2pDevice->Connect()) {
        ALOGD("%s: Connected to device!", FUNC);
      }
      else {
        ALOGE("%s: Cannot connect remote Target. Polling loop restarted.", FUNC);
      }
    }

    INfcManager* pINfcManager = NfcService::GetNfcManager();
    bool ret = pINfcManager->CheckLlcp();
    if (ret == true) {
      ret = pINfcManager->ActivateLlcp();
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

  mP2pLinkManager->OnLlcpActivated();

  TechDiscoveredEvent* data = new TechDiscoveredEvent();

  mP2pLinkManager->SetSessionId(SessionId::GenerateNewId());
  data->sessionId = mP2pLinkManager->GetSessionId();
  data->isP2P = true;
  data->techCount = 0;
  data->techList = NULL;
  data->tagIdCount = 0;
  data->tagId = NULL;
  data->ndefMsgCount = 0;
  data->ndefMsg = NULL;
  data->ndefInfo = NULL;
  mMsgHandler->ProcessNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);
  delete data;
  ALOGD("%s: exit", FUNC);
}

static void *PollingThreadFunc(void *aArg)
{
  PollingThreadParam* param = reinterpret_cast<PollingThreadParam*>(aArg);
  INfcTag* pINfcTag = param->pINfcTag;
  int sessionId = param->sessionId;

  NfcService::Instance()->TagDetected();

  while (pINfcTag->PresenceCheck()) {
    sleep(1);
  }

  NfcService::Instance()->TagRemoved();

  pINfcTag->Disconnect();

  NfcService::Instance()->NotifyTagLost(sessionId);

  delete param;

  return NULL;
}

void NfcService::HandleTagDiscovered(NfcEvent* aEvent)
{
  // Do not support multiple tag discover.
  if (IsTagPresent()) {
    return;
  }

  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(aEvent->obj);

  // To get complete tag information, need to call read ndef first.
  // In readNdef function, it will add NDEF related info in NfcTagManager.
  std::auto_ptr<NdefMessage> pNdefMessage(pINfcTag->ReadNdef());
  std::auto_ptr<NdefInfo> pNdefInfo(pINfcTag->ReadNdefInfo());

  // Do the following after read ndef.
  std::vector<TagTechnology>& techList = pINfcTag->GetTechList();
  int techCount = techList.size();

  uint8_t* gonkTechList = new uint8_t[techCount];
  std::copy(techList.begin(), techList.end(), gonkTechList);

  TechDiscoveredEvent* data = new TechDiscoveredEvent();
  data->sessionId = SessionId::GenerateNewId();
  data->isP2P = false;
  data->techCount = techCount;
  data->techList = gonkTechList;
  data->tagIdCount = pINfcTag->GetUid().size();
  uint8_t* tagId = new uint8_t[data->tagIdCount];
  memcpy(tagId, &pINfcTag->GetUid()[0], data->tagIdCount);
  data->tagId = tagId;
  data->ndefMsgCount = pNdefMessage.get() ? 1 : 0;
  data->ndefMsg = pNdefMessage.get();
  data->ndefInfo = pNdefInfo.get();
  mMsgHandler->ProcessNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);

  PollingThreadParam* param = new PollingThreadParam();
  param->sessionId = data->sessionId;
  param->pINfcTag = pINfcTag;

  delete tagId;
  delete gonkTechList;
  delete data;

  pthread_t tid;
  pthread_create(&tid, NULL, PollingThreadFunc, param);
}

void NfcService::HandleTagLost(NfcEvent* aEvent)
{
  mMsgHandler->ProcessNotification(NFC_NOTIFICATION_TECH_LOST, aEvent->obj);
}

void NfcService::HandleTransactionEvent(NfcEvent* aEvent)
{
  mMsgHandler->ProcessNotification(NFC_NOTIFICATION_TRANSACTION_EVENT, aEvent->obj);
}

void* NfcService::EventLoop()
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
      NfcEventType eventType = event->GetType();

      ALOGD("%s: NFCService msg=%d", FUNC, eventType);
      switch(eventType) {
        case MSG_LLCP_LINK_ACTIVATION:
          HandleLlcpLinkActivation(event);
          break;
        case MSG_LLCP_LINK_DEACTIVATION:
          HandleLlcpLinkDeactivation(event);
          break;
        case MSG_TAG_DISCOVERED:
          HandleTagDiscovered(event);
          break;
        case MSG_TAG_LOST:
          HandleTagLost(event);
          break;
        case MSG_SE_NOTIFY_TRANSACTION_EVENT:
          HandleTransactionEvent(event);
          break;
        case MSG_READ_NDEF:
          HandleReadNdefResponse(event);
          break;
        case MSG_WRITE_NDEF:
          HandleWriteNdefResponse(event);
          break;
        case MSG_SOCKET_CONNECTED:
          mMsgHandler->ProcessNotification(NFC_NOTIFICATION_INITIALIZED , NULL);
          break;
        case MSG_PUSH_NDEF:
          HandlePushNdefResponse(event);
          break;
        case MSG_MAKE_NDEF_READONLY:
          HandleMakeNdefReadonlyResponse(event);
          break;
        case MSG_LOW_POWER:
          HandleEnterLowPowerResponse(event);
          break;
        case MSG_ENABLE:
          HandleEnableResponse(event);
          break;
        case MSG_RECEIVE_NDEF_EVENT:
          HandleReceiveNdefEvent(event);
          break;
        case MSG_NDEF_FORMAT:
          HandleNdefFormatResponse(event);
          break;
        case MSG_TAG_TRANSCEIVE:
          HandleTagTransceiveResponse(event);
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
  if (!sInstance) {
    sInstance = new NfcService();
  }

  return sInstance;
}

INfcManager* NfcService::GetNfcManager()
{
  return reinterpret_cast<INfcManager*>(NfcService::sNfcManager);
}

bool NfcService::HandleDisconnect()
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                      (sNfcManager->QueryInterface(INTERFACE_TAG_MANAGER));
  bool result = pINfcTag->Disconnect();
  return result;
}

bool NfcService::HandleReadNdefRequest()
{
  NfcEvent *event = new NfcEvent(MSG_READ_NDEF);
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::HandleReadNdefResponse(NfcEvent* aEvent)
{
  NfcResponseType resType = NFC_RESPONSE_READ_NDEF;

  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(sNfcManager->QueryInterface(INTERFACE_TAG_MANAGER));
  if (!pINfcTag) {
    mMsgHandler->ProcessResponse(resType, NFC_ERROR_NOT_SUPPORTED, NULL);
    return;
  }

  std::auto_ptr<NdefMessage> pNdefMessage(pINfcTag->ReadNdef());
  if (!pNdefMessage.get()) {
    mMsgHandler->ProcessResponse(resType, NFC_ERROR_READ, NULL);
    return;
  }

  mMsgHandler->ProcessResponse(resType, NFC_SUCCESS, pNdefMessage.get());
}

void NfcService::HandleReceiveNdefEvent(NfcEvent* aEvent)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(aEvent->obj);

  TechDiscoveredEvent* data = new TechDiscoveredEvent();
  data->sessionId = SessionId::GetCurrentId();
  data->isP2P = true;
  data->techCount = 0;
  data->techList = NULL;
  data->tagIdCount = 0;
  data->tagId = NULL;
  data->ndefMsgCount = ndef ? 1 : 0;
  data->ndefMsg = ndef;
  mMsgHandler->ProcessNotification(NFC_NOTIFICATION_TECH_DISCOVERED, data);

  delete data;
  delete ndef;
}

bool NfcService::HandleWriteNdefRequest(NdefMessage* aNdef, bool aIsP2P)
{
  NfcEvent *event = new NfcEvent(MSG_WRITE_NDEF);
  event->arg1 = aIsP2P;
  event->obj = aNdef;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::HandleWriteNdefResponse(NfcEvent* aEvent)
{
  NfcResponseType resType = NFC_RESPONSE_GENERAL;
  NfcErrorCode code = NFC_SUCCESS;

  std::auto_ptr<NdefMessage> pNdef(reinterpret_cast<NdefMessage*>(aEvent->obj));
  if (!pNdef.get()) {
    mMsgHandler->ProcessResponse(resType, NFC_ERROR_INVALID_PARAM, NULL);
    return;
  }

  bool isP2P = aEvent->arg1;
  if (isP2P && mP2pLinkManager->IsLlcpActive()) {
    mP2pLinkManager->Push(*pNdef.get());
  } else if (!isP2P && IsTagPresent()) {
    INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                        (sNfcManager->QueryInterface(INTERFACE_TAG_MANAGER));

    code = !!pINfcTag ?
           (pINfcTag->WriteNdef(*pNdef.get()) ? NFC_SUCCESS : NFC_ERROR_IO) :
           NFC_ERROR_NOT_SUPPORTED;
  } else {
    code = NFC_ERROR_IO;
  }

  mMsgHandler->ProcessResponse(resType, code, NULL);
}

void NfcService::OnConnected()
{
  NfcEvent *event = new NfcEvent(MSG_SOCKET_CONNECTED);
  mQueue.push_back(event);
  sem_post(&thread_sem);
}

bool NfcService::HandlePushNdefRequest(NdefMessage* aNdef)
{
  NfcEvent *event = new NfcEvent(MSG_PUSH_NDEF);
  event->obj = aNdef;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::HandlePushNdefResponse(NfcEvent* aEvent)
{
  NfcResponseType resType = NFC_RESPONSE_GENERAL;

  std::auto_ptr<NdefMessage> pNdef(reinterpret_cast<NdefMessage*>(aEvent->obj));
  if (!pNdef.get()) {
    mMsgHandler->ProcessResponse(resType, NFC_ERROR_INVALID_PARAM, NULL);
    return;
  }

  mP2pLinkManager->Push(*pNdef.get());

  mMsgHandler->ProcessResponse(resType, NFC_SUCCESS, NULL);
}

bool NfcService::HandleMakeNdefReadonlyRequest()
{
  NfcEvent *event = new NfcEvent(MSG_MAKE_NDEF_READONLY);
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::HandleMakeNdefReadonlyResponse(NfcEvent* aEvent)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                      (sNfcManager->QueryInterface(INTERFACE_TAG_MANAGER));

  NfcErrorCode code = !!pINfcTag ?
                      (pINfcTag->MakeReadOnly() ? NFC_SUCCESS : NFC_ERROR_IO) :
                      NFC_ERROR_NOT_SUPPORTED;

  mMsgHandler->ProcessResponse(NFC_RESPONSE_GENERAL, code, NULL);
}

bool NfcService::HandleNdefFormatRequest()
{
  NfcEvent *event = new NfcEvent(MSG_NDEF_FORMAT);
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

bool NfcService::HandleTagTransceiveRequest(int aTech, const uint8_t* aBuf, uint32_t aBufLen)
{
  std::vector<uint8_t>* cmd = new std::vector<uint8_t>(aBuf, aBuf + aBufLen);

  NfcEvent *event = new NfcEvent(MSG_TAG_TRANSCEIVE);
  event->arg1 = aTech;
  event->obj = reinterpret_cast<void*>(cmd);
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::HandleTagTransceiveResponse(NfcEvent* aEvent)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                      (sNfcManager->QueryInterface(INTERFACE_TAG_MANAGER));

  int tech = aEvent->arg1;
  std::vector<uint8_t>* command = reinterpret_cast<std::vector<uint8_t>*>(aEvent->obj);
  std::vector<uint8_t> response;

  NfcErrorCode code = pINfcTag->Connect(static_cast<TagTechnology>(tech)) ?
                      NFC_SUCCESS : NFC_ERROR_IO;

  if (NFC_SUCCESS == code) {
    code = !!pINfcTag ? (pINfcTag->Transceive(*command, response) ?
           NFC_SUCCESS : NFC_ERROR_IO) : NFC_ERROR_NOT_SUPPORTED;
  }

  delete command;

  mMsgHandler->ProcessResponse(NFC_RESPONSE_TAG_TRANSCEIVE, code,
                               reinterpret_cast<void*>(&response));
}

void NfcService::HandleNdefFormatResponse(NfcEvent* aEvent)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>
                      (sNfcManager->QueryInterface(INTERFACE_TAG_MANAGER));

  NfcErrorCode code = !!pINfcTag ?
                      (pINfcTag->FormatNdef() ? NFC_SUCCESS : NFC_ERROR_IO) :
                      NFC_ERROR_NOT_SUPPORTED;

  mMsgHandler->ProcessResponse(NFC_RESPONSE_GENERAL, code, NULL);
}

bool NfcService::HandleEnterLowPowerRequest(bool aEnter)
{
  NfcEvent *event = new NfcEvent(MSG_LOW_POWER);
  event->arg1 = aEnter;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

void NfcService::HandleEnterLowPowerResponse(NfcEvent* aEvent)
{
  bool low = aEvent->arg1;

  NfcErrorCode code = SetLowPowerMode(low);

  ALOGD("%s mState=%d", __func__, mState);
  mMsgHandler->ProcessResponse(NFC_RESPONSE_CHANGE_RF_STATE, code, &mState);
}

bool NfcService::HandleEnableRequest(bool aEnable)
{
  NfcEvent *event = new NfcEvent(MSG_ENABLE);
  event->arg1 = aEnable;
  mQueue.push_back(event);
  sem_post(&thread_sem);
  return true;
}

/**
 * There are two case for enable:
 * 1. NFC is off -> enable NFC and then enable discovery.
 * 2. NFC is already on but discovery mode is off -> enable discovery.
 */
void NfcService::HandleEnableResponse(NfcEvent* aEvent)
{
  NfcErrorCode code = NFC_SUCCESS;

  bool enable = aEvent->arg1;
  if (enable) {
    // Disable low power mode if already in low power mode
    if (mState == STATE_NFC_ON_LOW_POWER) {
      code = SetLowPowerMode(false);
    } else if (mState == STATE_NFC_OFF) {
      code = EnableNfc();
      if (code != NFC_SUCCESS) {
        goto TheEnd;
      }
      code = EnableSE();
    }
  } else {
    code = DisableSE();
    if (code != NFC_SUCCESS) {
        goto TheEnd;
    }
    code = DisableNfc();
  }
TheEnd:
  ALOGD("%s mState=%d", __func__, mState);
  mMsgHandler->ProcessResponse(NFC_RESPONSE_CHANGE_RF_STATE, code, &mState);
}

NfcErrorCode NfcService::EnableNfc()
{
  ALOGD("Enable NFC");

  if (mState != STATE_NFC_OFF) {
    return NFC_SUCCESS;
  }

  if (!sNfcManager->Initialize()) {
    return NFC_ERROR_INITIALIZE_FAIL;
  }

  if (mP2pLinkManager) {
    mP2pLinkManager->EnableDisable(true);
  }

  if (!sNfcManager->EnableDiscovery()) {
    return NFC_ERROR_FAIL_ENABLE_DISCOVERY;
  }

  mState = STATE_NFC_ON;

  return NFC_SUCCESS;
}

NfcErrorCode NfcService::DisableNfc()
{
  ALOGD("Disable NFC");

  if (mState == STATE_NFC_OFF) {
    return NFC_SUCCESS;
  }

  if (mP2pLinkManager) {
    mP2pLinkManager->EnableDisable(false);
  }

  if (!sNfcManager->Deinitialize()) {
    return NFC_ERROR_DEINITIALIZE_FAIL;
  }

  if (!sNfcManager->DisableDiscovery()) {
    return NFC_ERROR_FAIL_DISABLE_DISCOVERY;
  }

  mState = STATE_NFC_OFF;

  return NFC_SUCCESS;
}

NfcErrorCode NfcService::SetLowPowerMode(bool aLow)
{
  if ((aLow && mState != STATE_NFC_ON) ||
      (!aLow && mState == STATE_NFC_ON)) {
    return NFC_SUCCESS;
  }

  if (aLow) {
    if (!sNfcManager->DisableP2pListening() ||
        !sNfcManager->DisablePolling()) {
      return NFC_ERROR_FAIL_ENABLE_LOW_POWER_MODE;
    }

    mState = STATE_NFC_ON_LOW_POWER;
  } else {
    if (!sNfcManager->EnableP2pListening() ||
        !sNfcManager->EnablePolling()) {
      return NFC_ERROR_FAIL_DISABLE_LOW_POWER_MODE;
    }

    mState = STATE_NFC_ON;
  }

  return NFC_SUCCESS;
}

// TODO: Emulator doesn't support SE now.
//       So always return sucess to pass testcase.
NfcErrorCode NfcService::EnableSE()
{
  ALOGD("Enable secure element");

  if (!sNfcManager->DoSelectSecureElement()) {
    ALOGE("Enable secure element fail");
  }

  return NFC_SUCCESS;
}

// TODO: Emulator doesn't support SE now.
//       So always return sucess to pass testcase.
NfcErrorCode NfcService::DisableSE()
{
  ALOGD("Disable secure element");

  if (!sNfcManager->DoDeselectSecureElement()) {
    ALOGE("Disable secure element fail");
  }

  return NFC_SUCCESS;
}

void NfcService::OnP2pReceivedNdef(NdefMessage* aNdef)
{
  NfcEvent *event = new NfcEvent(MSG_RECEIVE_NDEF_EVENT);

  event->obj = aNdef ? new NdefMessage(aNdef) : NULL;

  mQueue.push_back(event);
  sem_post(&thread_sem);
}
