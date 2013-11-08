/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NdefMessage_h
#define mozilla_nfcd_NdefMessage_h

#include "NdefRecord.h"
#include <vector>

class NdefMessage{
public:
  NdefMessage();
  ~NdefMessage();

  /**
   * Initialize NDEF meesage with NDEF binary data.
   *
   * @param  buf Input buffer contains raw NDEF data.
   * @return     True if the buffer can be correctly parsed.
   */
  bool init(std::vector<uint8_t>& buf);

  /**
   * Initialize NDEF meesage with NDEF binary data.
   *
   * @param  buf    Input buffer contains raw NDEF data.
   * @param  offset Indicate the start position of buffer to be parsed.
   * @return        True if the buffer can be correctly parsed.
   */
  bool init(std::vector<uint8_t>& buf, int offset);

  /**
   * Write current NdefMessage to byte buffer.
   *
   * @param  buf Output raw buffer.
   * @return     None.
   */
  void toByteArray(std::vector<uint8_t>& buf);

  // Array of NDEF records.
  std::vector<NdefRecord> mRecords;
};

/**
 * NdefDetail structure contains the information should be returned by readNdefDetail of INfcTag.
 */
class NdefDetail {
public:
  int maxSupportedLength;
  bool isReadOnly;
  bool canBeMadeReadOnly;
};

#endif
