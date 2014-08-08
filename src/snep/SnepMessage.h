/*
 * Copyright (C) 2013-2014  Mozilla Foundation
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

#ifndef mozilla_nfcd_SnepMessage_h
#define mozilla_nfcd_SnepMessage_h

#include <vector>

class NdefMessage;

class SnepMessage{
public:
  SnepMessage(uint8_t version,uint8_t field,int length,int acceptableLength, NdefMessage* ndefMessage);
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

  NdefMessage* getNdefMessage() { return mNdefMessage; }
  uint8_t getField() { return mField; }
  uint8_t getVersion() { return mVersion; }
  int getLength() { return mLength; }
  int getAcceptableLength() { return mField != REQUEST_GET ? 0 : mAcceptableLength; }

  void toByteArray(std::vector<uint8_t>& buf);

  static SnepMessage* getGetRequest(int acceptableLength, NdefMessage& ndef);
  static SnepMessage* getPutRequest(NdefMessage& ndef);
  static SnepMessage* getMessage(uint8_t field);
  static SnepMessage* getSuccessResponse(NdefMessage* ndef);
  static SnepMessage* fromByteArray(std::vector<uint8_t>& buf);
  static SnepMessage* fromByteArray(uint8_t* pBuf, int size);

private:
  SnepMessage();
  SnepMessage(std::vector<uint8_t>& buf);

  static bool isValidFormat(std::vector<uint8_t>& buf);

  static const int HEADER_LENGTH = 6;

  NdefMessage* mNdefMessage;
  uint8_t mVersion;
  uint8_t mField;
  uint32_t mLength;
  uint32_t mAcceptableLength;
};

#endif
