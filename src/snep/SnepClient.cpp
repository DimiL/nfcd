#include "SnepClient.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

SnepClient::SnepClient()
{
  mState = SnepClient::DISCONNECTED;
  mServiceName = SnepServer::DEFAULT_SERVICE_NAME;
  mPort = SnepServer::DEFAULT_PORT;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = -1;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::SnepClient(const char* serviceName) {
  mServiceName = serviceName;
  mPort = -1;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = -1;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::SnepClient(int miu, int rwSize) {
  mServiceName = SnepServer::DEFAULT_SERVICE_NAME;
  mPort = SnepServer::DEFAULT_PORT;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = -1;
  mMiu = miu;
  mRwSize = rwSize;
}

SnepClient::SnepClient(const char* serviceName, int fragmentLength) {
  mServiceName = serviceName;
  mPort = -1;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = fragmentLength;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::SnepClient(const char* serviceName, int acceptableLength, int fragmentLength) {
  mServiceName = serviceName;
  mPort = -1;
  mAcceptableLength = acceptableLength;
  mFragmentLength = fragmentLength;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::~SnepClient()
{
  if (mMessenger) {
    mMessenger->close();
    delete mMessenger;
    mMessenger = NULL;
  }
}

void SnepClient::put(NdefMessage& msg)
{
  SnepMessenger* messenger = NULL;
  if (mState != SnepClient::CONNECTED) {
    ALOGE("%s: socket is not connected", __FUNCTION__);
    return;
  }
  messenger = mMessenger;

  SnepMessage* snepMessage = SnepMessage::getPutRequest(msg);
  if (snepMessage) {
    messenger->sendMessage(*snepMessage);
  } else {
    ALOGE("%s: get put request fail", __FUNCTION__);
  }

  SnepMessage* response = messenger->getMessage();

  delete snepMessage;
  delete response;
}

SnepMessage* SnepClient::get(NdefMessage& msg)
{
  SnepMessenger* messenger = NULL;
  if (mState != SnepClient::CONNECTED) {
    ALOGE("%s: socket is not connected", __FUNCTION__);
    return NULL;
  }
  messenger = mMessenger;  

  SnepMessage* snepMessage = SnepMessage::getGetRequest(mAcceptableLength, msg);
  messenger->sendMessage(*snepMessage);
  delete snepMessage;

  return messenger->getMessage();
}

void SnepClient::connect()
{
  if (mState != SnepClient::DISCONNECTED) {
    ALOGE("%s: socket already in use", __FUNCTION__);
    return;
  }
  mState = SnepClient::CONNECTING;

  INfcManager* pINfcManager = NfcService::getNfcManager();
  ILlcpSocket* socket = pINfcManager->createLlcpSocket(0, mMiu, mRwSize, 1024);
  if (!socket) {
    ALOGE("%s: could not connect to socket", __FUNCTION__);
    return;
  }

  bool ret = false;
  if (mPort == -1) {
    if (!socket->connectToService(mServiceName)) {
      ALOGE("%s: could not connect to service (%s)", __FUNCTION__, mServiceName);
      delete socket;
      return;
    }
  } else {
    if (!socket->connectToSap(mPort)) {
      ALOGE("%s: could not connect to sap (%d)", __FUNCTION__, mPort);
      delete socket;
      return;
    }    
  }

  int miu = socket->getRemoteMiu();
  int fragmentLength = (mFragmentLength == -1) ?  miu : miu < mFragmentLength ? miu : mFragmentLength;
  SnepMessenger* messenger = new SnepMessenger(true, socket, fragmentLength);

  // Remove old messenger.
  if (mMessenger) {
    mMessenger->close();
    delete mMessenger;
  }
  mMessenger = messenger;

  mState = SnepClient::CONNECTED;
}

void SnepClient::close()
{
  if (mMessenger) {
    mMessenger->close();
    delete mMessenger;
    mMessenger = NULL;
  }
  mState = SnepClient::DISCONNECTED;
}
