/*
 * Copyright (C) 2013-2014  Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
  MIFARE_CLASSIC = 6,
  MIFARE_ULTRALIGHT = 7,
  NFC_BARCODE = 8,
  UNKNOWN_TECH = 9
} TagTechnology;


// Pre-defined tag type values.
typedef enum {
  NDEF_UNKNOWN_TYPE = -1,
  NDEF_TYPE1_TAG = 1,
  NDEF_TYPE2_TAG = 2,
  NDEF_TYPE3_TAG = 3,
  NDEF_TYPE4_TAG = 4,
  NDEF_MIFARE_CLASSIC_TAG = 101
} NdefType;

#endif
