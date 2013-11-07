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
  NDEF_WRITABLE = 7,
  MIFARE_CLASSIC = 8,
  MIFARE_ULTRALIGHT = 9,
  NFC_BARCODE = 10,
  UNKNOWN_TECH = 11
} TagTechnology;

#define NDEF_UNKNOWN_TYPE          -1
#define NDEF_TYPE1_TAG             1
#define NDEF_TYPE2_TAG             2
#define NDEF_TYPE3_TAG             3
#define NDEF_TYPE4_TAG             4
#define NDEF_MIFARE_CLASSIC_TAG    101

#endif
