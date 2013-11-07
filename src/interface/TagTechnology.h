/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_TagTechnology_h
#define mozilla_nfcd_TagTechnology_h

// Used by getTechList() of INfcTag.
// Define the tag technology.
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

#endif
