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

  void ProcessRequest(const uint8_t* aData, size_t aLength);
  void ProcessResponse(NfcResponseType aResponse, NfcErrorCode aError, void* aData);
  void ProcessNotification(NfcNotificationType aNotification, void* aData);

  void SetOutgoingSocket(NfcIpcSocket* aSocket);

private:
  void NotifyInitialized(android::Parcel& aParcel);
  void NotifyTechDiscovered(android::Parcel& aParcel, void* aData);
  void NotifyTechLost(android::Parcel& aParcel, void* aData);
  void NotifyTransactionEvent(android::Parcel& aParcel, void* aData);

  bool HandleChangeRFStateRequest(android::Parcel& aParcel);
  bool HandleReadNdefRequest(android::Parcel& aParcel);
  bool HandleWriteNdefRequest(android::Parcel& aParcel);
  bool HandleMakeNdefReadonlyRequest(android::Parcel& aParcel);
  bool HandleNdefFormatRequest(android::Parcel& aParcel);
  bool HandleTagTransceiveRequest(android::Parcel& aParcel);

  bool HandleChangeRFStateResponse(android::Parcel& aParcel, void* aData);
  bool HandleReadNdefResponse(android::Parcel& aParcel, void* aData);
  bool HandleTagTransceiveResponse(android::Parcel& aParcel, void* aData);
  bool HandleResponse(android::Parcel& aParcel);

  void SendResponse(android::Parcel& aParcel);

  bool SendNdefMsg(android::Parcel& aParcel, NdefMessage* aNdef);
  bool SendNdefInfo(android::Parcel& aParcel, NdefInfo* aInfo);

  NfcIpcSocket* mSocket;
  NfcService* mService;
};

struct TechDiscoveredEvent {
  int sessionId;
  bool isP2P;
  uint32_t techCount;
  void* techList;
  uint32_t tagIdCount;
  uint8_t* tagId;
  uint32_t ndefMsgCount;
  NdefMessage* ndefMsg;
  NdefInfo* ndefInfo;
};

#endif // mozilla_nfcd_MessageHandler_h
