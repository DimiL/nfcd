/*
 * Copyright (C) 2013-2014  Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

SnepClient::SnepClient(const char* aServiceName)
 : mMessenger(NULL)
{
  mServiceName = aServiceName;
  mPort = -1;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = -1;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::SnepClient(int aMiu,
                       int aRwSize)
 : mMessenger(NULL)
{
  mServiceName = SnepServer::DEFAULT_SERVICE_NAME;
  mPort = SnepServer::DEFAULT_PORT;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = -1;
  mMiu = aMiu;
  mRwSize = aRwSize;
}

SnepClient::SnepClient(const char* aServiceName,
                       int aFragmentLength)
 : mMessenger(NULL)
{
  mServiceName = aServiceName;
  mPort = -1;
  mAcceptableLength = SnepClient::DEFAULT_ACCEPTABLE_LENGTH;
  mFragmentLength = aFragmentLength;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::SnepClient(const char* aServiceName,
                       int aAcceptableLength,
                       int aFragmentLength)
 : mMessenger(NULL)
{
  mServiceName = aServiceName;
  mPort = -1;
  mAcceptableLength = aAcceptableLength;
  mFragmentLength = aFragmentLength;
  mMiu = SnepClient::DEFAULT_MIU;
  mRwSize = SnepClient::DEFAULT_RWSIZE;
}

SnepClient::~SnepClient()
{
  Close();
}

/**
 * Request Codes : PUT
 * The client requests that the server accept the NDEF message
 * transmitted with the request.
 */
void SnepClient::Put(NdefMessage& aMsg)
{
  if (!mMessenger) {
    NFCD_ERROR("no messenger");
    return;
  }

  if (mState != SnepClient::CONNECTED) {
    NFCD_ERROR("socket is not connected");
    return;
  }

  SnepMessage* snepRequest = SnepMessage::GetPutRequest(aMsg);
  if (snepRequest) {
    // Send request.
    mMessenger->SendMessage(*snepRequest);
  } else {
    NFCD_ERROR("get put request fail");
  }

  // Get response.
  SnepMessage* snepResponse = mMessenger->GetMessage();

  delete snepRequest;
  delete snepResponse;
}

/**
 * Request Codes : GET
 * The client requests that the server return an NDEF message
 * designated by the NDEF message transmitted with the request
 */
SnepMessage* SnepClient::Get(NdefMessage& aMsg)
{
  if (!mMessenger) {
    NFCD_ERROR("no messenger");
    return NULL;
  }

  if (mState != SnepClient::CONNECTED) {
    NFCD_ERROR("socket is not connected");
    return NULL;
  }

  SnepMessage* snepRequest = SnepMessage::GetGetRequest(mAcceptableLength, aMsg);
  mMessenger->SendMessage(*snepRequest);

  delete snepRequest;

  return mMessenger->GetMessage();
}

bool SnepClient::Connect()
{
  if (mState != SnepClient::DISCONNECTED) {
    NFCD_ERROR("snep already connected");
    return false;
  }
  mState = SnepClient::CONNECTING;

  INfcManager* pINfcManager = NfcService::GetNfcManager();

  /**
   * SNEP messages SHALL be transmitted over LLCP data link connections
   * using LLCP connection-oriented transport service.
   */
  ILlcpSocket* socket = pINfcManager->CreateLlcpSocket(0, mMiu, mRwSize, 1024);
  if (!socket) {
    NFCD_ERROR("could not connect to socket");
    mState = SnepClient::DISCONNECTED;
    return false;
  }

  bool ret = false;
  if (mPort == -1) {
    if (!socket->ConnectToService(mServiceName)) {
      NFCD_ERROR("could not connect to service (%s)", mServiceName);
      mState = SnepClient::DISCONNECTED;
      delete socket;
      return false;
    }
  } else {
    if (!socket->ConnectToSap(mPort)) {
      NFCD_ERROR("could not connect to sap (%d)", mPort);
      mState = SnepClient::DISCONNECTED;
      delete socket;
      return false;
    }
  }

  const int miu = socket->GetRemoteMiu();
  const int fragmentLength = (mFragmentLength == -1) ?  miu : miu < mFragmentLength ? miu : mFragmentLength;

  // Remove old messenger.
  if (mMessenger) {
    mMessenger->Close();
    delete mMessenger;
  }

  mMessenger = new SnepMessenger(true, socket, fragmentLength);

  mState = SnepClient::CONNECTED;

  return true;
}

void SnepClient::Close()
{
  if (mMessenger) {
    mMessenger->Close();
    delete mMessenger;
    mMessenger = NULL;
  }
  mState = SnepClient::DISCONNECTED;
}
