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
class P2pLinkManager;

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
  bool handleConfigRequest(int powerLevel);
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
  bool handleEnableDiscoveryRequest(bool enter);
  void handleEnableDiscoveryResponse(NfcEvent* event);
  bool handleEnableRequest(bool enable);
  void handleEnableResponse(NfcEvent* event);

  void onConnected();
  void onP2pReceivedNdef(NdefMessage* ndef);
  void enableNfc();
  void disableNfc();
  bool enableNfcDiscovery(bool enable);

private:
  NfcService();

  bool mIsEnable;
  static NfcService* sInstance;
  static NfcManager* sNfcManager;
  android::List<NfcEvent*> mQueue;
  MessageHandler* mMsgHandler;
  P2pLinkManager* mP2pLinkManager;
};

#endif // mozilla_nfcd_NfcService_h
