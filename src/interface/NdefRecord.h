/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  NdefRecord(uint8_t tnf, std::vector<uint8_t>& type, std::vector<uint8_t>& id, std::vector<uint8_t>& payload);

  /**
   * Constructor with type, id, payload as input parameter.
   */
  NdefRecord(uint8_t tnf, uint32_t typeLength, uint8_t* type, uint32_t idLength,
             uint8_t* id, uint32_t payloadLength, uint8_t* payload);

  /**
   * Destructor.
   */
  ~NdefRecord();

  /**
   * Utility function to fill NdefRecord.
   *
   * @param  buf        Input buffer contain raw NDEF data.
   * @param  ignoreMbMe Set if only want to parse single NdefRecord and do not care about Mb,Me field.
   * @param  records    Output formatted NdefRecord parsed from buf.
   * @return            True if the buffer can be correctly parsed.
   */
  static bool parse(std::vector<uint8_t>& buf, bool ignoreMbMe, std::vector<NdefRecord>& records);

  /**
   * Utility function to fill NdefRecord.
   *
   * @param  buf        Input buffer contain raw NDEF data.
   * @param  ignoreMbMe Set if only want to parse single NdefRecord and do not care about Mb,Me field.
   * @param  records    Output formatted NdefRecord parsed from buf.
   * @param  offset     Indicate the start position of buffer to be parsed.
   * @return            True if the buffer can be correctly parsed.
   */
  static bool parse(std::vector<uint8_t>& buf, bool ignoreMbMe, std::vector<NdefRecord>& records, int offset);

  /**
   * Write current Ndefrecord to byte buffer. MB,ME bit is specified in parameter.
   *
   * @param  buf Output raw buffer.
   * @param  mb  Message begine bit of NDEF record.
   * @param  me  Message end bit of NDEF record.
   * @return     None.
   */
  void writeToByteBuffer(std::vector<uint8_t>& buf, bool mb, bool me);

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
