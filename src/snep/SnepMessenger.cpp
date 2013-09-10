#include "SnepMessenger.h"
#include <vector>

#define LOG_TAG "nfcd"
#include <cutils/log.h>

SnepMessenger::SnepMessenger(bool isClient, ILlcpSocket* socket, int fragmentLength) :
mIsClient(isClient),
mSocket(socket),
mFragmentLength(fragmentLength)
{
}

SnepMessenger::~SnepMessenger()
{
}

void SnepMessenger::sendMessage(SnepMessage& msg)
{
  SnepMessage* snepResponse = NULL;
  uint8_t remoteContinue;
  std::vector<uint8_t> buf;
  uint32_t offset;

  msg.toByteArray(buf);
  if (mIsClient) {
    remoteContinue = SnepMessage::RESPONSE_CONTINUE;
  } else {
    remoteContinue = SnepMessage::REQUEST_CONTINUE;
  }

  uint32_t length = buf.size() <  mFragmentLength ? buf.size() : mFragmentLength;

  mSocket->send(buf);

  if (length == buf.size()) {
    return;
  }

  // Look for Continue or Reject from peer.
  offset = length;
  std::vector<uint8_t> responseBytes;
  mSocket->receive(responseBytes);

  snepResponse = SnepMessage::fromByteArray(responseBytes);
  if (snepResponse == NULL) {
    ALOGE("Invalid SNEP message");
    return;
  }

  if (snepResponse->getField() != remoteContinue) {
    ALOGE("Invalid response from server (%d)", snepResponse->getField());
    delete snepResponse;
    return;
  }

  // Send remaining fragments.
  while (offset < buf.size()) {
    std::vector<uint8_t> tmpBuf;
    length = buf.size() - offset < mFragmentLength ? buf.size() - offset : mFragmentLength;
    // TODO : Need check here
    for(uint32_t i = offset; i < offset + length; i++)
      tmpBuf.push_back(buf[i]);
    mSocket->send(tmpBuf);
    offset += length;
  }
}
  
SnepMessage* SnepMessenger::getMessage()
{
  std::vector<uint8_t> partial;
  std::vector<uint8_t> buffer;
  int size;
  int requestSize = 0;
  int readSize = 0;
  uint8_t requestVersion = 0;
  bool doneReading = false;
  uint8_t fieldContinue;
  uint8_t fieldReject;

  if (mIsClient) {
    fieldContinue = SnepMessage::REQUEST_CONTINUE;
    fieldReject = SnepMessage::REQUEST_REJECT;
  } else {
    fieldContinue = SnepMessage::RESPONSE_CONTINUE;
    fieldReject = SnepMessage::RESPONSE_REJECT;
  }

  size = mSocket->receive(partial);
  if (size < 0 || size < HEADER_LENGTH) {
    socketSend(fieldReject);
  } else {
    readSize = size - HEADER_LENGTH;
    buffer.insert(buffer.end(), partial.begin(), partial.end());
  }

  requestVersion = partial[0];
  uint8_t requestField = partial[1];
  requestSize = partial[2] << 24 || partial[3] << 16 || partial[4] << 8 || partial[5];

  if (((requestVersion & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    // Invalid protocol version; treat message as complete.
    return new SnepMessage(requestVersion, requestField, 0, 0, NULL);
  }

  if (requestSize > readSize) {
    socketSend(fieldContinue);
  } else {
    doneReading = true;
  }

  // TODO : DO we need clear partial here?
  partial.clear();
  // Remaining fragments
  while (!doneReading) {
    size = mSocket->receive(partial);
    if (size < 0) {
      socketSend(fieldReject);
    } else {
      readSize += size;
      buffer.insert(buffer.end(), partial.begin(), partial.end());
      if (readSize == requestSize) {
        doneReading = true;
      }
    }
  }

  SnepMessage* snep = SnepMessage::fromByteArray(buffer); 
  return snep;
}

void SnepMessenger::close()
{
  if (mSocket != NULL)
    mSocket->close();
}

void SnepMessenger::socketSend(uint8_t field)
{
  std::vector<uint8_t> data;
  SnepMessage* msg = SnepMessage::getMessage(field);
  msg->toByteArray(data);
  mSocket->send(data);
  delete msg;
}
