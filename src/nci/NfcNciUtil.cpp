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

#include "NfcNciUtil.h"

TagTechnology NfcNciUtil::toGenericTagTechnology(unsigned int tagTech)
{
  switch(tagTech) {
    case TARGET_TYPE_ISO14443_3A:     return NFC_A;
    case TARGET_TYPE_ISO14443_3B:     return NFC_B;
    case TARGET_TYPE_ISO14443_4:      return NFC_ISO_DEP;
    case TARGET_TYPE_FELICA:          return NFC_F;
    case TARGET_TYPE_ISO15693:        return NFC_V;
    case TARGET_TYPE_MIFARE_CLASSIC:  return MIFARE_CLASSIC;
    case TARGET_TYPE_MIFARE_UL:       return MIFARE_ULTRALIGHT;
    case TARGET_TYPE_KOVIO_BARCODE:   return NFC_BARCODE;
    case TARGET_TYPE_UNKNOWN:
    default:                          return UNKNOWN_TECH;
  }
}

unsigned int NfcNciUtil::toNciTagTechnology(TagTechnology tagTech)
{
  switch(tagTech) {
    case NFC_A:                return TARGET_TYPE_ISO14443_3A;
    case NFC_B:                return TARGET_TYPE_ISO14443_3B;
    case NFC_ISO_DEP:          return TARGET_TYPE_ISO14443_4;
    case NFC_F:                return TARGET_TYPE_FELICA;
    case NFC_V:                return TARGET_TYPE_ISO15693;
    case MIFARE_CLASSIC:       return TARGET_TYPE_MIFARE_CLASSIC;
    case MIFARE_ULTRALIGHT:    return TARGET_TYPE_MIFARE_UL;
    case NFC_BARCODE:          return TARGET_TYPE_KOVIO_BARCODE;
    case UNKNOWN_TECH:
    default:                   return TARGET_TYPE_UNKNOWN;
  }
}
