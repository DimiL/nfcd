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

  bool init(std::vector<uint8_t>& buf, int offset);
  bool init(std::vector<uint8_t>& buf);
  void toByteArray(std::vector<uint8_t>& buf);

  std::vector<NdefRecord> mRecords;
};

class NdefDetail{
public:
  NdefDetail();
  ~NdefDetail();

  int maxSupportedLength;
  int mode;
};

#endif
