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

class MessageHandler {
public:
  static void initialize();

  static void processRequest(const uint8_t* data, size_t length);
  static void processResponse(NfcRequest request, int token, void* data);
  static void processNotification(NfcNotification notification, void* data);

private:
  MessageHandler();

  static void notifyTechDiscovered(android::Parcel& parcel, void* data);

//  static bool handleWriteNdef(const char *input, size_t length);
//  static bool handleNdefPush(const char *input, size_t length);
//  static bool handleNdefDetailsRequest();
  static bool handleReadNdefRequest(android::Parcel& parcel, int token);
  static bool handleConnectRequest(android::Parcel& parcel, int token); 
  static bool handleReadNdefResponse(android::Parcel& parcel, void* data);
  static bool handleConnectResponse(android::Parcel& parcel);
//  static bool handleTransceiveReq(const char *input, size_t length);

  static void sendResponse(uint8_t* data, size_t length);
};

#endif // mozilla_nfcd_MessageHandler_h
