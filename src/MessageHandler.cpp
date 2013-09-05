/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <map>
#include <string>

#include "MessageHandler.h"
#include "NfcService.h"
#include "NfcIpcSocket.h"
#include "NfcUtil.h"
#include "NdefMessage.h"
#include "NdefRecord.h"
#include "SessionId.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include <utils/Log.h>

using android::Parcel;

void MessageHandler::initialize()
{
}

void MessageHandler::notifyTechDiscovered(Parcel& parcel, void* data)
{

  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(data);
  std::vector<int>& techList = pINfcTag->getTechList();

  // TODO: SessionId
  //parcel.writeInt32(SessionId::getSessionId());
  int numberOfTech = techList.size();
  parcel.writeInt32(numberOfTech);
  for (int i = 0; i < numberOfTech; i++) {
    parcel.writeInt32(techList[i]);
  }
  sendResponse(parcel);
}

// static
void MessageHandler::processRequest(const uint8_t* data, size_t dataLen)
{
  Parcel parcel;
  int32_t sizeLe, size, request, token;
  uint32_t status;

  ALOGD("%s enter data=%p, dataLen=%d", __func__, data, dataLen);
  parcel.setData((uint8_t*)data, dataLen);
  status = parcel.readInt32(&request);
  status = parcel.readInt32(&token);

  if (status != 0) {
    ALOGE("Invalid request block");
    return;
  }

  switch (request) {
    case NFC_REQUEST_READ_NDEF:
      handleReadNdefRequest(parcel, token);
      break;
    case NFC_REQUEST_WRITE_NDEF:
      handleWriteNdefRequest(parcel, token);
      break;
    case NFC_REQUEST_CONNECT:
      handleConnectRequest(parcel, token);
      break;
    case NFC_REQUEST_CLOSE:
      handleCloseRequest(parcel, token);
      break;
    default:
      ALOGE("Unhandled Request %d", request);
      break;
  }
}

// static
void MessageHandler::processResponse(NfcRequest request, int token, void* data)
{
  ALOGD("%s enter request=%d, token=%d ", __func__, request, token);
  Parcel parcel;
  parcel.writeInt32(NFCC_MESSAGE_RESPONSE);
  parcel.writeInt32(token);
  parcel.writeInt32(0); //error code

  switch (request) {
    case NFC_REQUEST_READ_NDEF:
      handleReadNdefResponse(parcel, data);
      break;
    case NFC_REQUEST_WRITE_NDEF:
      handleWriteNdefResponse(parcel);
      break;
    case NFC_REQUEST_CONNECT:
      handleConnectResponse(parcel);
      break;
  }
}

// static
void MessageHandler::processNotification(NfcNotification notification, void* data)
{
  Parcel parcel;
  parcel.writeInt32(NFCC_MESSAGE_NOTIFICATION);
  parcel.writeInt32(notification);

  switch (notification) {
    case NFC_NOTIFICATION_TECH_DISCOVERED:
      notifyTechDiscovered(parcel, data);
      break;
  }
}

// static
void MessageHandler::sendResponse(Parcel& parcel)
{
  NfcIpcSocket::writeToOutgoingQueue(const_cast<uint8_t*>(parcel.data()), parcel.dataSize());
}

#if 0
bool MessageHandler::handleNdefPush(const char *input, size_t length)
{
  return true;
}

bool MessageHandler::handleNdefDetailsRequest()
{
  return true;
}
#endif

bool MessageHandler::handleReadNdefRequest(Parcel& parcel, int token)
{
  //TODO read SessionId
  return NfcService::handleReadNdefRequest(token);
}

bool MessageHandler::handleWriteNdefRequest(Parcel& parcel, int token)
{
  //TODO read SessionId
  NdefMessagePdu ndefMessagePdu;
  NdefMessage ndefMessage;

  uint32_t numRecords = parcel.readInt32();
  ndefMessagePdu.numRecords = numRecords;
  ndefMessagePdu.records = new NdefRecordPdu[numRecords];

  for (uint32_t i = 0; i < numRecords; i++) {
    ndefMessagePdu.records[i].tnf = parcel.readInt32();

    uint32_t typeLength = parcel.readInt32();
    ndefMessagePdu.records[i].typeLength = typeLength;
    ndefMessagePdu.records[i].type = new uint32_t[typeLength];
    for (uint32_t j = 0; j < typeLength; j++) {
      ndefMessagePdu.records[i].type[j] = parcel.readInt32();
    }

    uint32_t idLength = parcel.readInt32();
    ndefMessagePdu.records[i].idLength = idLength;
    ndefMessagePdu.records[i].id = new uint32_t[idLength];
    for (uint32_t j = 0; j < idLength; j++) {
      ndefMessagePdu.records[i].id[j] = parcel.readInt32();
    }

    uint32_t payloadLength = parcel.readInt32();
    ndefMessagePdu.records[i].payloadLength = payloadLength;
    ndefMessagePdu.records[i].payload = new uint32_t[payloadLength];
    for (uint32_t j = 0; j < payloadLength; j++) {
      ndefMessagePdu.records[i].payload[j] = parcel.readInt32();
    }
  }

  NfcUtil::convertNdefPduToNdefMessage(ndefMessagePdu, ndefMessage);

  for (uint32_t i = 0; i < numRecords; i++) {
    delete[] ndefMessagePdu.records[i].type;
    delete[] ndefMessagePdu.records[i].id;
    delete[] ndefMessagePdu.records[i].payload;
  }
  delete[] ndefMessagePdu.records;

  return NfcService::handleWriteNdefRequest(ndefMessage, token);
}

bool MessageHandler::handleConnectRequest(Parcel& parcel, int token)
{
  int32_t techType = parcel.readInt32();
  ALOGD("%s techType=%d", __func__, techType);
  NfcService::handleConnect(techType, token);
  return false;
}

bool MessageHandler::handleCloseRequest(Parcel& parcel, int token)
{
  return false;
}

bool MessageHandler::handleReadNdefResponse(Parcel& parcel, void* data)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(data);
  //TODO write SessionId
  int numRecords = ndef->mRecords.size();
  parcel.writeInt32(numRecords);

  for (int i = 0; i < numRecords; i++) {
    NdefRecord &record = ndef->mRecords[i];

    ALOGV("tnf=%u",record.mTnf);
    parcel.writeInt32(record.mTnf);

    uint32_t typeLength = record.mType.size();
    ALOGV("typeLength=%u",typeLength);
    parcel.writeInt32(typeLength);
    for (int j = 0; j < typeLength; j++) {
      ALOGV("mType %d = %u", j, record.mType[j]);
      parcel.writeInt32(record.mType[j]);
    }

    uint8_t idLength = record.mId.size();
    ALOGV("idLength=%d",idLength);
    parcel.writeInt32(idLength);
    for (int j = 0; j < idLength; j++) {
      ALOGV("mId %d = %u", j, record.mId[j]);
      parcel.writeInt32(record.mId[j]);
    }

    uint32_t payloadLength = record.mPayload.size();
    ALOGV("payloadLength=%d",payloadLength);
    parcel.writeInt32(payloadLength);
    for (int j = 0; j < payloadLength; j++) {
      ALOGV("mPayload %d = %u", j, record.mPayload[j]);
      parcel.writeInt32(record.mPayload[j]);
    }
  }

  //TODO check when will parcel release data.
  sendResponse(parcel);
  return true;
}

bool MessageHandler::handleWriteNdefResponse(android::Parcel& parcel)
{
  //TODO write SessionId
  sendResponse(parcel);
  return true;
}

bool MessageHandler::handleConnectResponse(Parcel& parcel)
{
  sendResponse(parcel);
  return true;
}
