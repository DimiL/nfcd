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

#include "NdefRecord.h"
#include "NfcDebug.h"

static bool EnsureSanePayloadSize(long aSize);
static bool ValidateTnf(uint8_t aTnf,
                        std::vector<uint8_t>& aType,
                        std::vector<uint8_t>& aId,
                        std::vector<uint8_t>& aPayload);

static const uint8_t FLAG_MB = 0x80;
static const uint8_t FLAG_ME = 0x40;
static const uint8_t FLAG_CF = 0x20;
static const uint8_t FLAG_SR = 0x10;
static const uint8_t FLAG_IL = 0x08;

// 10 MB payload limit.
static const int MAX_PAYLOAD_SIZE = 10 * (1 << 20);

NdefRecord::NdefRecord(uint8_t aTnf,
                       std::vector<uint8_t>& aType,
                       std::vector<uint8_t>& aId,
                       std::vector<uint8_t>& aPayload)
{
  mTnf = aTnf;

  for (size_t i = 0; i < aType.size(); i++) {
    mType.push_back(aType[i]);
  }
  for (size_t i = 0; i < aId.size(); i++) {
    mId.push_back(aId[i]);
  }
  for (size_t i = 0; i < aPayload.size(); i++) {
    mPayload.push_back(aPayload[i]);
  }
}

NdefRecord::NdefRecord(uint8_t aTnf,
                       uint32_t aTypeLength,
                       uint8_t* aType,
                       uint32_t aIdLength,
                       uint8_t* aId,
                       uint32_t aPayloadLength,
                       uint8_t* aPayload)
{
  mTnf = aTnf;

  for (uint32_t i = 0; i < aTypeLength; i++) {
    mType.push_back((uint8_t)aType[i]);
  }
  for (uint32_t i = 0; i < aIdLength; i++) {
    mId.push_back((uint8_t)aId[i]);
  }
  for (uint32_t i = 0; i < aPayloadLength; i++) {
    mPayload.push_back((uint8_t)aPayload[i]);
  }
}

NdefRecord::~NdefRecord()
{
}

bool NdefRecord::Parse(std::vector<uint8_t>& aBuf,
                       bool aIgnoreMbMe,
                       std::vector<NdefRecord>& aRecords)
{
  return NdefRecord::Parse(aBuf, aIgnoreMbMe, aRecords, 0);
}

bool NdefRecord::Parse(std::vector<uint8_t>& aBuf,
                       bool aIgnoreMbMe,
                       std::vector<NdefRecord>& aRecords,
                       int aOffset)
{
  bool inChunk = false;
  uint8_t chunkTnf = -1;
  bool me = false;
  uint32_t index = aOffset;

  while (!me) {
    std::vector<uint8_t> type;
    std::vector<uint8_t> id;
    std::vector<uint8_t> payload;
    std::vector<std::vector<uint8_t> > chunks;

    uint8_t flag = aBuf[index++];

    bool mb = (flag & FLAG_MB) != 0;
    me = (flag & FLAG_ME) != 0;
    bool cf = (flag & FLAG_CF) != 0;
    bool sr = (flag & FLAG_SR) != 0;
    bool il = (flag & FLAG_IL) != 0;
    uint8_t tnf = flag & 0x07;

    if (!mb && aRecords.size() == 0 && !inChunk && !aIgnoreMbMe) {
      NFCD_ERROR("expected MB flag");
      return false;
    } else if (mb && aRecords.size() != 0 && !aIgnoreMbMe) {
      NFCD_ERROR("unexpected MB flag");
      return false;
    } else if (inChunk && il) {
      NFCD_ERROR("unexpected IL flag in non-leading chunk");
      return false;
    } else if (cf && me) {
      NFCD_ERROR("unexpected ME flag in non-trailing chunk");
      return false ;
    } else if (inChunk && tnf != NdefRecord::TNF_UNCHANGED) {
      NFCD_ERROR("expected TNF_UNCHANGED in non-leading chunk");
      return false ;
    } else if (!inChunk && tnf == NdefRecord::TNF_UNCHANGED) {
      NFCD_ERROR("unexpected TNF_UNCHANGED in first chunk or unchunked record");
      return false;
    }

    uint32_t typeLength = aBuf[index++] & 0xFF;

    if (!tnf && typeLength != 0) {
      NFCD_ERROR("expected zero-length type in empty NDEF message");
      return false;
    }

    uint32_t payloadLength;
    if (sr) {
      payloadLength = aBuf[index++] & 0xFF;
    } else {
      payloadLength = ((uint32_t)aBuf[index]     << 24) |
                      ((uint32_t)aBuf[index + 1] << 16) |
                      ((uint32_t)aBuf[index + 2] <<  8) |
                      ((uint32_t)aBuf[index + 3]);
      index += 4;
    }

    if (!tnf && payloadLength != 0) {
      NFCD_ERROR("expected zero-length payload in empty NDEF message");
      return false;
    }

    uint32_t idLength = il ? (aBuf[index++] & 0xFF) : 0;

    if (!tnf && idLength != 0) {
      NFCD_ERROR("expected zero-length id in empty NDEF message");
      return false;
    }

    if (inChunk && typeLength != 0) {
      NFCD_ERROR("expected zero-length type in non-leading chunk");
      return false;
    }

    if (!inChunk) {
      for (uint32_t i = 0; i < typeLength; i++) {
        type.push_back(aBuf[index++]);
      }
      for (uint32_t i = 0; i < idLength; i++) {
        id.push_back(aBuf[index++]);
      }
    }

    if (!EnsureSanePayloadSize(payloadLength)) {
      return false;
    }

    for (uint32_t i = 0; i < payloadLength; i++) {
      payload.push_back(aBuf[index++]);
    }

    if (cf && !inChunk) {
      // first chunk.
      chunks.clear();
      chunkTnf = tnf;
    }
    if (cf || inChunk) {
      // any chunk.
      chunks.push_back(payload);
    }
    if (!cf && inChunk) {
      // last chunk, flatten the payload.
      payloadLength = 0;
      for (size_t i = 0; i < chunks.size(); i++) {
        payloadLength += chunks[i].size();
      }
      if (!EnsureSanePayloadSize(payloadLength)) {
        return false;
      }

      for (size_t i = 0; i < chunks.size(); i++) {
        for (size_t j = 0; j < chunks[i].size(); j++) {
          payload.push_back(chunks[i][j]);
        }
      }
      tnf = chunkTnf;
    }
    if (cf) {
      // more chunks to come.
      inChunk = true;
      continue;
    } else {
      inChunk = false;
    }

    bool isValid = ValidateTnf(tnf, type, id, payload);
    if (isValid == false) {
      return false;
    }

    NdefRecord record(tnf, type, id, payload);
    aRecords.push_back(record);

    if (aIgnoreMbMe) {  // for parsing a single NdefRecord.
      break;
    }
  }
  return true;
}

bool EnsureSanePayloadSize(long aSize)
{
  if (aSize > MAX_PAYLOAD_SIZE) {
    NFCD_ERROR("payload above max limit: %d > ", MAX_PAYLOAD_SIZE);
    return false;
  }
  return true;
}

bool ValidateTnf(uint8_t aTnf,
                 std::vector<uint8_t>& aType,
                 std::vector<uint8_t>& aId,
                 std::vector<uint8_t>& aPayload)
{
  bool isValid = true;
  switch (aTnf) {
    case NdefRecord::TNF_EMPTY:
      if (aType.size() != 0 || aId.size() != 0 || aPayload.size() != 0) {
        NFCD_ERROR("unexpected data in TNF_EMPTY record");
        isValid = false;
      }
      break;
    case NdefRecord::TNF_WELL_KNOWN:
    case NdefRecord::TNF_MIME_MEDIA:
    case NdefRecord::TNF_ABSOLUTE_URI:
    case NdefRecord::TNF_EXTERNAL_TYPE:
      break;
    case NdefRecord::TNF_UNKNOWN:
    case NdefRecord::TNF_RESERVED:
      if (aType.size() != 0) {
        NFCD_ERROR("unexpected type field in TNF_UNKNOWN or TNF_RESERVEd record");
        isValid = false;
      }
      break;
    case NdefRecord::TNF_UNCHANGED:
      NFCD_ERROR("unexpected TNF_UNCHANGED in first chunk or logical record");
      isValid = false;
      break;
    default:
      NFCD_ERROR("unexpected tnf value");
      isValid = false;
      break;
  }
  return isValid;
}

void NdefRecord::WriteToByteBuffer(std::vector<uint8_t>& aBuf,
                                   bool aMb,
                                   bool aMe)
{
  bool sr = mPayload.size() < 256;
  bool il = mId.size() > 0;

  uint8_t flags = (uint8_t)((aMb ? FLAG_MB : 0) |
                            (aMe ? FLAG_ME : 0) |
                            (sr  ? FLAG_SR : 0) |
                            (il  ? FLAG_IL : 0) | mTnf);
  aBuf.push_back(flags);

  aBuf.push_back((uint8_t)mType.size());
  if (sr) {
    aBuf.push_back((uint8_t)mPayload.size());
  } else {
    aBuf.push_back((mPayload.size() >> 24) & 0xff);
    aBuf.push_back((mPayload.size() >> 16) & 0xff);
    aBuf.push_back((mPayload.size() >>  8) & 0xff);
    aBuf.push_back(mPayload.size() & 0xff);
  }
  if (il) {
    aBuf.push_back((uint8_t)mId.size());
  }

  for (uint32_t i = 0; i < mType.size(); i++) {
    aBuf.push_back(mType[i]);
  }
  for (uint32_t i = 0; i < mId.size(); i++) {
    aBuf.push_back(mId[i]);
  }
  for (uint32_t i = 0; i < mPayload.size(); i++) {
    aBuf.push_back(mPayload[i]);
  }
}
