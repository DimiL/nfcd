#include "NdefMessage.h"

NdefMessage::NdefMessage(std::vector<uint8_t>& buf)
{
  NdefRecord::parse(buf, false, mRecords);
}

