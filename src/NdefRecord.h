/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NdefRecord_h__
#define mozilla_NdefRecord_h__

#include <vector>

class NdefRecord {

public:

  NdefRecord();
  ~NdefRecord();

  static void parse(std::vector<uint8_t>& buf, bool ignoreMbMe);

  uint8_t mFlags;
  uint16_t mTnf;
  std::vector<uint8_t> mType;
  std::vector<uint8_t> mId;
  std::vector<uint8_t> mPayload;
};

#endif
