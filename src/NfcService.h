/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcService_h
#define mozilla_nfcd_NfcService_h

#include "NfcManager.h"

class MessageHandler;

class NfcService{
public:
  ~NfcService();
  void initialize(NfcManager* pNfcManager, MessageHandler* msgHandler);

  //TODO update this name
  void handleNdefTag(void* tag);
  static NfcService* Instance();
  static INfcManager* getNfcManager();

  static void nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice);
  static void nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice);
  static void nfc_service_send_MSG_NDEF_TAG(void* pTag);
  static void nfc_service_send_MSG_SE_FIELD_ACTIVATED();
  static void nfc_service_send_MSG_SE_FIELD_DEACTIVATED();
  static void nfc_service_send_MSG_SE_NOTIFY_TRANSACTION_LISTENERS();

  static bool handleDisconnect();
  static int handleConnect(int technology, int token);
  static bool handleReadNdefRequest(int token);
  static void handleReadNdefResponse(int token);
  static bool handleWriteNdefRequest(NdefMessage& ndef, int token);
  static void handleWriteNdefResponse(int token);

private:
  static NfcService* sInstance;
  static NfcManager* sNfcManager;
  static MessageHandler* sMsgHandler;
  NfcService();
};

#endif // mozilla_nfcd_NfcService_h
