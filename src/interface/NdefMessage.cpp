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

#include "NdefMessage.h"

NdefMessage::NdefMessage()
{
}

NdefMessage::NdefMessage(NdefMessage* ndef)
{
  if (!ndef)
    return;

  std::vector<NdefRecord>& record = ndef->mRecords;
  for (uint32_t i = 0; i < record.size(); i++) {
    mRecords.push_back(NdefRecord(record[i].mTnf, record[i].mType, record[i].mId, record[i].mPayload));
  }
}

NdefMessage::~NdefMessage()
{
  mRecords.clear();
}

bool NdefMessage::init(std::vector<uint8_t>& buf, int offset)
{
  return NdefRecord::parse(buf, false, mRecords, offset);
}

bool NdefMessage::init(std::vector<uint8_t>& buf)
{
  return NdefRecord::parse(buf, false, mRecords);
}

/**
 * This method will generate current NDEF message to byte array(vector).
 */
void NdefMessage::toByteArray(std::vector<uint8_t>& buf)
{
  int recordSize = mRecords.size();
  for (int i = 0; i < recordSize; i++) {
    bool mb = (i == 0);  // first record.
    bool me = (i == recordSize - 1);  // last record.
    mRecords[i].writeToByteBuffer(buf, mb, me);
  }
  return;
}
