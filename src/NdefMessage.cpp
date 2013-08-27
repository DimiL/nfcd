#include "NdefMessage.h"

NdefMessage::NdefMessage(std::vector<uint8_t>& buf)
{
  // Mozilla : TODO : throw exception
  NdefRecord::parse(buf, false, mRecords);
}

NdefMessage::~NdefMessage()
{
  mRecords.clear();
}

