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
#define MINOR_VERSION (22)

using android::Parcel;

void MessageHandler::NotifyInitialized(Parcel& aParcel)
{
  aParcel.writeInt32(0); // status
  aParcel.writeInt32(MAJOR_VERSION);
  aParcel.writeInt32(MINOR_VERSION);
  SendResponse(aParcel);
}

void MessageHandler::NotifyTechDiscovered(Parcel& aParcel, void* aData)
{
  TechDiscoveredEvent *event = reinterpret_cast<TechDiscoveredEvent*>(aData);

  aParcel.writeInt32(event->sessionId);
  aParcel.writeInt32(event->isP2P);
  aParcel.writeInt32(event->techCount);
  void* dest = aParcel.writeInplace(event->techCount);
  memcpy(dest, event->techList, event->techCount);
  aParcel.writeInt32(event->tagIdCount);
  void* idPtr = aParcel.writeInplace(event->tagIdCount);
  memcpy(idPtr, event->tagId, event->tagIdCount);
  aParcel.writeInt32(event->ndefMsgCount);
  SendNdefMsg(aParcel, event->ndefMsg);
  SendNdefInfo(aParcel, event->ndefInfo);
  SendResponse(aParcel);
}

void MessageHandler::NotifyTechLost(Parcel& aParcel, void* aData)
{
  aParcel.writeInt32(reinterpret_cast<int>(aData));
  SendResponse(aParcel);
}

void MessageHandler::NotifyTransactionEvent(Parcel& aParcel, void* aData)
{
  TransactionEvent *event = reinterpret_cast<TransactionEvent*>(aData);

  aParcel.writeInt32(NfcUtil::ConvertOriginType(event->originType));
  aParcel.writeInt32(event->originIndex);

  aParcel.writeInt32(event->aidLen);
  void* aid = aParcel.writeInplace(event->aidLen);
  memcpy(aid, event->aid, event->aidLen);

  aParcel.writeInt32(event->payloadLen);
  void* payload = aParcel.writeInplace(event->payloadLen);
  memcpy(payload, event->payload, event->payloadLen);

  SendResponse(aParcel);

  delete event;
}

void MessageHandler::NotifyNdefReceived(Parcel& aParcel, void* aData)
{
  NdefReceivedEvent *event = reinterpret_cast<NdefReceivedEvent*>(aData);

  aParcel.writeInt32(event->sessionId);
  aParcel.writeInt32(event->ndefMsgCount);
  SendNdefMsg(aParcel, event->ndefMsg);
  SendResponse(aParcel);
}

void MessageHandler::ProcessRequest(const uint8_t* aData, size_t aDataLen)
{
  Parcel parcel;
  int32_t sizeLe, size, request;
  uint32_t status;

  NFCD_DEBUG("enter data=%p, dataLen=%d", aData, aDataLen);
  parcel.setData((uint8_t*)aData, aDataLen);
  status = parcel.readInt32(&request);
  if (status != 0) {
    NFCD_ERROR("Invalid request block");
    return;
  }

  switch (request) {
    case NFC_REQUEST_CHANGE_RF_STATE:
      HandleChangeRFStateRequest(parcel);
      break;
    case NFC_REQUEST_READ_NDEF:
      HandleReadNdefRequest(parcel);
      break;
    case NFC_REQUEST_WRITE_NDEF:
      HandleWriteNdefRequest(parcel);
      break;
    case NFC_REQUEST_MAKE_NDEF_READ_ONLY:
      HandleMakeNdefReadonlyRequest(parcel);
      break;
    case NFC_REQUEST_FORMAT:
      HandleNdefFormatRequest(parcel);
      break;
    case NFC_REQUEST_TRANSCEIVE:
      HandleTagTransceiveRequest(parcel);
      break;
    default:
      NFCD_ERROR("Unhandled Request=%d", request);
      break;
  }
}

void MessageHandler::ProcessResponse(NfcResponseType aResponse, NfcErrorCode aError, void* aData)
{
  NFCD_DEBUG("enter response=%d, error=%d", aResponse, aError);
  Parcel parcel;
  parcel.writeInt32(0); // Parcel Size.
  parcel.writeInt32(aResponse);
  parcel.writeInt32(aError);

  switch (aResponse) {
    case NFC_RESPONSE_CHANGE_RF_STATE:
      HandleChangeRFStateResponse(parcel, aData);
      break;
    case NFC_RESPONSE_READ_NDEF:
      HandleReadNdefResponse(parcel, aData);
      break;
    case NFC_RESPONSE_WRITE_NDEF: // Fall through.
    case NFC_RESPONSE_MAKE_READ_ONLY:
    case NFC_RESPONSE_FORMAT:
      HandleResponse(parcel);
      break;
    case NFC_RESPONSE_TAG_TRANSCEIVE:
      HandleTagTransceiveResponse(parcel, aData);
      break;
    default:
      NFCD_ERROR("Not implement");
      break;
  }
}

void MessageHandler::ProcessNotification(NfcNotificationType aNotification, void* aData)
{
  NFCD_DEBUG("processNotificaton notification=%d", aNotification);
  Parcel parcel;
  parcel.writeInt32(0); // Parcel Size.
  parcel.writeInt32(aNotification | 0x80000000);

  switch (aNotification) {
    case NFC_NOTIFICATION_INITIALIZED :
      NotifyInitialized(parcel);
      break;
    case NFC_NOTIFICATION_TECH_DISCOVERED:
      NotifyTechDiscovered(parcel, aData);
      break;
    case NFC_NOTIFICATION_TECH_LOST:
      NotifyTechLost(parcel, aData);
      break;
    case NFC_NOTIFICATION_TRANSACTION_EVENT:
      NotifyTransactionEvent(parcel, aData);
      break;
    case NFC_NOTIFICATION_NDEF_RECEIVED:
      NotifyNdefReceived(parcel, aData);
      break;
    default:
      NFCD_ERROR("Not implement");
      break;
  }
}

void MessageHandler::SetOutgoingSocket(NfcIpcSocket* aSocket)
{
  mSocket = aSocket;
}

void MessageHandler::SendResponse(Parcel& aParcel)
{
  aParcel.setDataPosition(0);
  uint32_t sizeBE = htonl(aParcel.dataSize() - sizeof(int));
  aParcel.writeInt32(sizeBE);
  mSocket->WriteToOutgoingQueue(const_cast<uint8_t*>(aParcel.data()), aParcel.dataSize());
}

bool MessageHandler::HandleChangeRFStateRequest(Parcel& aParcel)
{
  int rfState = aParcel.readInt32();
  bool value;
  switch (rfState) {
    case NFC_RF_STATE_IDLE: // Fall through.
    case NFC_RF_STATE_DISCOVERY:
      value = rfState == NFC_RF_STATE_DISCOVERY;
      return mService->HandleEnableRequest(value);
    case NFC_RF_STATE_LISTEN:
      value = rfState == NFC_RF_STATE_LISTEN;
      return mService->HandleEnterLowPowerRequest(value);
  }
  return false;

}

bool MessageHandler::HandleReadNdefRequest(Parcel& aParcel)
{
  int sessionId = aParcel.readInt32();
  //TODO check SessionId
  return mService->HandleReadNdefRequest();
}

bool MessageHandler::HandleWriteNdefRequest(Parcel& aParcel)
{
  NdefMessagePdu ndefMessagePdu;
  NdefMessage* ndefMessage = new NdefMessage();

  int sessionId = aParcel.readInt32();
  //TODO check SessionId
  bool isP2P = aParcel.readInt32() != 0;

  uint32_t numRecords = aParcel.readInt32();
  ndefMessagePdu.numRecords = numRecords;
  ndefMessagePdu.records = new NdefRecordPdu[numRecords];

  for (uint32_t i = 0; i < numRecords; i++) {
    ndefMessagePdu.records[i].tnf = aParcel.readInt32();

    uint32_t typeLength = aParcel.readInt32();
    ndefMessagePdu.records[i].typeLength = typeLength;
    ndefMessagePdu.records[i].type = new uint8_t[typeLength];
    const void* data = aParcel.readInplace(typeLength);
    memcpy(ndefMessagePdu.records[i].type, data, typeLength);

    uint32_t idLength = aParcel.readInt32();
    ndefMessagePdu.records[i].idLength = idLength;
    ndefMessagePdu.records[i].id = new uint8_t[idLength];
    data = aParcel.readInplace(idLength);
    memcpy(ndefMessagePdu.records[i].id, data, idLength);

    uint32_t payloadLength = aParcel.readInt32();
    ndefMessagePdu.records[i].payloadLength = payloadLength;
    ndefMessagePdu.records[i].payload = new uint8_t[payloadLength];
    data = aParcel.readInplace(payloadLength);
    memcpy(ndefMessagePdu.records[i].payload, data, payloadLength);
  }

  NfcUtil::ConvertNdefPduToNdefMessage(ndefMessagePdu, ndefMessage);

  for (uint32_t i = 0; i < numRecords; i++) {
    delete[] ndefMessagePdu.records[i].type;
    delete[] ndefMessagePdu.records[i].id;
    delete[] ndefMessagePdu.records[i].payload;
  }
  delete[] ndefMessagePdu.records;

  return mService->HandleWriteNdefRequest(ndefMessage, isP2P);
}

bool MessageHandler::HandleMakeNdefReadonlyRequest(Parcel& aParcel)
{
  mService->HandleMakeNdefReadonlyRequest();
  return true;
}

bool MessageHandler::HandleNdefFormatRequest(Parcel& aParcel)
{
  mService->HandleNdefFormatRequest();
  return true;
}

bool MessageHandler::HandleTagTransceiveRequest(Parcel& aParcel)
{
  int sessionId = aParcel.readInt32();
  int tech = aParcel.readInt32();
  int bufLen = aParcel.readInt32();

  const void* buf = aParcel.readInplace(bufLen);
  mService->HandleTagTransceiveRequest(tech, static_cast<const uint8_t*>(buf), bufLen);
  return true;
}

bool MessageHandler::HandleChangeRFStateResponse(Parcel& aParcel, void* aData)
{
  aParcel.writeInt32(*reinterpret_cast<int*>(aData));
  SendResponse(aParcel);
  return true;
}

bool MessageHandler::HandleReadNdefResponse(Parcel& aParcel, void* aData)
{
  NdefMessage* ndef = reinterpret_cast<NdefMessage*>(aData);

  aParcel.writeInt32(SessionId::GetCurrentId());

  SendNdefMsg(aParcel, ndef);
  SendResponse(aParcel);

  return true;
}

bool MessageHandler::HandleTagTransceiveResponse(Parcel& aParcel, void* aData)
{
  std::vector<uint8_t>* response = reinterpret_cast<std::vector<uint8_t>*>(aData);
  uint32_t length = response->size();

  aParcel.writeInt32(SessionId::GetCurrentId());
  aParcel.writeInt32(length);

  uint8_t* buf = static_cast<uint8_t*>(aParcel.writeInplace(length));
  std::copy(response->begin(), response->end(), buf);

  SendResponse(aParcel);

  return true;
}

bool MessageHandler::HandleResponse(Parcel& aParcel)
{
  aParcel.writeInt32(SessionId::GetCurrentId());
  SendResponse(aParcel);
  return true;
}

bool MessageHandler::SendNdefMsg(Parcel& aParcel, NdefMessage* aNdef)
{
  if (!aNdef)
    return false;

  int numRecords = aNdef->mRecords.size();
  NFCD_DEBUG("numRecords=%d", numRecords);
  aParcel.writeInt32(numRecords);

  for (int i = 0; i < numRecords; i++) {
    NdefRecord &record = aNdef->mRecords[i];

    NFCD_DEBUG("tnf=%u", record.mTnf);
    aParcel.writeInt32(record.mTnf);

    uint32_t typeLength = record.mType.size();
    NFCD_DEBUG("typeLength=%u", typeLength);
    aParcel.writeInt32(typeLength);
    void* dest = aParcel.writeInplace(typeLength);
    if (dest == NULL) {
      NFCD_ERROR("writeInplace returns NULL");
      return false;
    }
    memcpy(dest, &record.mType.front(), typeLength);

    uint32_t idLength = record.mId.size();
    NFCD_DEBUG("idLength=%d", idLength);
    aParcel.writeInt32(idLength);
    dest = aParcel.writeInplace(idLength);
    memcpy(dest, &record.mId.front(), idLength);

    uint32_t payloadLength = record.mPayload.size();
    NFCD_DEBUG("payloadLength=%u", payloadLength);
    aParcel.writeInt32(payloadLength);
    dest = aParcel.writeInplace(payloadLength);
    memcpy(dest, &record.mPayload.front(), payloadLength);
    for (uint32_t j = 0; j < payloadLength; j++) {
      NFCD_DEBUG("mPayload %d = %u", j, record.mPayload[j]);
    }
  }

  return true;
}

bool MessageHandler::SendNdefInfo(Parcel& aParcel, NdefInfo* aInfo)
{
  // if contain ndef information
  aParcel.writeInt32(aInfo ? true : false);

  if (!aInfo) {
    return false;
  }

  // ndef tyoe
  NfcNdefType type = (NfcUtil::ConvertNdefType(aInfo->ndefType));
  aParcel.writeInt32(static_cast<int>(type));

  // max support length
  aParcel.writeInt32(aInfo->maxSupportedLength);

  // is ready only
  aParcel.writeInt32(aInfo->isReadOnly);

  // ndef formatable
  aParcel.writeInt32(aInfo->isFormatable);

  return true;
}
