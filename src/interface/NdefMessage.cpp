#include "NdefMessage.h"

NdefMessage::NdefMessage()
{
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
