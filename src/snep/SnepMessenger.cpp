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
SnepMessenger::SnepMessenger(bool isClient, ILlcpSocket* socket, uint32_t fragmentLength)
 : mSocket(socket)
 , mFragmentLength(fragmentLength)
 , mIsClient(isClient)
{
}

SnepMessenger::~SnepMessenger()
{
  close();
}

void SnepMessenger::sendMessage(SnepMessage& msg)
{
  ALOGD("%s: enter", FUNC);

  uint8_t remoteContinue;
  if (mIsClient) {
    remoteContinue = SnepMessage::RESPONSE_CONTINUE;
  } else {
    remoteContinue = SnepMessage::REQUEST_CONTINUE;
  }

  std::vector<uint8_t> buf;
  msg.toByteArray(buf);
  uint32_t length = -1;

  if (buf.size() <  mFragmentLength) {
    length = buf.size();
    mSocket->send(buf);
  } else {
    length = mFragmentLength;
    std::vector<uint8_t> tmpBuf;
    for (uint32_t i = 0; i < mFragmentLength; i++)
      tmpBuf.push_back(buf[i]);
    mSocket->send(tmpBuf);
  }

  if (length == buf.size()) {
    ALOGD("%s: exit", FUNC);
    return;
  }

  // Fragmented SNEP message handling.
  // Look for Continue or Reject from peer.
  uint32_t offset = length;
  std::vector<uint8_t> responseBytes;
  mSocket->receive(responseBytes);

  SnepMessage* snepResponse = SnepMessage::fromByteArray(responseBytes);
  if (!snepResponse) {
    ALOGE("%s: invalid SNEP message", FUNC);
    return;
  }

  if (snepResponse->getField() != remoteContinue) {
    ALOGE("%s: invalid response from server (%d)", FUNC, snepResponse->getField());
    delete snepResponse;
    return;
  }

  // Send remaining fragments.
  while (offset < buf.size()) {
    std::vector<uint8_t> tmpBuf;
    length = buf.size() - offset < mFragmentLength ? buf.size() - offset : mFragmentLength;
    // TODO : Need check here
    for(uint32_t i = offset; i < offset + length; i++)
      tmpBuf.push_back(buf[i]);
    mSocket->send(tmpBuf);
    offset += length;
  }

  ALOGD("%s: exit", FUNC);
}

/**
 * Get Snep message from remote side.
 */
SnepMessage* SnepMessenger::getMessage()
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
  int size = mSocket->receive(partial);
  if (size < 0 || size < HEADER_LENGTH) {
    if (!socketSend(fieldReject)) {
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
    if (!socketSend(fieldContinue)) {
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
    size = mSocket->receive(partial);
    if (size < 0) {
      if (!socketSend(fieldReject)) {
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

  SnepMessage* snep = SnepMessage::fromByteArray(buffer);
  if (!snep)
    ALOGE("%s: nadly formatted NDEF message, ignoring", FUNC);

  ALOGD("%s: exit", FUNC);
  return snep;
}

void SnepMessenger::close()
{
  if (mSocket) {
    mSocket->close();
    mSocket = NULL;
  }
}

bool SnepMessenger::socketSend(uint8_t field)
{
  bool status = false;
  std::vector<uint8_t> data;
  SnepMessage* msg = SnepMessage::getMessage(field);
  msg->toByteArray(data);
  status = mSocket->send(data);
  delete msg;
  return status;
}
