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
#include "NfcDebug.h"

TagTechnology NfcNciUtil::ToTagTechnology(TechnologyType aTechType)
{
  // Now we don't expose NDEF as a technology to gecko.
  // It should be only used inside NCI stack.
  if (aTechType == TECHNOLOGY_TYPE_NDEF) {
    NCI_ERROR("NDEF technology should only be used internally in NCI");
  }

  switch(aTechType) {
    case TECHNOLOGY_TYPE_ISO14443_3A:     return NFC_A;
    case TECHNOLOGY_TYPE_ISO14443_3B:     return NFC_B;
    case TECHNOLOGY_TYPE_ISO14443_4:      return NFC_ISO_DEP;
    case TECHNOLOGY_TYPE_FELICA:          return NFC_F;
    case TECHNOLOGY_TYPE_ISO15693:        return NFC_V;
    case TECHNOLOGY_TYPE_MIFARE_CLASSIC:  return MIFARE_CLASSIC;
    case TECHNOLOGY_TYPE_MIFARE_UL:       return MIFARE_ULTRALIGHT;
    case TECHNOLOGY_TYPE_KOVIO_BARCODE:   return NFC_BARCODE;
    case TECHNOLOGY_TYPE_UNKNOWN:
    default:                              return UNKNOWN;
  }
}

TechnologyType NfcNciUtil::ToTechnologyType(TagTechnology aTagTech)
{
  switch(aTagTech) {
    case NFC_A:                return TECHNOLOGY_TYPE_ISO14443_3A;
    case NFC_B:                return TECHNOLOGY_TYPE_ISO14443_3B;
    case NFC_ISO_DEP:          return TECHNOLOGY_TYPE_ISO14443_4;
    case NFC_F:                return TECHNOLOGY_TYPE_FELICA;
    case NFC_V:                return TECHNOLOGY_TYPE_ISO15693;
    case MIFARE_CLASSIC:       return TECHNOLOGY_TYPE_MIFARE_CLASSIC;
    case MIFARE_ULTRALIGHT:    return TECHNOLOGY_TYPE_MIFARE_UL;
    case NFC_BARCODE:          return TECHNOLOGY_TYPE_KOVIO_BARCODE;
    case UNKNOWN:
    default:                   return TECHNOLOGY_TYPE_UNKNOWN;
  }
}
