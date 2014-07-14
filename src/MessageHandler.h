/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_MessageHandler_h
#define mozilla_nfcd_MessageHandler_h

#include <stdio.h>
#include "NfcGonkMessage.h"
#include "TagTechnology.h"
#include <binder/Parcel.h>

class NfcIpcSocket;
class NfcService;
class NdefMessage;

class MessageHandler {
public:
  MessageHandler(NfcService* service): mService(service) {};
  void processRequest(const uint8_t* data, size_t length);
  void processResponse(NfcResponseType response, NfcErrorCode error, void* data);
  void processNotification(NfcNotificationType notification, void* data);

  void setOutgoingSocket(NfcIpcSocket* socket);

private:
  void notifyInitialized(android::Parcel& parcel);
  void notifyTechDiscovered(android::Parcel& parcel, void* data);
  void notifyTechLost(android::Parcel& parcel);
  void notifyTransactionEvent(android::Parcel& parcel, void* data);

  bool handleConfigRequest(android::Parcel& parcel);
  bool handleReadNdefDetailRequest(android::Parcel& parcel);
  bool handleReadNdefRequest(android::Parcel& parcel);
  bool handleWriteNdefRequest(android::Parcel& parcel);
  bool handleConnectRequest(android::Parcel& parcel);
  bool handleCloseRequest(android::Parcel& parcel);
  bool handleMakeNdefReadonlyRequest(android::Parcel& parcel);

  bool handleConfigResponse(android::Parcel& parcel, void* data);
  bool handleReadNdefDetailResponse(android::Parcel& parcel, void* data);
  bool handleReadNdefResponse(android::Parcel& parcel, void* data);
  bool handleResponse(android::Parcel& parcel);

  void sendResponse(android::Parcel& parcel);

  bool sendNdefMsg(android::Parcel& parcel, NdefMessage* ndef);

  NfcIpcSocket* mSocket;
  NfcService* mService;
};

struct TechDiscoveredEvent {
  bool isNewSession;
  uint32_t techCount;
  void* techList;
  uint32_t ndefMsgCount;
  NdefMessage* ndefMsg;
};

#endif // mozilla_nfcd_MessageHandler_h
