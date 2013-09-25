/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_TagTechnology_h
#define mozilla_nfcd_TagTechnology_h

typedef enum {
  NFC_A = 0,
  NFC_B = 1,
  NFC_ISO_DEP = 2,
  NFC_F = 3,
  NFC_V = 4,
  NDEF = 5,
  NDEF_FORMATABLE = 6,
  MIFARE_CLASSIC = 7,
  MIFARE_ULTRALIGHT = 8,
  NFC_BARCODE = 9,
  UNKNOWN_TECH = 10
} TagTechnology;

#endif
