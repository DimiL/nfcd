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
  NDEF_FORMATABLE = 6,
  NDEF_WRITABLE = 7,
  MIFARE_CLASSIC = 8,
  MIFARE_ULTRALIGHT = 9,
  NFC_BARCODE = 10,
  UNKNOWN_TECH = 11
} TagTechnology;

#endif
