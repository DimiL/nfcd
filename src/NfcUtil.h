/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcUtil_h
#define mozilla_nfcd_NfcUtil_h

#include <stdio.h>

class NfcUtil{

public:
  static char* encodeBase64(const char *input, size_t length);
  static char* decodeBase64(unsigned char *input, size_t length, size_t *out_length);
  static char* getTechString(int techIndex);

private:
  NfcUtil();
};

#endif

