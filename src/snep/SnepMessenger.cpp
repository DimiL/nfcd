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

#include "SnepMessenger.h"
#include <vector>

#include "ILlcpSocket.h"
#include "NfcDebug.h"

/**
 * The Simple NDEF Exchange Protocol (SNEP) is a request/response protocol.
 * A SNEP client sends a request to a SNEP server in the form of a protocol
 * version, a request method, the length of an information field in octets,
 * and an information field.
 */
SnepMessenger::SnepMessenger(bool aIsClient,
                             ILlcpSocket* aSocket,
                             uint32_t aFragmentLength)
 : mSocket(aSocket)
 , mFragmentLength(aFragmentLength)
 , mIsClient(aIsClient)
{
}

SnepMessenger::~SnepMessenger()
{
  Close();
}

void SnepMessenger::SendMessage(SnepMessage& aMsg)
{
  ALOGD("%s: enter", FUNC);

  uint8_t remoteContinue;
  if (mIsClient) {
    remoteContinue = SnepMessage::RESPONSE_CONTINUE;
  } else {
    remoteContinue = SnepMessage::REQUEST_CONTINUE;
  }

  std::vector<uint8_t> buf;
  aMsg.ToByteArray(buf);
  uint32_t length = -1;

  if (buf.size() <  mFragmentLength) {
    length = buf.size();
    mSocket->Send(buf);
  } else {
    length = mFragmentLength;
    std::vector<uint8_t> tmpBuf;
    for (uint32_t i = 0; i < mFragmentLength; i++) {
      tmpBuf.push_back(buf[i]);
    }
    mSocket->Send(tmpBuf);
  }

  if (length == buf.size()) {
    ALOGD("%s: exit", FUNC);
    return;
  }

  // Fragmented SNEP message handling.
  // Look for Continue or Reject from peer.
  uint32_t offset = length;
  std::vector<uint8_t> responseBytes;
  mSocket->Receive(responseBytes);

  SnepMessage* snepResponse = SnepMessage::FromByteArray(responseBytes);
  if (!snepResponse) {
    ALOGE("%s: invalid SNEP message", FUNC);
    return;
  }

  if (snepResponse->GetField() != remoteContinue) {
    ALOGE("%s: invalid response from server (%d)", FUNC, snepResponse->GetField());
    delete snepResponse;
    return;
  }

  // Send remaining fragments.
  while (offset < buf.size()) {
    std::vector<uint8_t> tmpBuf;
    length = buf.size() - offset < mFragmentLength ? buf.size() - offset : mFragmentLength;
    // TODO : Need check here
    for(uint32_t i = offset; i < offset + length; i++) {
      tmpBuf.push_back(buf[i]);
    }
    mSocket->Send(tmpBuf);
    offset += length;
  }

  ALOGD("%s: exit", FUNC);
}

/**
 * Get Snep message from remote side.
 */
SnepMessage* SnepMessenger::GetMessage()
{
  ALOGD("%s: enter", FUNC);

  std::vector<uint8_t> partial;
  std::vector<uint8_t> buffer;

  uint8_t fieldContinue = 0;
  uint8_t fieldReject = 0;

  if (mIsClient) {
    /**
     * Request Codes : CONTINUE
     * The client requests that the server send the remaining fragments
     * of a fragmented SNEP response message.
     *
     * Request Codes : REJECT
     * The client is unable to receive remaining fragments of a fragmented
     * SNEP response message.
     */
    fieldContinue = SnepMessage::REQUEST_CONTINUE;
    fieldReject = SnepMessage::REQUEST_REJECT;
  } else {
    /**
     * Response Codes : CONTINUE
     * The server received the first fragment of a fragmented SNEP request
     * message and is able to receive the remaining fragments.
     *
     * Response Codes : REJECT
     * The server is unable to receive remaining fragments of a fragmented
     * SNEP request message.
     */
    fieldContinue = SnepMessage::RESPONSE_CONTINUE;
    fieldReject = SnepMessage::RESPONSE_REJECT;
  }

  uint32_t readSize = 0;
  int size = mSocket->Receive(partial);
  if (size < 0 || size < HEADER_LENGTH) {
    if (!SocketSend(fieldReject)) {
      ALOGE("%s: snep message send fail", FUNC);
      return NULL;
    }
  } else {
    readSize = size - HEADER_LENGTH;
    buffer.insert(buffer.end(), partial.begin(), partial.end());
  }

  const uint8_t requestVersion = partial[0];
  const uint8_t requestField = partial[1];
  const uint32_t requestSize = ((uint32_t)partial[2] << 24) |
                               ((uint32_t)partial[3] << 16) |
                               ((uint32_t)partial[4] <<  8) |
                               ((uint32_t)partial[5]);

  if (((requestVersion & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    // Invalid protocol version; treat message as complete.
    ALOGE("%s: invalid protocol version %d != %d",
       FUNC, ((requestVersion & 0xF0) >> 4), SnepMessage::VERSION_MAJOR);
    return new SnepMessage(requestVersion, requestField, 0, 0, NULL);
  }

  bool doneReading = false;
  if (requestSize > readSize) {
    if (!SocketSend(fieldContinue)) {
      ALOGE("%s: snep message send fail", FUNC);
      return NULL;
    }
  } else {
    doneReading = true;
  }

  // Fragmented SNEP message handling.
  // Remaining fragments.
  while (!doneReading) {
    partial.clear();
    size = mSocket->Receive(partial);
    if (size < 0) {
      if (!SocketSend(fieldReject)) {
        ALOGE("%s: snep message send fail", FUNC);
        return NULL;
      }
    } else {
      readSize += size;
      buffer.insert(buffer.end(), partial.begin(), partial.end());
      if (readSize == requestSize) {
        doneReading = true;
      }
    }
  }

  SnepMessage* snep = SnepMessage::FromByteArray(buffer);
  if (!snep)
    ALOGE("%s: nadly formatted NDEF message, ignoring", FUNC);

  ALOGD("%s: exit", FUNC);
  return snep;
}

void SnepMessenger::Close()
{
  if (mSocket) {
    mSocket->Close();
    mSocket = NULL;
  }
}

bool SnepMessenger::SocketSend(uint8_t aField)
{
  bool status = false;
  std::vector<uint8_t> data;
  SnepMessage* msg = SnepMessage::GetMessage(aField);
  msg->ToByteArray(data);
  status = mSocket->Send(data);
  delete msg;
  return status;
}
