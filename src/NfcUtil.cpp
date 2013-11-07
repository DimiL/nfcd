#include "NfcUtil.h"

void NfcUtil::convertNdefPduToNdefMessage(NdefMessagePdu& ndefPdu, NdefMessage* ndefMessage) {
  for (uint32_t i = 0; i < ndefPdu.numRecords; i++) {
    NdefRecordPdu& record = ndefPdu.records[i];
    ndefMessage->mRecords.push_back(NdefRecord(
      record.tnf,
      record.typeLength, record.type,
      record.idLength, record.id,
      record.payloadLength, record.payload));
  }
}

NfcTechnology NfcUtil::convertTagTechToGonkFormat(TagTechnology tagTech) {
  switch(tagTech) {
    case NFC_A:              return  NFC_TECH_NFCA;
    case NFC_B:              return  NFC_TECH_NFCB;
    case NFC_ISO_DEP:        return  NFC_TECH_ISO_DEP;
    case NFC_F:              return  NFC_TECH_NFCF;
    case NFC_V:              return  NFC_TECH_NFCV;
    case NDEF:               return  NFC_TECH_NDEF;
    case NDEF_FORMATABLE:    return  NFC_TECH_NDEF_FORMATABLE;
    case NDEF_WRITABLE:      return  NFC_TECH_NDEF_WRITABLE;
    case MIFARE_CLASSIC:     return  NFC_TECH_MIFARE_CLASSIC;
    case MIFARE_ULTRALIGHT:  return  NFC_TECH_MIFARE_ULTRALIGHT;
    case NFC_BARCODE:        return  NFC_TECH_MIFARE_ULTRALIGHT;
    case UNKNOWN_TECH:
    default:                 return  NFC_TECH_UNKNOWN;
  }
  return NFC_TECH_NFCA;
}
