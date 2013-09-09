/* This Source Code Form is subject to the terms of the Mozilla 
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_SnepMessage_h
#define mozilla_nfcd_SnepMessage_h

#include "NdefMessage.h"

class SnepMessage{
public:
  SnepMessage();
  SnepMessage(std::vector<uint8_t>& buf);
  SnepMessage(uint8_t version,uint8_t field,int length,int acceptableLength, NdefMessage ndefMessage);
  ~SnepMessage();

  static const uint8_t VERSION_MAJOR = 0x1;
  static const uint8_t VERSION_MINOR = 0x0;
  static const uint8_t VERSION = (0xF0 & (VERSION_MAJOR << 4)) | (0x0F & VERSION_MINOR);

  static const uint8_t REQUEST_CONTINUE = 0x00;
  static const uint8_t REQUEST_GET = 0x01;
  static const uint8_t REQUEST_PUT = 0x02;
  static const uint8_t REQUEST_REJECT = 0x07;

  static const uint8_t RESPONSE_CONTINUE = 0x80;
  static const uint8_t RESPONSE_SUCCESS = 0x81;
  static const uint8_t RESPONSE_NOT_FOUND = 0xC0;
  static const uint8_t RESPONSE_EXCESS_DATA = 0xC1;
  static const uint8_t RESPONSE_BAD_REQUEST = 0xC2;
  static const uint8_t RESPONSE_NOT_IMPLEMENTED = 0xE0;
  static const uint8_t RESPONSE_UNSUPPORTED_VERSION = 0xE1;
  static const uint8_t RESPONSE_REJECT = 0xFF;

  NdefMessage& getNdefMessage() {  return mNdefMessage;  }
  uint8_t getField() {  return mField;  }
  uint8_t getVersion() {  return mVersion;  }
  int getLength() {  return mLength;  }
  int getAcceptableLength() {  return mField != REQUEST_GET ? 0 : mAcceptableLength;  }

  void toByteArray(std::vector<uint8_t>& buf);

  static SnepMessage* getGetRequest(int acceptableLength, NdefMessage& ndef);
  static SnepMessage* getPutRequest(NdefMessage& ndef); 
  static SnepMessage* getMessage(uint8_t field);
  static SnepMessage* fromByteArray(std::vector<uint8_t>& buf);
private: 

  static const int HEADER_LENGTH = 6;
  
  uint8_t mVersion;
  uint8_t mField;
  int mLength;
  int mAcceptableLength;
  NdefMessage mNdefMessage;
};

#endif

