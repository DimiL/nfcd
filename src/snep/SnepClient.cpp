#include "SnepClient.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"
#include "NfcDebug.h"

SnepClient::SnepClient()
 : mMessenger(NULL)
{
  mState = SnepClient::DISCONNECTED;
  mServiceName = SnepServer::DEFAULT_SERVICE_NAME;
  mPort = SnepServer::DEFAULT_PORT;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = -1;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::SnepClient(const char* serviceName)
 : mMessenger(NULL)
{
  mServiceName = serviceName;
  mPort = -1;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = -1;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::SnepClient(int miu, int rwSize)
 : mMessenger(NULL)
{
  mServiceName = SnepServer::DEFAULT_SERVICE_NAME;
  mPort = SnepServer::DEFAULT_PORT;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = -1;
  mMiu = miu;
  mRwSize = rwSize;
}

SnepClient::SnepClient(const char* serviceName, int fragmentLength)
 : mMessenger(NULL)
{
  mServiceName = serviceName;
  mPort = -1;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = fragmentLength;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::SnepClient(const char* serviceName, int acceptableLength, int fragmentLength)
 : mMessenger(NULL)
{
  mServiceName = serviceName;
  mPort = -1;
  mAcceptableLength = acceptableLength;
  mFragmentLength = fragmentLength;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::~SnepClient()
{
  close();
}

/**
 * Request Codes : PUT
 * The client requests that the server accept the NDEF message
 * transmitted with the request.
 */
void SnepClient::put(NdefMessage& msg)
{
  if (!mMessenger) {
    ALOGE("%s: no messenger", FUNC);
    return;
  }

  if (mState != SnepClient::CONNECTED) {
    ALOGE("%s: socket is not connected", FUNC);
    return;
  }

  SnepMessage* snepRequest = SnepMessage::getPutRequest(msg);
  if (snepRequest) {
    // Send request.
    mMessenger->sendMessage(*snepRequest);
  } else {
    ALOGE("%s: get put request fail", FUNC);
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
    ALOGE("%s: no messenger", FUNC);
    return NULL;
  }

  if (mState != SnepClient::CONNECTED) {
    ALOGE("%s: socket is not connected", FUNC);
    return NULL;
  }

  SnepMessage* snepRequest = SnepMessage::getGetRequest(mAcceptableLength, msg);
  mMessenger->sendMessage(*snepRequest);

  delete snepRequest;

  return mMessenger->getMessage();
}

bool SnepClient::connect()
{
  if (mState != SnepClient::DISCONNECTED) {
    ALOGE("%s: snep already connected", FUNC);
    return false;
  }
  mState = SnepClient::CONNECTING;

  INfcManager* pINfcManager = NfcService::getNfcManager();

  /**
   * SNEP messages SHALL be transmitted over LLCP data link connections
   * using LLCP connection-oriented transport service.
   */
  ILlcpSocket* socket = pINfcManager->createLlcpSocket(0, mMiu, mRwSize, 1024);
  if (!socket) {
    ALOGE("%s: could not connect to socket", FUNC);
    mState = SnepClient::DISCONNECTED;
    return false;
  }

  bool ret = false;
  if (mPort == -1) {
    if (!socket->connectToService(mServiceName)) {
      ALOGE("%s: could not connect to service (%s)", FUNC, mServiceName);
      mState = SnepClient::DISCONNECTED;
      delete socket;
      return false;
    }
  } else {
    if (!socket->connectToSap(mPort)) {
      ALOGE("%s: could not connect to sap (%d)", FUNC, mPort);
      mState = SnepClient::DISCONNECTED;
      delete socket;
      return false;
    }
  }

  const int miu = socket->getRemoteMiu();
  const int fragmentLength = (mFragmentLength == -1) ?  miu : miu < mFragmentLength ? miu : mFragmentLength;

  // Remove old messenger.
  if (mMessenger) {
    mMessenger->close();
    delete mMessenger;
  }

  mMessenger = new SnepMessenger(true, socket, fragmentLength);

  mState = SnepClient::CONNECTED;

  return true;
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
