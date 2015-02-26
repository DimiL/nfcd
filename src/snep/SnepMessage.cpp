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

SnepMessage::SnepMessage(std::vector<uint8_t>& aBuf)
{
  int ndefOffset = 0;
  int ndefLength = 0;
  int idx = 0;

  mVersion = aBuf[idx++];
  mField = aBuf[idx++];

  mLength = ((uint32_t)aBuf[idx]     << 24) |
            ((uint32_t)aBuf[idx + 1] << 16) |
            ((uint32_t)aBuf[idx + 2] <<  8) |
             (uint32_t)aBuf[idx + 3];
  idx += 4 ;

  if (mField == SnepMessage::REQUEST_GET) {
    mAcceptableLength = ((uint32_t)aBuf[idx]     << 24) |
                        ((uint32_t)aBuf[idx + 1] << 16) |
                        ((uint32_t)aBuf[idx + 2] <<  8) |
                         (uint32_t)aBuf[idx + 3];
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
    mNdefMessage->Init(aBuf, idx);
  } else {
    mNdefMessage = NULL;
  }
}

SnepMessage::SnepMessage(uint8_t aVersion,
                         uint8_t aField,
                         int aLength,
                         int aAcceptableLength,
                         NdefMessage* aNdefMessage)
{
  mVersion = aVersion;
  mField = aField;
  mLength = aLength;
  mAcceptableLength = aAcceptableLength;
  mNdefMessage = new NdefMessage(aNdefMessage);
}

bool SnepMessage::IsValidFormat(std::vector<uint8_t>& aBuf)
{
  int size = aBuf.size();
  // version(1), field(1), length(4)
  if (size < SnepMessage::HEADER_LENGTH) {
    return false;
  }

  // acceptable length(0 or 4)
  if (aBuf[1] == SnepMessage::REQUEST_GET &&
      size < SnepMessage::HEADER_LENGTH + 4) {
    return false;
  }

  return true;
}

SnepMessage* SnepMessage::GetGetRequest(int aAcceptableLength,
                                        NdefMessage& aNdef)
{
  std::vector<uint8_t> buf;
  aNdef.ToByteArray(buf);
  return new SnepMessage(SnepMessage::VERSION,
                         SnepMessage::REQUEST_GET,
                         4 + buf.size(),
                         aAcceptableLength,
                         &aNdef);
}

SnepMessage* SnepMessage::GetPutRequest(NdefMessage& aNdef)
{
  std::vector<uint8_t> buf;
  aNdef.ToByteArray(buf);
  return new SnepMessage(SnepMessage::VERSION,
                         SnepMessage::REQUEST_PUT,
                         buf.size(),
                         0,
                         &aNdef);
}

SnepMessage* SnepMessage::GetMessage(uint8_t aField)
{
  return new SnepMessage(SnepMessage::VERSION, aField, 0, 0, NULL);
}

SnepMessage* SnepMessage::GetSuccessResponse(NdefMessage* aNdef)
{
  if (!aNdef) {
    return new SnepMessage(SnepMessage::VERSION, SnepMessage::RESPONSE_SUCCESS, 0, 0, NULL);
  } else {
    std::vector<uint8_t> buf;
    aNdef->ToByteArray(buf);
    return new SnepMessage(SnepMessage::VERSION, SnepMessage::RESPONSE_SUCCESS, buf.size(), 0, aNdef);
  }
}

SnepMessage* SnepMessage::FromByteArray(std::vector<uint8_t>& aBuf)
{
  return SnepMessage::IsValidFormat(aBuf) ? new SnepMessage(aBuf) : NULL;
}

SnepMessage* SnepMessage::FromByteArray(uint8_t* aBuf,
                                        int aSize)
{
  std::vector<uint8_t> buf;
  for (int i = 0; i < aSize; i++) {
    buf[i] = aBuf[i];
  }

  return FromByteArray(buf);
}

void SnepMessage::ToByteArray(std::vector<uint8_t>& aBuf)
{
  if (mNdefMessage) {
    mNdefMessage->ToByteArray(aBuf);
  }

  std::vector<uint8_t> snepHeader;

  snepHeader.push_back(mVersion);
  snepHeader.push_back(mField);
  if (mField == SnepMessage::REQUEST_GET) {
    uint32_t len = aBuf.size() + 4;
    snepHeader.push_back((len >> 24) & 0xFF);
    snepHeader.push_back((len >> 16) & 0xFF);
    snepHeader.push_back((len >>  8) & 0xFF);
    snepHeader.push_back( len & 0xFF);
    snepHeader.push_back((mAcceptableLength >> 24) & 0xFF);
    snepHeader.push_back((mAcceptableLength >> 16) & 0xFF);
    snepHeader.push_back((mAcceptableLength >>  8) & 0xFF);
    snepHeader.push_back( mAcceptableLength & 0xFF);
  } else {
    uint32_t len = aBuf.size();
    snepHeader.push_back((len >> 24) & 0xFF);
    snepHeader.push_back((len >> 16) & 0xFF);
    snepHeader.push_back((len >>  8) & 0xFF);
    snepHeader.push_back( len & 0xFF);
  }

  aBuf.insert(aBuf.begin(), snepHeader.begin(), snepHeader.end());
}
