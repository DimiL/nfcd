#include "HandoverClient.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "HandoverServer.h"
#include "ILlcpSocket.h"
#include "NdefMessage.h"
#include "NfcDebug.h"

HandoverClient::HandoverClient()
 : mSocket(NULL)
 , mServiceName(HandoverServer::DEFAULT_SERVICE_NAME)
 , mState(HandoverClient::DISCONNECTED)
 , mMiu(HandoverClient::DEFAULT_MIU)
{
}

HandoverClient::~HandoverClient()
{
  close();
}

bool HandoverClient::connect()
{
  ALOGD("%s: enter", FUNC);

  if (mState != HandoverClient::DISCONNECTED) {
    ALOGE("%s: socket already in use", FUNC);
    return false;
  }
  mState = HandoverClient::CONNECTING;

  INfcManager* pINfcManager = NfcService::getNfcManager();

  mSocket = pINfcManager->createLlcpSocket(0, mMiu, 1, 1024);
  if (!mSocket) {
    ALOGE("%s: could not connect to socket", FUNC);
    mState = HandoverClient::DISCONNECTED;
    return false;
  }

  if (!mSocket->connectToService(mServiceName)) {
    ALOGE("%s: could not connect to service (%s)", FUNC, mServiceName);
    mSocket->close();
    delete mSocket;
    mSocket = NULL;
    mState = HandoverClient::DISCONNECTED;
    return false;
  }

  mState = HandoverClient::CONNECTED;

  ALOGD("%s: exit", FUNC);
  return true;
}

NdefMessage* HandoverClient::processHandoverRequest(NdefMessage& msg)
{
  NdefMessage* ndef = NULL;
  // Send handover request message.
  if (put(msg)) {
    // Get handover select message sent from remote.
    ndef = receive();
  }

  return ndef;
}

NdefMessage* HandoverClient::receive()
{
  std::vector<uint8_t> buffer;
  while(true) {
    std::vector<uint8_t> partial;
    int size = mSocket->receive(partial);
    if (size < 0) {
      ALOGE("%s: connection broken", FUNC);
      break;
    } else {
      buffer.insert(buffer.end(), partial.begin(), partial.end());
    }

    NdefMessage* ndef = new NdefMessage();
    if(ndef->init(buffer)) {
      ALOGD("%s: get a complete NDEF message", FUNC);
      return ndef;
    } else {
      delete ndef;
    }
  }
  return NULL;
}

bool HandoverClient::put(NdefMessage& msg)
{
  ALOGD("%s: enter", FUNC);

  if (mState != HandoverClient::CONNECTED) {
    ALOGE("%s: not connected", FUNC);
    return false;
  }

  std::vector<uint8_t> buf;
  msg.toByteArray(buf);
  mSocket->send(buf);

  ALOGD("%s: exit", FUNC);
  return true;
}

void HandoverClient::close()
{
  ALOGD("%s: enter", FUNC);

  if (mSocket) {
    mSocket->close();
    delete mSocket;
    mSocket = NULL;
  }

  mState = HandoverClient::DISCONNECTED;

  ALOGD("%s: exit", FUNC);
}
