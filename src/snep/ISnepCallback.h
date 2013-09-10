/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_ISnepCallback_h
#define mozilla_nfcd_ISnepCallback_h

#include "SnepMessage.h"
#include "NdefMessage.h"

class ISnepCallback {
public:
  virtual SnepMessage* doPut(NdefMessage& msg) = 0;
  virtual SnepMessage* doGet(int acceptableLength, NdefMessage& msg) = 0;

  virtual ~ISnepCallback() {};
};

#endif
