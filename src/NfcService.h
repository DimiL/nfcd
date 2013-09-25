/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcService_h
#define mozilla_nfcd_NfcService_h

#include "utils/List.h"
#include "NfcManager.h"

class MessageHandler;

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
  int token;
  void *data;
};

class NfcService {
public:
  ~NfcService();
  void initialize(NfcManager* pNfcManager, MessageHandler* msgHandler);

  void handleTagDiscovered(NfcEvent* event);
  void handleTagLost(NfcEvent* event);
  void handleLlcpLinkActivation(NfcEvent* event);
  void handleLlcpLinkDeactivation(NfcEvent* event);
  static NfcService* Instance();
  static INfcManager* getNfcManager();

  static void nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice);
  static void nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice);
  static void nfc_service_send_MSG_TAG(void* pTag);
  static void nfc_service_send_MSG_SE_FIELD_ACTIVATED();
  static void nfc_service_send_MSG_SE_FIELD_DEACTIVATED();
  static void nfc_service_send_MSG_SE_NOTIFY_TRANSACTION_LISTENERS();

  static bool handleDisconnect();
  int handleConnect(int technology, int token);
  bool handleConfigRequest(int token);
  void handleConfigResponse(NfcEvent* event);
  bool handleReadNdefDetailRequest(int token);
  void handleReadNdefDetailResponse(NfcEvent* event);
  bool handleReadNdefRequest(int token);
  void handleReadNdefResponse(NfcEvent* event);
  bool handleWriteNdefRequest(NdefMessage* ndef, int token);
  void handleWriteNdefResponse(NfcEvent* event);
  void handleCloseRequest();
  void handleCloseResponse(NfcEvent* event);
  bool handlePushNdefRequest(NdefMessage* ndef, int token);
  void handlePushNdefResponse(NfcEvent* event);
  bool handleMakeNdefReadonlyRequest(int token);
  void handleMakeNdefReadonlyResponse(NfcEvent* event);

  static void onSocketConnected();

  //TODO remove static
  //TODO put these in public because serviceThread will need to access this.
  static android::List<NfcEvent*> mQueue;
  static MessageHandler* sMsgHandler;
private:
  static NfcService* sInstance;
  static NfcManager* sNfcManager;
  NfcService();
};

#endif // mozilla_nfcd_NfcService_h
