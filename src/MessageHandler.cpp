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

#include "MessageHandler.h"
#include <arpa/inet.h> // for htonl
#include "NfcService.h"
#include "NfcIpcSocket.h"
#include "NfcUtil.h"
#include "NdefMessage.h"
#include "NdefRecord.h"
#include "SessionId.h"
#include "NfcDebug.h"

#define MAJOR_VERSION (1)
#define MINOR_VERSION (14)

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
  TechDiscoveredEvent *event = reinterpret_cast<TechDiscoveredEvent*>(data);

  parcel.writeInt32(event->sessionId);
  parcel.writeInt32(event->techCount);
  void* dest = parcel.writeInplace(event->techCount);
  memcpy(dest, event->techList, event->techCount);
  parcel.writeInt32(event->ndefMsgCount);
  sendNdefMsg(parcel, event->ndefMsg);
  sendNdefInfo(parcel, event->ndefInfo);
  sendResponse(parcel);
}

void MessageHandler::notifyTechLost(Parcel& parcel, void* data)
{
  parcel.writeInt32(reinterpret_cast<int>(data));
  sendResponse(parcel);
}

void MessageHandler::notifyTransactionEvent(Parcel& parcel, void* data)
{
  TransactionEvent *event = reinterpret_cast<TransactionEvent*>(data);

  parcel.writeInt32(NfcUtil::convertOriginType(event->originType));
  parcel.writeInt32(event->originIndex);

  parcel.writeInt32(event->aidLen);
  void* aid = parcel.writeInplace(event->aidLen);
  memcpy(aid, event->aid, event->aidLen);

  parcel.writeInt32(event->payloadLen);
  void* payload = parcel.writeInplace(event->payloadLen);
  memcpy(payload, event->payload, event->payloadLen);

  sendResponse(parcel);

  delete event;
}

void MessageHandler::processRequest(const uint8_t* data, size_t dataLen)
{
  Parcel parcel;
  int32_t sizeLe, size, request;
  uint32_t status;

  ALOGD("%s enter data=%p, dataLen=%d", FUNC, data, dataLen);
  parcel.setData((uint8_t*)data, dataLen);
  status = parcel.readInt32(&request);
  if (status != 0) {
    ALOGE("Invalid request block");
    return;
  }

  switch (request) {
    case NFC_REQUEST_CONFIG:
      handleConfigRequest(parcel);
      break;
    case NFC_REQUEST_READ_NDEF:
      handleReadNdefRequest(parcel);
      break;
    case NFC_REQUEST_WRITE_NDEF:
      handleWriteNdefRequest(parcel);
      break;
    case NFC_REQUEST_CONNECT:
      handleConnectRequest(parcel);
      break;
    case NFC_REQUEST_CLOSE:
      handleCloseRequest(parcel);
      break;
    case NFC_REQUEST_MAKE_NDEF_READ_ONLY:
      handleMakeNdefReadonlyRequest(parcel);
      break;
    default:
      ALOGE("Unhandled Request %d", request);
      break;
  }
}

void MessageHandler::processResponse(NfcResponseType response, NfcErrorCode error, void* data)
{
  ALOGD("%s enter response=%d, error=%d", FUNC, response, error);
  Parcel parcel;
  parcel.writeInt32(0); // Parcel Size.
  parcel.writeInt32(response);
  parcel.writeInt32(error);

  switch (response) {
    case NFC_RESPONSE_CONFIG:
      handleConfigResponse(parcel, data);
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
  parcel.writeInt32(0); // Parcel Size.
  parcel.writeInt32(notification);

  switch (notification) {
    case NFC_NOTIFICATION_INITIALIZED :
      notifyInitialized(parcel);
      break;
    case NFC_NOTIFICATION_TECH_DISCOVERED:
      notifyTechDiscovered(parcel, data);
      break;
    case NFC_NOTIFICATION_TECH_LOST:
      notifyTechLost(parcel, data);
      break;
    case NFC_NOTIFICATION_TRANSACTION_EVENT:
      notifyTransactionEvent(parcel, data);
      break;
    default:
      ALOGE("Not implement");
      break;
  }
}

void MessageHandler::setOutgoingSocket(NfcIpcSocket* socket)
{
  mSocket = socket;
}

void MessageHandler::sendResponse(Parcel& parcel)
{
  parcel.setDataPosition(0);
  uint32_t sizeBE = htonl(parcel.dataSize() - sizeof(int));
  parcel.writeInt32(sizeBE);
  mSocket->writeToOutgoingQueue(const_cast<uint8_t*>(parcel.data()), parcel.dataSize());
}

bool MessageHandler::handleConfigRequest(Parcel& parcel)
{
  // TODO, what does NFC_POWER_FULL mean
  // - OFF -> ON?
  // - Low Power -> Full Power?
  int powerLevel = parcel.readInt32();
  bool value;
  switch (powerLevel) {
    case NFC_POWER_OFF: // Fall through.
    case NFC_POWER_FULL:
      value = powerLevel == NFC_POWER_FULL;
      return mService->handleEnableRequest(value);
    case NFC_POWER_LOW:
      value = powerLevel == NFC_POWER_LOW;
      return mService->handleEnterLowPowerRequest(value);
  }
  return false;

}

bool MessageHandler::handleReadNdefRequest(Parcel& parcel)
{
  int sessionId = parcel.readInt32();
  //TODO check SessionId
  return mService->handleReadNdefRequest();
}

bool MessageHandler::handleWriteNdefRequest(Parcel& parcel)
{
  NdefMessagePdu ndefMessagePdu;
  NdefMessage* ndefMessage = new NdefMessage();

  int sessionId = parcel.readInt32();
  //TODO check SessionId
  bool isP2P = parcel.readInt32() != 0;

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

  return mService->handleWriteNdefRequest(ndefMessage, isP2P);
}

bool MessageHandler::handleConnectRequest(Parcel& parcel)
{
  int sessionId = parcel.readInt32();
  //TODO check SessionId

  //TODO should only read 1 octet here.
  int32_t techType = parcel.readInt32();
  ALOGD("%s techType=%d", FUNC, techType);
  mService->handleConnect(techType);
  return true;
}

bool MessageHandler::handleCloseRequest(Parcel& parcel)
{
  mService->handleCloseRequest();
  return true;
}

bool MessageHandler::handleMakeNdefReadonlyRequest(Parcel& parcel)
{
  mService->handleMakeNdefReadonlyRequest();
  return true;
}

bool MessageHandler::handleConfigResponse(Parcel& parcel, void* data)
{
  sendResponse(parcel);
  return true;
}

bool MessageHandler::handleReadNdefResponse(Parcel& parcel, void* data)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(data);

  parcel.writeInt32(SessionId::getCurrentId());

  sendNdefMsg(parcel, ndef);
  sendResponse(parcel);

  return true;
}

bool MessageHandler::handleResponse(Parcel& parcel)
{
  parcel.writeInt32(SessionId::getCurrentId());
  sendResponse(parcel);
  return true;
}

bool MessageHandler::sendNdefMsg(Parcel& parcel, NdefMessage* ndef)
{
  if (!ndef)
    return false;

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

  return true;
}

bool MessageHandler::sendNdefInfo(Parcel& parcel, NdefInfo* info)
{
  // if contain ndef information
  parcel.writeInt32(info ? true : false);

  if (!info) {
    return false;
  }

  // ndef tyoe
  NfcNdefType type = (NfcUtil::convertNdefType(info->ndefType));
  parcel.writeInt32(static_cast<int>(type));

  // max support length
  parcel.writeInt32(info->maxSupportedLength);

  // is ready only
  parcel.writeInt32(info->isReadOnly);

  // ndef formatable
  parcel.writeInt32(info->isFormatable);

  return true;
}
