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
  void initialize(NfcManager* pNfcManager, MessageHandler* msgHandler);

  static NfcService* Instance();
  static INfcManager* getNfcManager();

  static void notifyLlcpLinkActivated(IP2pDevice* pDevice);
  static void notifyLlcpLinkDeactivated(IP2pDevice* pDevice);
  static void notifyTagDiscovered(INfcTag* pTag);
  static void notifyTagLost(int sessionId);
  static void notifySEFieldActivated();
  static void notifySEFieldDeactivated();
  static void notifySETransactionEvent(TransactionEvent* pEvent);

  static bool handleDisconnect();

  void* eventLoop();

  void handleTagDiscovered(NfcEvent* event);
  void handleTagLost(NfcEvent* event);
  void handleTransactionEvent(NfcEvent* event);
  void handleLlcpLinkActivation(NfcEvent* event);
  void handleLlcpLinkDeactivation(NfcEvent* event);
  void handleConnect(int technology);
  bool handleReadNdefRequest();
  void handleReadNdefResponse(NfcEvent* event);
  bool handleWriteNdefRequest(NdefMessage* ndef, bool isP2P);
  void handleWriteNdefResponse(NfcEvent* event);
  void handleCloseRequest();
  void handleCloseResponse(NfcEvent* event);
  bool handlePushNdefRequest(NdefMessage* ndef);
  void handlePushNdefResponse(NfcEvent* event);
  bool handleMakeNdefReadonlyRequest();
  void handleMakeNdefReadonlyResponse(NfcEvent* event);
  bool handleNdefFormatRequest();
  void handleNdefFormatResponse(NfcEvent* event);
  bool handleEnterLowPowerRequest(bool enter);
  void handleEnterLowPowerResponse(NfcEvent* event);
  bool handleEnableRequest(bool enable);
  void handleEnableResponse(NfcEvent* event);
  void handleReceiveNdefEvent(NfcEvent* event);

  void onConnected();
  void onP2pReceivedNdef(NdefMessage* ndef);
  NfcErrorCode enableNfc();
  NfcErrorCode disableNfc();

  void tagDetected()    { mIsTagPresent = true; }
  void tagRemoved()     { mIsTagPresent = false; }
  bool isTagPresent()  { return mIsTagPresent; }

private:
  NfcService();

  NfcErrorCode setLowPowerMode(bool low);
  NfcErrorCode enableSE();
  NfcErrorCode disableSE();

  uint32_t mState;
  bool mIsTagPresent;
  static NfcService* sInstance;
  static NfcManager* sNfcManager;
  android::List<NfcEvent*> mQueue;
  MessageHandler* mMsgHandler;
  P2pLinkManager* mP2pLinkManager;
};

#endif // mozilla_nfcd_NfcService_h
