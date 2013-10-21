/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcService_h
#define mozilla_nfcd_NfcService_h

#include "utils/List.h"
#include "IpcSocketListener.h"
#include "NfcManager.h"

class NdefMessage;
class MessageHandler;
class NfcEvent;
class INfcManager;
class SnepServer;
class HandoverServer;

class NfcService : public IpcSocketListener {
public:
  ~NfcService();
  void initialize(NfcManager* pNfcManager, MessageHandler* msgHandler);

  static NfcService* Instance();
  static INfcManager* getNfcManager();

  static void notifyLlcpLinkActivation(void* pDevice);
  static void notifyLlcpLinkDeactivation(void* pDevice);
  static void notifyTagDiscovered(void* pTag);
  static void notifyTagLost();
  static void notifySEFieldActivated();
  static void notifySEFieldDeactivated();
  static void notifySETransactionListeners();

  static bool handleDisconnect();

  void* eventLoop();

  void handleTagDiscovered(NfcEvent* event);
  void handleTagLost(NfcEvent* event);
  void handleLlcpLinkActivation(NfcEvent* event);
  void handleLlcpLinkDeactivation(NfcEvent* event);
  int handleConnect(int technology);
  bool handleConfigRequest();
  void handleConfigResponse(NfcEvent* event);
  bool handleReadNdefDetailRequest();
  void handleReadNdefDetailResponse(NfcEvent* event);
  bool handleReadNdefRequest();
  void handleReadNdefResponse(NfcEvent* event);
  bool handleWriteNdefRequest(NdefMessage* ndef);
  void handleWriteNdefResponse(NfcEvent* event);
  void handleCloseRequest();
  void handleCloseResponse(NfcEvent* event);
  bool handlePushNdefRequest(NdefMessage* ndef);
  void handlePushNdefResponse(NfcEvent* event);
  bool handleMakeNdefReadonlyRequest();
  void handleMakeNdefReadonlyResponse(NfcEvent* event);
  bool handlePowerOnOffRequest(bool onOff);
  void handlePowerOnOffResponse(NfcEvent* event);
  bool handleNfcEnableDisableRequest(bool enableDisable);
  void handleNfcEnableDisableResponse(NfcEvent* event);

  void onConnected();

  void enableNfc();
  void disableNfc();

private:
  NfcService();

  static NfcService* sInstance;
  static NfcManager* sNfcManager;
  android::List<NfcEvent*> mQueue;
  MessageHandler* mMsgHandler;

  SnepServer* mSnepServer;
  HandoverServer* mHandoverServer;
};

#endif // mozilla_nfcd_NfcService_h
