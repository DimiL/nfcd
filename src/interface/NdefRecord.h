/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NdefRecord_h
#define mozilla_nfcd_NdefRecord_h

#include <vector>

class NdefRecord {

public:
  static const uint8_t TNF_EMPTY = 0x00;
  static const uint8_t TNF_WELL_KNOWN = 0x01;
  static const uint8_t TNF_MIME_MEDIA = 0x02;
  static const uint8_t TNF_ABSOLUTE_URI = 0x03;
  static const uint8_t TNF_EXTERNAL_TYPE = 0x04;
  static const uint8_t TNF_UNKNOWN = 0x05;
  static const uint8_t TNF_UNCHANGED = 0x06;
  static const uint8_t TNF_RESERVED = 0x07;

  NdefRecord();
  NdefRecord(uint8_t tnf, std::vector<uint8_t>& type, std::vector<uint8_t>& id, std::vector<uint8_t>& payload);
//  NdefRecord(uint8_t tnf, uint32_t typeLength, uint32_t* type, uint32_t idLength, uint32_t* id, uint32_t payloadLength, uint32_t* payload);
  NdefRecord(uint8_t tnf, uint32_t typeLength, uint8_t* type, uint32_t idLength, uint8_t* id, uint32_t payloadLength, uint8_t* payload);
  ~NdefRecord();

  static bool parse(std::vector<uint8_t>& buf, bool ignoreMbMe, std::vector<NdefRecord>& records);
  static bool parse(std::vector<uint8_t>& buf, bool ignoreMbMe, std::vector<NdefRecord>& records, int offset);
  static bool ensureSanePayloadSize(long size);
  static bool validateTnf(uint8_t tnf, std::vector<uint8_t>& type, std::vector<uint8_t>& id, std::vector<uint8_t>& payload);
  
  void writeToByteBuffer(std::vector<uint8_t>& buf, bool mb, bool me);

  uint8_t mFlags;
  uint8_t mTnf;
  std::vector<uint8_t> mType;
  std::vector<uint8_t> mId;
  std::vector<uint8_t> mPayload;

private:
  static const uint8_t FLAG_MB = 0x80;
  static const uint8_t FLAG_ME = 0x40;
  static const uint8_t FLAG_CF = 0x20;
  static const uint8_t FLAG_SR = 0x10;
  static const uint8_t FLAG_IL = 0x08;

  static const int MAX_PAYLOAD_SIZE = 10 * (1 << 20);  // 10 MB payload limit
};

#endif
