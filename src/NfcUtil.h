/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcUtil_h
#define mozilla_nfcd_NfcUtil_h

#include <stdio.h>
#include "NdefMessage.h"
#include "NfcGonkMessage.h"

class NfcUtil{

public:
  static void convertNdefPduToNdefMessage(NdefMessagePdu& ndefPdu, NdefMessage* ndefMessage);

private:
  NfcUtil();
};

#endif

