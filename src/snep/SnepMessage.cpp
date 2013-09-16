#include "SnepMessage.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

SnepMessage::SnepMessage()
{
  mNdefMessage = NULL;
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
    // TODO : Need to check idx is correct
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
  mNdefMessage = ndefMessage;
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

SnepMessage* SnepMessage::getMessage(uint8_t field) {
  return new SnepMessage(SnepMessage::VERSION, field, 0, 0, NULL);
}

SnepMessage* getSuccessResponse(NdefMessage* ndef) {
  if (ndef == NULL) {
    return new SnepMessage(SnepMessage::VERSION, SnepMessage::RESPONSE_SUCCESS, 0, 0, NULL);
  } else {
    std::vector<uint8_t> buf;
    ndef->toByteArray(buf);
    return new SnepMessage(SnepMessage::VERSION, SnepMessage::RESPONSE_SUCCESS, buf.size(), 0, ndef);
  }
}

SnepMessage* SnepMessage::fromByteArray(std::vector<uint8_t>& buf)
{
  return new SnepMessage(buf);
}

SnepMessage* SnepMessage::fromByteArray(uint8_t* pBuf, int size)
{
  std::vector<uint8_t> buf;
  for (int i = 0; i < size; i++)  buf[i] = pBuf[i];
  return new SnepMessage(buf);
}

void SnepMessage::toByteArray(std::vector<uint8_t>& buf)
{
  if (mNdefMessage != NULL) {
    mNdefMessage->toByteArray(buf);
  } else {
    ALOGE("Null NDEF message");
    buf.push_back(0);
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
