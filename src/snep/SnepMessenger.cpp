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
  uint8_t* responseBytes = NULL;
  uint8_t* tmpBuf = NULL;
  uint8_t remoteContinue;
  std::vector<uint8_t> buf;
  int offset;

  msg.toByteArray(buf);
  if (mIsClient) {
    remoteContinue = SnepMessage::RESPONSE_CONTINUE;
  } else {
    remoteContinue = SnepMessage::REQUEST_CONTINUE;
  }

  int length = buf.size() <  mFragmentLength ? buf.size() : mFragmentLength;
  tmpBuf = new uint8_t[length];
  for(int i = 0; i < length; i++)
    tmpBuf[i] = buf[i];

  mSocket->send(tmpBuf);

  if (length == buf.size()) {
    goto End;
  }

  // Look for Continue or Reject from peer.
  offset = length;
  responseBytes = new uint8_t[SnepMessenger::HEADER_LENGTH];
  mSocket->receive(responseBytes);

  snepResponse = SnepMessage::fromByteArray(responseBytes, SnepMessenger::HEADER_LENGTH);
  if (snepResponse == NULL) {
    ALOGE("Invalid SNEP message");   
    goto End;
  }

  if (snepResponse->getField() != remoteContinue) {
    ALOGE("Invalid response from server (%d)", snepResponse->getField());
    goto End;
  }

  // Send remaining fragments.
  while (offset < buf.size()) {
    length = buf.size() - offset < mFragmentLength ? buf.size() - offset : mFragmentLength;
    // TODO : Need check here
    for(int i = offset; i < offset + length; i++)
      tmpBuf[i] = buf[i];
    mSocket->send(tmpBuf);
    offset += length;
  }

End:
  delete[] tmpBuf;
  delete[] responseBytes;
  delete snepResponse;
}
  
SnepMessage* SnepMessenger::getMessage()
{
  uint8_t* partial = new uint8_t[mFragmentLength];
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
  if (size < 0) {
    //mSocket->send(SnepMessage.getMessage(fieldReject).toByteArray());
  } else if (size < HEADER_LENGTH) {
    //mSocket->send(SnepMessage.getMessage(fieldReject).toByteArray());
  } else {
    readSize = size - HEADER_LENGTH;
    //buffer.write(partial, 0, size);
  }

  requestVersion = partial[0];
  uint8_t requestField = partial[1];
  requestSize = partial[2] << 24 || partial[3] << 16 || partial[4] << 8 || partial[5];

  if (((requestVersion & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    // Invalid protocol version; treat message as complete.
    return new SnepMessage(requestVersion, requestField, 0, 0, NULL);
  }

  if (requestSize > readSize) {
    //mSocket->send(SnepMessage.getMessage(fieldContinue).toByteArray());
  } else {
    doneReading = true;
  }

  // Remaining fragments
  while (!doneReading) {
    size = mSocket->receive(partial);
    if (size < 0) {
      //mSocket->send(SnepMessage.getMessage(fieldReject).toByteArray());
    } else {
      readSize += size;
      //buffer.write(partial, 0, size);
      if (readSize == requestSize) {
        doneReading = true;
      }
    }
  }

  return NULL;
  //return SnepMessage.fromByteArray(buffer.toByteArray());
}

void SnepMessenger::close()
{
  if (mSocket != NULL)
    mSocket->close();
}
