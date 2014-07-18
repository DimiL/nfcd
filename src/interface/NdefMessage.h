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

#ifndef mozilla_nfcd_NdefMessage_h
#define mozilla_nfcd_NdefMessage_h

#include "NdefRecord.h"
#include <vector>

class NdefMessage{
public:
  NdefMessage();
  NdefMessage(NdefMessage* ndef);
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
