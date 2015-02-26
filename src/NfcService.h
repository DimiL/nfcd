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

#ifndef mozilla_nfcd_NfcService_h
#define mozilla_nfcd_NfcService_h

#include "utils/List.h"
#include "IpcSocketListener.h"
#include "NfcManager.h"
#include "NfcGonkMessage.h"

class NdefMessage;
class MessageHandler;
class NfcEvent;
class INfcManager;
class INfcTag;
class IP2pDevice;
class P2pLinkManager;

class NfcService : public IpcSocketListener {
public:
  ~NfcService();
  void Initialize(NfcManager* aNfcManager, MessageHandler* aMsgHandler);

  static NfcService* Instance();
  static INfcManager* GetNfcManager();

  static void NotifyLlcpLinkActivated(IP2pDevice* aDevice);
  static void NotifyLlcpLinkDeactivated(IP2pDevice* aDevice);
  static void NotifyTagDiscovered(INfcTag* aTag);
  static void NotifyTagLost(int aSessionId);
  static void NotifySEFieldActivated();
  static void NotifySEFieldDeactivated();
  static void NotifySETransactionEvent(TransactionEvent* aEvent);

  static bool HandleDisconnect();

  void* EventLoop();

  void HandleTagDiscovered(NfcEvent* aEvent);
  void HandleTagLost(NfcEvent* aEvent);
  void HandleTransactionEvent(NfcEvent* aEvent);
  void HandleLlcpLinkActivation(NfcEvent* aEvent);
  void HandleLlcpLinkDeactivation(NfcEvent* aEvent);
  bool HandleReadNdefRequest();
  void HandleReadNdefResponse(NfcEvent* aEvent);
  bool HandleWriteNdefRequest(NdefMessage* aNdef, bool aIsP2P);
  void HandleWriteNdefResponse(NfcEvent* aEvent);
  void HandleCloseResponse(NfcEvent* aEvent);
  bool HandlePushNdefRequest(NdefMessage* aNdef);
  void HandlePushNdefResponse(NfcEvent* aEvent);
  bool HandleMakeNdefReadonlyRequest();
  void HandleMakeNdefReadonlyResponse(NfcEvent* aEvent);
  bool HandleNdefFormatRequest();
  void HandleNdefFormatResponse(NfcEvent* aEvent);
  bool HandleTagTransceiveRequest(int aTech, const uint8_t* aBuf, uint32_t aBufLen);
  void HandleTagTransceiveResponse(NfcEvent* aEvent);
  bool HandleEnterLowPowerRequest(bool aEnter);
  void HandleEnterLowPowerResponse(NfcEvent* aEvent);
  bool HandleEnableRequest(bool aEnable);
  void HandleEnableResponse(NfcEvent* aEvent);
  void HandleReceiveNdefEvent(NfcEvent* aEvent);

  void OnConnected();
  void OnP2pReceivedNdef(NdefMessage* aNdef);
  NfcErrorCode EnableNfc();
  NfcErrorCode DisableNfc();

  void TagDetected()    { mIsTagPresent = true; }
  void TagRemoved()     { mIsTagPresent = false; }
  bool IsTagPresent()  { return mIsTagPresent; }

private:
  NfcService();

  NfcErrorCode SetLowPowerMode(bool aLow);
  NfcErrorCode EnableSE();
  NfcErrorCode DisableSE();

  uint32_t mState;
  bool mIsTagPresent;
  static NfcService* sInstance;
  static NfcManager* sNfcManager;
  android::List<NfcEvent*> mQueue;
  MessageHandler* mMsgHandler;
  P2pLinkManager* mP2pLinkManager;
};

#endif // mozilla_nfcd_NfcService_h
