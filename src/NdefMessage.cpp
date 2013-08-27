#include "NdefMessage.h"

NdefMessage::NdefMessage()
{
}

NdefMessage::~NdefMessage()
{
  mRecords.clear();
}

bool NdefMessage::init(std::vector<uint8_t>& buf)
{
  return NdefRecord::parse(buf, false, mRecords);
}
