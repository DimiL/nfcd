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

#ifndef mozilla_nfcd_NdefRecord_h
#define mozilla_nfcd_NdefRecord_h

#include <vector>

class NdefRecord {

public:
  // Type Name Format, defined in NFC forum "NFC Data Exchange Format (NDEF)" spec
  static const uint8_t TNF_EMPTY = 0x00;
  static const uint8_t TNF_WELL_KNOWN = 0x01;
  static const uint8_t TNF_MIME_MEDIA = 0x02;
  static const uint8_t TNF_ABSOLUTE_URI = 0x03;
  static const uint8_t TNF_EXTERNAL_TYPE = 0x04;
  static const uint8_t TNF_UNKNOWN = 0x05;
  static const uint8_t TNF_UNCHANGED = 0x06;
  static const uint8_t TNF_RESERVED = 0x07;

  /**
   * Default constructor.
   */
  NdefRecord();

  /**
   * Constructor with type, id, payload as input parameter.
   */
  NdefRecord(uint8_t aTnf,
             std::vector<uint8_t>& aType,
             std::vector<uint8_t>& aId,
             std::vector<uint8_t>& aPayload);

  /**
   * Constructor with type, id, payload as input parameter.
   */
  NdefRecord(uint8_t aTnf,
             uint32_t aTypeLength,
             uint8_t* aType,
             uint32_t aIdLength,
             uint8_t* aId,
             uint32_t aPayloadLength,
             uint8_t* aPayload);

  /**
   * Destructor.
   */
  ~NdefRecord();

  /**
   * Utility function to fill NdefRecord.
   *
   * @param  aBuf        Input buffer contains raw NDEF data.
   * @param  aIgnoreMbMe Set if only want to parse single NdefRecord and do not care about Mb,Me field.
   * @param  aRecords    Output formatted NdefRecord parsed from buf.
   * @return             True if the buffer can be correctly parsed.
   */
  static bool Parse(std::vector<uint8_t>& aBuf,
                    bool aIgnoreMbMe,
                    std::vector<NdefRecord>& aRecords);

  /**
   * Utility function to fill NdefRecord.
   *
   * @param  aBuf        Input buffer contains raw NDEF data.
   * @param  aIgnoreMbMe Set if only want to parse single NdefRecord and do not care about Mb,Me field.
   * @param  aRecords    Output formatted NdefRecord parsed from buf.
   * @param  aOffset     Indicate the start position of buffer to be parsed.
   * @return             True if the buffer can be correctly parsed.
   */
  static bool Parse(std::vector<uint8_t>& aBuf,
                    bool aIgnoreMbMe,
                    std::vector<NdefRecord>& aRecords,
                    int aOffset);

  /**
   * Write current Ndefrecord to byte buffer. MB,ME bit is specified in parameter.
   *
   * @param  aBuf Output raw buffer.
   * @param  aMb  Message begine bit of NDEF record.
   * @param  aMe  Message end bit of NDEF record.
   * @return      None.
   */
  void WriteToByteBuffer(std::vector<uint8_t>& aBuf,
                         bool aMb,
                         bool aMe);

  // MB, ME, CF, SR, IL.
  uint8_t mFlags;

  // Type name format.
  uint8_t mTnf;

  // Payload type.
  std::vector<uint8_t> mType;

  // Identifier.
  std::vector<uint8_t> mId;

  // NDEF payload.
  std::vector<uint8_t> mPayload;
};

#endif
