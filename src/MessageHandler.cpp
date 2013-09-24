/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#define MAJOR_VERSION (1)
#define MINOR_VERSION (5)

using android::Parcel;

void MessageHandler::notifyInitialized(Parcel& parcel)
{
  parcel.writeInt32(0); // status
  parcel.writeInt32(MAJOR_VERSION);
  parcel.writeInt32(MINOR_VERSION);
  sendResponse(parcel);
}

void MessageHandler::notifyTechDiscovered(Parcel& parcel, void* data)
{
  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(data);
  std::vector<uint8_t>& techList = pINfcTag->getTechList();

  parcel.writeInt32(SessionId::generateNewId());

  int numberOfTech = techList.size();
  parcel.writeInt32(numberOfTech);
  void* dest = parcel.writeInplace(numberOfTech);
  memcpy(dest, &techList.front(), numberOfTech);

  sendResponse(parcel);
}

void MessageHandler::notifyTechLost(Parcel& parcel)
{
  parcel.writeInt32(SessionId::getCurrentId());
  sendResponse(parcel);
}

void MessageHandler::processRequest(const uint8_t* data, size_t dataLen)
{
  Parcel parcel;
  int32_t sizeLe, size, request, token;
  uint32_t status;

  ALOGD("%s enter data=%p, dataLen=%d", __func__, data, dataLen);
  parcel.setData((uint8_t*)data, dataLen);
  status = parcel.readInt32(&request);
  if (status != 0) {
    ALOGE("Invalid request block");
    return;
  }

  //TODO remove token
  token = 0;

  switch (request) {
    case NFC_REQUEST_CONFIG:
      handleConfigRequest(parcel, token);
      break;
    case NFC_REQUEST_GET_DETAILS:
      handleReadNdefDetailRequest(parcel, token);
      break;
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
    case NFC_REQUEST_MAKE_NDEF_READ_ONLY:
      handleMakeNdefReadonlyRequest(parcel, token);
      break;
    default:
      ALOGE("Unhandled Request %d", request);
      break;
  }
}

void MessageHandler::processResponse(NfcResponseType response, int token, NfcErrorCode error, void* data)
{
  ALOGD("%s enter response=%d, token=%d ", __func__, response, token);
  Parcel parcel;
  parcel.writeInt32(response);
  parcel.writeInt32(error);

  switch (response) {
    case NFC_RESPONSE_CONFIG:
      handleConfigResponse(parcel, data);
      break;
    case NFC_RESPONSE_READ_NDEF_DETAILS:
      handleReadNdefDetailResponse(parcel, data);
      break;
    case NFC_RESPONSE_READ_NDEF:
      handleReadNdefResponse(parcel, data);
      break;
    case NFC_RESPONSE_GENERAL:
      handleResponse(parcel);
      break;
    default:
      ALOGE("Not implement");
      break;
  }
}

void MessageHandler::processNotification(NfcNotificationType notification, void* data)
{
  ALOGD("processNotificaton notification=%d", notification);
  Parcel parcel;
  parcel.writeInt32(notification);

  switch (notification) {
    case NFC_NOTIFICATION_INITIALIZED :
      notifyInitialized(parcel);
      break;
    case NFC_NOTIFICATION_TECH_DISCOVERED:
      notifyTechDiscovered(parcel, data);
      break;
    case NFC_NOTIFICATION_TECH_LOST:
      notifyTechLost(parcel);
      break;
    default:
      ALOGE("Not implement");
      break;
  }
}

void MessageHandler::setSocket(NfcIpcSocket* socket)
{
  mSocket = socket;
}

void MessageHandler::onSocketConnected()
{
  NfcService::onSocketConnected();
}

void MessageHandler::sendResponse(Parcel& parcel)
{
  mSocket->writeToOutgoingQueue(const_cast<uint8_t*>(parcel.data()), parcel.dataSize());
}

bool MessageHandler::handleConfigRequest(Parcel& parcel, int token)
{
  int sessionId = parcel.readInt32();
  //TODO check SessionId
  return NfcService::handleConfigRequest(token);
}

bool MessageHandler::handleReadNdefDetailRequest(Parcel& parcel, int token)
{
  int sessionId = parcel.readInt32();
  //TODO check SessionId
  return NfcService::handleReadNdefDetailRequest(token);
}

bool MessageHandler::handleReadNdefRequest(Parcel& parcel, int token)
{
  int sessionId = parcel.readInt32();
  //TODO check SessionId
  return NfcService::handleReadNdefRequest(token);
}

bool MessageHandler::handleWriteNdefRequest(Parcel& parcel, int token)
{
  NdefMessagePdu ndefMessagePdu;
  NdefMessage* ndefMessage = new NdefMessage();

  int sessionId = parcel.readInt32();
  //TODO check SessionId

  uint32_t numRecords = parcel.readInt32();
  ndefMessagePdu.numRecords = numRecords;
  ndefMessagePdu.records = new NdefRecordPdu[numRecords];

  for (uint32_t i = 0; i < numRecords; i++) {
    ndefMessagePdu.records[i].tnf = parcel.readInt32();

    uint32_t typeLength = parcel.readInt32();
    ndefMessagePdu.records[i].typeLength = typeLength;
    ndefMessagePdu.records[i].type = new uint8_t[typeLength];
    const void* data = parcel.readInplace(typeLength);
    memcpy(ndefMessagePdu.records[i].type, data, typeLength);

    uint32_t idLength = parcel.readInt32();
    ndefMessagePdu.records[i].idLength = idLength;
    ndefMessagePdu.records[i].id = new uint8_t[idLength];
    data = parcel.readInplace(idLength);
    memcpy(ndefMessagePdu.records[i].id, data, idLength);

    uint32_t payloadLength = parcel.readInt32();
    ndefMessagePdu.records[i].payloadLength = payloadLength;
    ndefMessagePdu.records[i].payload = new uint8_t[payloadLength];
    data = parcel.readInplace(payloadLength);
    memcpy(ndefMessagePdu.records[i].payload, data, payloadLength);
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
  int sessionId = parcel.readInt32();
  //TODO check SessionId

  //TODO should only read 1 octet here.
  int32_t techType = parcel.readInt32();
  ALOGD("%s techType=%d", __func__, techType);
  NfcService::handleConnect(techType, token);
  return true;
}

bool MessageHandler::handleCloseRequest(Parcel& parcel, int token)
{
  NfcService::handleCloseRequest();
  return true;
}

bool MessageHandler::handleMakeNdefReadonlyRequest(Parcel& parcel, int token)
{
  NfcService::handleMakeNdefReadonlyRequest(token);
  return true;
}

bool MessageHandler::handleConfigResponse(Parcel& parcel, void* data)
{
  sendResponse(parcel);
  return true;
}

bool MessageHandler::handleReadNdefDetailResponse(Parcel& parcel, void* data)
{
  NdefDetail* ndefDetail = reinterpret_cast<NdefDetail*>(data);

  parcel.writeInt32(SessionId::getCurrentId());

  parcel.writeInt32(ndefDetail->maxSupportedLength);
  parcel.writeInt32(ndefDetail->mode);

  sendResponse(parcel);
  return true;
}

bool MessageHandler::handleReadNdefResponse(Parcel& parcel, void* data)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(data);

  parcel.writeInt32(SessionId::getCurrentId());

  int numRecords = ndef->mRecords.size();
  ALOGD("numRecords=%d", numRecords);
  parcel.writeInt32(numRecords);

  for (int i = 0; i < numRecords; i++) {
    NdefRecord &record = ndef->mRecords[i];

    ALOGD("tnf=%u",record.mTnf);
    parcel.writeInt32(record.mTnf);

    uint32_t typeLength = record.mType.size();
    ALOGD("typeLength=%u",typeLength);
    parcel.writeInt32(typeLength);
    void* dest = parcel.writeInplace(typeLength);
    if (dest == NULL) {
      ALOGE("writeInplace returns NULL");
      return false;
    }
    memcpy(dest, &record.mType.front(), typeLength);

    uint32_t idLength = record.mId.size();
    ALOGD("idLength=%d",idLength);
    parcel.writeInt32(idLength);
    dest = parcel.writeInplace(idLength);
    memcpy(dest, &record.mId.front(), idLength);

    uint32_t payloadLength = record.mPayload.size();
    ALOGD("payloadLength=%u",payloadLength);
    parcel.writeInt32(payloadLength);
    dest = parcel.writeInplace(payloadLength);
    memcpy(dest, &record.mPayload.front(), payloadLength);
    for (uint32_t j = 0; j < payloadLength; j++) {
      ALOGD("mPayload %d = %u", j, record.mPayload[j]);
    }
  }

  sendResponse(parcel);
  return true;
}

bool MessageHandler::handleResponse(Parcel& parcel)
{
  parcel.writeInt32(SessionId::getCurrentId());
  sendResponse(parcel);
  return true;
}
