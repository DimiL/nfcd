/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_MessageHandler_h
#define mozilla_nfcd_MessageHandler_h

#include <stdio.h>
#include "NfcGonkMessage.h"
#include <binder/Parcel.h>

class NativeNfcTag;
class NdefMessage;
class NfcIpcSocket;

class MessageHandler {
public:
  MessageHandler() {};
  void processRequest(const uint8_t* data, size_t length);
  void processResponse(NfcResponseType response, int token, NfcErrorCode error, void* data);
  void processNotification(NfcNotificationType notification, void* data);
  //TODO a better naming?
  void setSocket(NfcIpcSocket* socket);

  void onSocketConnected();
private:
  void notifyInitialized(android::Parcel& parcel);
  void notifyTechDiscovered(android::Parcel& parcel, void* data);

  bool handleReadNdefDetailRequest(android::Parcel& parcel, int token);
  bool handleReadNdefRequest(android::Parcel& parcel, int token);
  bool handleWriteNdefRequest(android::Parcel& parcel, int token);
  bool handleConnectRequest(android::Parcel& parcel, int token); 
  bool handleCloseRequest(android::Parcel& parcel, int token);

  bool handleReadNdefDetailResponse(android::Parcel& parcel, void* data);
  bool handleReadNdefResponse(android::Parcel& parcel, void* data);
  bool handleResponse(android::Parcel& parcel);

  void sendResponse(android::Parcel& parcel);

  NfcIpcSocket* mSocket;
};

#endif // mozilla_nfcd_MessageHandler_h
