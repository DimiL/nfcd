/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NdefMessage_h__
#define mozilla_NdefMessage_h__

#include "NdefRecord.h"
#include <vector>

class NdefMessage{
public:
  NdefMessage();
  NdefMessage(std::vector<uint8_t>& buf);

  ~NdefMessage();

  std::vector<NdefRecord> mRecords;
};

#endif
