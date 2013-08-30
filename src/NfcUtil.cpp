#include "NfcUtil.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

void NfcUtil::convertNdefPduToNdefMessage(NdefMessagePdu& ndefPdu, NdefMessage& ndefMessage) {
  for (uint32_t i = 0; i < ndefPdu.numRecords; i++) {
    NdefRecordPdu& record = ndefPdu.records[i];
    ndefMessage.mRecords.push_back(NdefRecord(
      record.tnf,
      record.typeLength, record.type,
      record.idLength, record.id,
      record.payloadLength, record.payload));
  }
}
