/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_TagTechnology_h
#define mozilla_nfcd_TagTechnology_h

class TagTechnology {
public:
  static const int NFC_A = 1;
  static const int NFC_B = 2;
  static const int ISO_DEP = 3;
  static const int NFC_F = 4;
  static const int NFC_V = 5;
  static const int NDEF = 6;
  static const int NDEF_FORMATABLE = 7;
  static const int MIFARE_CLASSIC = 8;
  static const int MIFARE_ULTRALIGHT = 9;
  static const int NFC_BARCODE = 10;

private:
  TagTechnology();
};

#endif
