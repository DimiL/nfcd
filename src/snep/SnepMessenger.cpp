#include "SnepMessenger.h"
#include <vector>

#define LOG_TAG "nfcd"
#include <cutils/log.h>

SnepMessenger::SnepMessenger(bool isClient, ILlcpSocket* socket, uint32_t fragmentLength) :
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
  std::vector<uint8_t> buf;
  uint8_t remoteContinue;
  uint32_t offset;

  msg.toByteArray(buf);
  if (mIsClient) {
    remoteContinue = SnepMessage::RESPONSE_CONTINUE;
  } else {
    remoteContinue = SnepMessage::REQUEST_CONTINUE;
  }

  msg.toByteArray(buf);
  uint32_t length = buf.size() <  mFragmentLength ? buf.size() : mFragmentLength;

  mSocket->send(buf);

  if (length == buf.size()) {
    return;
  }

  // Look for Continue or Reject from peer.
  offset = length;
  std::vector<uint8_t> responseBytes;
  mSocket->receive(responseBytes);

  SnepMessage* snepResponse = SnepMessage::fromByteArray(responseBytes);
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

  uint8_t fieldContinue = 0;
  uint8_t fieldReject = 0;

  if (mIsClient) {
    fieldContinue = SnepMessage::REQUEST_CONTINUE;
    fieldReject = SnepMessage::REQUEST_REJECT;
  } else {
    fieldContinue = SnepMessage::RESPONSE_CONTINUE;
    fieldReject = SnepMessage::RESPONSE_REJECT;
  }

  int readSize = 0;
  int size = mSocket->receive(partial);
  if (size < 0 || size < HEADER_LENGTH) {
    socketSend(fieldReject);
  } else {
    readSize = size - HEADER_LENGTH;
    buffer.insert(buffer.end(), partial.begin(), partial.end());
  }

  uint8_t requestVersion = partial[0];
  uint8_t requestField = partial[1];
  uint32_t requestSize = ((uint32_t)partial[2] << 24) |
                         ((uint32_t)partial[3] << 16) |
                         ((uint32_t)partial[4] <<  8) |
                         ((uint32_t)partial[5]);

  if (((requestVersion & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    // Invalid protocol version; treat message as complete.
    return new SnepMessage(requestVersion, requestField, 0, 0, NULL);
  }

  bool doneReading = false;
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
  if (snep == NULL) {
    ALOGE("Badly formatted NDEF message, ignoring");
    return NULL;
  } else {
    return snep;
  }
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
