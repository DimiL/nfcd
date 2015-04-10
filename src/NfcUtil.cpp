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

#include "NfcUtil.h"

void NfcUtil::ConvertNdefPduToNdefMessage(NdefMessagePdu& aNdefPdu,
                                          NdefMessage* aNdefMessage) {
  for (uint32_t i = 0; i < aNdefPdu.numRecords; i++) {
    NdefRecordPdu& record = aNdefPdu.records[i];
    aNdefMessage->mRecords.push_back(NdefRecord(
      record.tnf,
      record.typeLength, record.type,
      record.idLength, record.id,
      record.payloadLength, record.payload));
  }
}

NfcEvtTransactionOrigin NfcUtil::ConvertOriginType(TransactionEvent::OriginType aType)
{
  switch (aType) {
    case TransactionEvent::SIM:    return NFC_EVT_TRANSACTION_SIM;
    case TransactionEvent::ESE:    return NFC_EVT_TRANSACTION_ESE;
    case TransactionEvent::ASSD:   return NFC_EVT_TRANSACTION_ASSD;
    default:                       return NFC_EVT_TRANSACTION_SIM;
  }
}

NfcNdefType NfcUtil::ConvertNdefType(NdefType aType)
{
  switch (aType) {
    case NDEF_TYPE1_TAG:           return NFC_NDEF_TYPE_1_TAG;
    case NDEF_TYPE2_TAG:           return NFC_NDEF_TYPE_2_TAG;
    case NDEF_TYPE3_TAG:           return NFC_NDEF_TYPE_3_TAG;
    case NDEF_TYPE4_TAG:           return NFC_NDEF_TYPE_4_TAG;
    case NDEF_MIFARE_CLASSIC_TAG:  return NFC_NDEF_MIFARE_CLASSIC_TAG;
    default:                       return NFC_NDEF_UNKNOWN_TAG;
  }
}
