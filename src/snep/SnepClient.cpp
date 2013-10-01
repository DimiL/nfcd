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

/**
 * Request Codes : PUT
 * The client requests that the server accept the NDEF message
 * transmitted with the request.
 */
void SnepClient::put(NdefMessage& msg)
{
  if (!mMessenger) {
    ALOGE("%s: no messenger", __FUNCTION__);
    return;
  }

  if (mState != SnepClient::CONNECTED) {
    ALOGE("%s: socket is not connected", __FUNCTION__);
    return;
  }

  SnepMessage* snepRequest = SnepMessage::getPutRequest(msg);
  if (snepRequest) {
    // Send request.
    mMessenger->sendMessage(*snepRequest);
  } else {
    ALOGE("%s: get put request fail", __FUNCTION__);
  }

  // Get response.
  SnepMessage* snepResponse = mMessenger->getMessage();

  delete snepRequest;
  delete snepResponse;
}

/**
 * Request Codes : GET
 * The client requests that the server return an NDEF message
 * designated by the NDEF message transmitted with the request
 */
SnepMessage* SnepClient::get(NdefMessage& msg)
{
  if (!mMessenger) {
    ALOGE("%s: no messenger", __FUNCTION__);
    return NULL;
  }

  if (mState != SnepClient::CONNECTED) {
    ALOGE("%s: socket is not connected", __FUNCTION__);
    return NULL;
  }

  SnepMessage* snepRequest = SnepMessage::getGetRequest(mAcceptableLength, msg);
  mMessenger->sendMessage(*snepRequest);
  delete snepRequest;

  return mMessenger->getMessage();
}

void SnepClient::connect()
{
  if (mState != SnepClient::DISCONNECTED) {
    ALOGE("%s: socket already in use", __FUNCTION__);
    return;
  }
  mState = SnepClient::CONNECTING;

  INfcManager* pINfcManager = NfcService::getNfcManager();

  /**
   * SNEP messages SHALL be transmitted over LLCP data link connections
   * using LLCP connection-oriented transport service.
   */
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

  // Remove old messenger.
  if (mMessenger) {
    mMessenger->close();
    delete mMessenger;
  }

  mMessenger = new SnepMessenger(true, socket, fragmentLength);

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
