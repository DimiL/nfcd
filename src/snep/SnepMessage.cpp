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

#include "SnepMessage.h"

#include "NdefMessage.h"
#include "NfcDebug.h"

SnepMessage::SnepMessage()
 : mNdefMessage(NULL)
{
}

SnepMessage::~SnepMessage()
{
  delete mNdefMessage;
}

SnepMessage::SnepMessage(std::vector<uint8_t>& buf)
{
  int ndefOffset = 0;
  int ndefLength = 0;
  int idx = 0;

  mVersion = buf[idx++];
  mField = buf[idx++];

  mLength = ((uint32_t)buf[idx]     << 24) |
            ((uint32_t)buf[idx + 1] << 16) |
            ((uint32_t)buf[idx + 2] <<  8) |
             (uint32_t)buf[idx + 3];
  idx += 4 ;

  if (mField == SnepMessage::REQUEST_GET) {
    mAcceptableLength = ((uint32_t)buf[idx]     << 24) |
                        ((uint32_t)buf[idx + 1] << 16) |
                        ((uint32_t)buf[idx + 2] <<  8) |
                         (uint32_t)buf[idx + 3];
    idx += 4;
    ndefOffset = SnepMessage::HEADER_LENGTH + 4;
    ndefLength = mLength - 4;
  } else {
    mAcceptableLength = -1;
    ndefOffset = SnepMessage::HEADER_LENGTH;
    ndefLength = mLength;
  }

  if (ndefLength > 0) {
    mNdefMessage = new NdefMessage();
    // TODO : Need to check idx is correct.
    mNdefMessage->init(buf, idx);
  } else {
    mNdefMessage = NULL;
  }
}

SnepMessage::SnepMessage(uint8_t version, uint8_t field, int length,
                 int acceptableLength, NdefMessage* ndefMessage)
{
  mVersion = version;
  mField = field;
  mLength = length;
  mAcceptableLength = acceptableLength;
  mNdefMessage = new NdefMessage(ndefMessage);
}

bool SnepMessage::isValidFormat(std::vector<uint8_t>& buf)
{
  int size = buf.size();
  // version(1), field(1), length(4)
  if (size < SnepMessage::HEADER_LENGTH) {
    return false;
  }

  // acceptable length(0 or 4)
  if (buf[1] == SnepMessage::REQUEST_GET &&
      size < SnepMessage::HEADER_LENGTH + 4) {
    return false;
  }

  return true;
}

SnepMessage* SnepMessage::getGetRequest(int acceptableLength, NdefMessage& ndef)
{
  std::vector<uint8_t> buf;
  ndef.toByteArray(buf);
  return new SnepMessage(SnepMessage::VERSION, SnepMessage::REQUEST_GET, 4 + buf.size(), acceptableLength, &ndef);
}

SnepMessage* SnepMessage::getPutRequest(NdefMessage& ndef)
{
  std::vector<uint8_t> buf;
  ndef.toByteArray(buf);
  return new SnepMessage(SnepMessage::VERSION, SnepMessage::REQUEST_PUT, buf.size(), 0, &ndef);
}

SnepMessage* SnepMessage::getMessage(uint8_t field)
{
  return new SnepMessage(SnepMessage::VERSION, field, 0, 0, NULL);
}

SnepMessage* SnepMessage::getSuccessResponse(NdefMessage* ndef)
{
  if (!ndef) {
    return new SnepMessage(SnepMessage::VERSION, SnepMessage::RESPONSE_SUCCESS, 0, 0, NULL);
  } else {
    std::vector<uint8_t> buf;
    ndef->toByteArray(buf);
    return new SnepMessage(SnepMessage::VERSION, SnepMessage::RESPONSE_SUCCESS, buf.size(), 0, ndef);
  }
}

SnepMessage* SnepMessage::fromByteArray(std::vector<uint8_t>& buf)
{
  return SnepMessage::isValidFormat(buf) ? new SnepMessage(buf) : NULL;
}

SnepMessage* SnepMessage::fromByteArray(uint8_t* pBuf, int size)
{
  std::vector<uint8_t> buf;
  for (int i = 0; i < size; i++) {
    buf[i] = pBuf[i];
  }

  return fromByteArray(buf);
}

void SnepMessage::toByteArray(std::vector<uint8_t>& buf)
{
  if (mNdefMessage) {
    mNdefMessage->toByteArray(buf);
  }

  std::vector<uint8_t> snepHeader;

  snepHeader.push_back(mVersion);
  snepHeader.push_back(mField);
  if (mField == SnepMessage::REQUEST_GET) {
    uint32_t len = buf.size() + 4;
    snepHeader.push_back((len >> 24) & 0xFF);
    snepHeader.push_back((len >> 16) & 0xFF);
    snepHeader.push_back((len >>  8) & 0xFF);
    snepHeader.push_back( len & 0xFF);
    snepHeader.push_back((mAcceptableLength >> 24) & 0xFF);
    snepHeader.push_back((mAcceptableLength >> 16) & 0xFF);
    snepHeader.push_back((mAcceptableLength >>  8) & 0xFF);
    snepHeader.push_back( mAcceptableLength & 0xFF);
  } else {
    uint32_t len = buf.size();
    snepHeader.push_back((len >> 24) & 0xFF);
    snepHeader.push_back((len >> 16) & 0xFF);
    snepHeader.push_back((len >>  8) & 0xFF);
    snepHeader.push_back( len & 0xFF);
  }

  buf.insert(buf.begin(), snepHeader.begin(), snepHeader.end());
}
