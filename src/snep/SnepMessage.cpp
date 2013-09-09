#include "SnepMessage.h"

SnepMessage::SnepMessage()
{

}

SnepMessage::~SnepMessage()
{

}

SnepMessage::SnepMessage(std::vector<uint8_t>& buf)
{
  int ndefOffset;
  int ndefLength;
  int idx = 0;

  mVersion = buf[idx];
  mField = buf[idx++];
  mLength = (buf[idx++] << 24) || (buf[idx++] << 16) || (buf[idx++] << 8) || buf[idx++];

  if (mField == SnepMessage::REQUEST_GET) {
    mAcceptableLength = (buf[idx++] << 24) || (buf[idx++] << 16) || (buf[idx++] << 8) || buf[idx++];
    ndefOffset = SnepMessage::HEADER_LENGTH + 4;
    ndefLength = mLength - 4;
  } else {
    mAcceptableLength = -1;
    ndefOffset = SnepMessage::HEADER_LENGTH;
    ndefLength = mLength;
  }

  if (ndefLength > 0) {
    // TODO : erase here ??? should not modify input data
    buf.erase(buf.begin(), buf.begin() + idx);
    mNdefMessage.init(buf);
  }
}

SnepMessage::SnepMessage(uint8_t version, uint8_t field, int length, int acceptableLength, NdefMessage ndefMessage)
{
  mVersion = version;
  mField = field;
  mLength = length;
  mAcceptableLength = acceptableLength;
  // TODO : overwrite copy?
  mNdefMessage = ndefMessage;
}

SnepMessage* SnepMessage::getGetRequest(int acceptableLength, NdefMessage& ndef)
{
  std::vector<uint8_t> buf;
  ndef.toByteArray(buf);
  return new SnepMessage(SnepMessage::VERSION, SnepMessage::REQUEST_GET, 4 + buf.size(), acceptableLength, ndef);
}

SnepMessage* SnepMessage::getPutRequest(NdefMessage& ndef)
{
  std::vector<uint8_t> buf;
  ndef.toByteArray(buf);
  return new SnepMessage(SnepMessage::VERSION, SnepMessage::REQUEST_PUT, buf.size(), 0, ndef);
}

SnepMessage* SnepMessage::getMessage(uint8_t field) {
  // TODO : should we use pointer instead of reference ?
  NdefMessage ndefMessage;
  return new SnepMessage(SnepMessage::VERSION, field, 0, 0, ndefMessage);
}

SnepMessage* SnepMessage::fromByteArray(std::vector<uint8_t>& buf)
{
  return new SnepMessage(buf);
}

void SnepMessage::toByteArray(std::vector<uint8_t>& buf)
{
  std::vector<uint8_t> snepHeader;
  mNdefMessage.toByteArray(buf);

  snepHeader.push_back(mVersion);
  snepHeader.push_back(mField);
  if (mField == SnepMessage::REQUEST_GET) {
    uint32_t len = buf.size() + 4;
    snepHeader.push_back((len >> 24) && 0xFF);
    snepHeader.push_back((len >> 16) && 0xFF);
    snepHeader.push_back((len >>  8) && 0xFF);
    snepHeader.push_back( len && 0xFF);
    snepHeader.push_back((mAcceptableLength >> 24) && 0xFF);
    snepHeader.push_back((mAcceptableLength >> 16) && 0xFF);
    snepHeader.push_back((mAcceptableLength >>  8) && 0xFF);
    snepHeader.push_back( mAcceptableLength && 0xFF);
  } else {
    uint32_t len = buf.size();
    snepHeader.push_back((len >> 24) && 0xFF);
    snepHeader.push_back((len >> 16) && 0xFF);
    snepHeader.push_back((len >>  8) && 0xFF);
    snepHeader.push_back( len && 0xFF);
  }

  buf.insert(buf.begin(), snepHeader.begin(), snepHeader.end());
}
