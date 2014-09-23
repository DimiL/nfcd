/*
 * Copyright (C) 2014  Mozilla Foundation
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

#ifndef mozilla_nfcd_MessageHandler_h
#define mozilla_nfcd_MessageHandler_h

#include <stdio.h>
#include "NfcGonkMessage.h"
#include "TagTechnology.h"
#include <binder/Parcel.h>

class NfcIpcSocket;
class NfcService;
class NdefMessage;
class NdefInfo;

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
  void notifyTechLost(android::Parcel& parcel, void* data);
  void notifyTransactionEvent(android::Parcel& parcel, void* data);

  bool handleConfigRequest(android::Parcel& parcel);
  bool handleReadNdefRequest(android::Parcel& parcel);
  bool handleWriteNdefRequest(android::Parcel& parcel);
  bool handleConnectRequest(android::Parcel& parcel);
  bool handleCloseRequest(android::Parcel& parcel);
  bool handleMakeNdefReadonlyRequest(android::Parcel& parcel);

  bool handleConfigResponse(android::Parcel& parcel, void* data);
  bool handleReadNdefResponse(android::Parcel& parcel, void* data);
  bool handleResponse(android::Parcel& parcel);

  void sendResponse(android::Parcel& parcel);

  bool sendNdefMsg(android::Parcel& parcel, NdefMessage* ndef);
  bool sendNdefInfo(android::Parcel& parcel, NdefInfo* info);

  NfcIpcSocket* mSocket;
  NfcService* mService;
};

struct TechDiscoveredEvent {
  int sessionId;
  uint32_t techCount;
  void* techList;
  uint32_t ndefMsgCount;
  NdefMessage* ndefMsg;
  NdefInfo* ndefInfo;
};

#endif // mozilla_nfcd_MessageHandler_h
