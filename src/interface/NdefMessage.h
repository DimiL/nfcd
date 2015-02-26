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
#include "TagTechnology.h"
#include <vector>

class NdefMessage{
public:
  NdefMessage();
  NdefMessage(NdefMessage* ndef);
  ~NdefMessage();

  /**
   * Initialize NDEF meesage with NDEF binary data.
   *
   * @param  aBuf Input buffer contains raw NDEF data.
   * @return      True if the buffer can be correctly parsed.
   */
  bool Init(std::vector<uint8_t>& aBuf);

  /**
   * Initialize NDEF meesage with NDEF binary data.
   *
   * @param  aBuf    Input buffer contains raw NDEF data.
   * @param  aOffset Indicate the start position of buffer to be parsed.
   * @return         True if the buffer can be correctly parsed.
   */
  bool Init(std::vector<uint8_t>& aBuf, int aOffset);

  /**
   * Write current NdefMessage to byte buffer.
   *
   * @param  aBuf Output raw buffer.
   * @return      None.
   */
  void ToByteArray(std::vector<uint8_t>& aBuf);

  // Array of NDEF records.
  std::vector<NdefRecord> mRecords;
};

/**
 * NdefInfo structure contains the information returned by readNdefInfo of INfcTag.
 */
class NdefInfo {
public:
  NdefType ndefType;
  int maxSupportedLength;
  bool isReadOnly;
  bool isFormatable;
};

#endif
