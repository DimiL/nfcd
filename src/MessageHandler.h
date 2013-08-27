/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_MessageHandler_h
#define mozilla_nfcd_MessageHandler_h

#include <stdio.h>
#include "NfcGonkMessage.h"
#include <binder/Parcel.h>

//enum {
//  NOTIFY_NDEF_DISCOVERED = 1,
//  NOTIFY_TECH_DISCOVERED,
//  NOTIFY_READ_REQ,
//  NOTIFY_WRITE_REQ,
//  NOTIFY_TRANSCEIVE_REQ,
//  NOTIFY_TRANSCEIVE_RSP,
//  NOTIFY_NDEF_DETAILS_REQUEST,
//  NOTIFY_READ_NDEF,
//  NOTIFY_NDEF_WRITE_REQUEST,
//  NOTIFY_NDEF_DISCONNECTED,
//  NOTIFY_REQUEST_STATUS,
//  NOTIFY_NDEF_PUSH_REQUEST,
//  NOTIFY_SECURE_ELEMENT_ACTIVATE,
//  NOTIFY_SECURE_ELEMENT_DEACTIVATE,
//  NOTIFY_SECURE_ELEMENT_TRANSACTION
//};

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

//  static bool retrieveMessageType(const char *input, size_t length, unsigned int *outTypeId);

//  static void messageNotifyNdefDiscovered(NdefMessage* ndefMsg);
//  static void messageNotifyNdefDetails(int maxNdefMsgLength, int state);
//  static void messageNotifyNdefDisconnected();
//  static void messageNotifyNdefDisconnected(const char *message);
  static void notifyTechDiscovered(android::Parcel& parcel, void* data);
//  static void messageNotifyRequestStatus(const char *requestId, int status, char *message);
//  static void messageNotifySecureElementFieldActivated();
//  static void messageNotifySecureElementFieldDeactivated();

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
