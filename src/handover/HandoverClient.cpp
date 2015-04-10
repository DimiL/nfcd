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
  Close();
}

bool HandoverClient::Connect()
{
  NFCD_DEBUG("enter");

  if (mState != HandoverClient::DISCONNECTED) {
    NFCD_ERROR("socket already in use");
    return false;
  }
  mState = HandoverClient::CONNECTING;

  INfcManager* pINfcManager = NfcService::GetNfcManager();

  mSocket = pINfcManager->CreateLlcpSocket(0, mMiu, 1, 1024);
  if (!mSocket) {
    NFCD_ERROR("could not connect to socket");
    mState = HandoverClient::DISCONNECTED;
    return false;
  }

  if (!mSocket->ConnectToService(mServiceName)) {
    NFCD_ERROR("could not connect to service (%s)", mServiceName);
    mSocket->Close();
    delete mSocket;
    mSocket = NULL;
    mState = HandoverClient::DISCONNECTED;
    return false;
  }

  mState = HandoverClient::CONNECTED;

  NFCD_DEBUG("exit");
  return true;
}

NdefMessage* HandoverClient::ProcessHandoverRequest(NdefMessage& aMsg)
{
  NdefMessage* ndef = NULL;
  // Send handover request message.
  if (Put(aMsg)) {
    // Get handover select message sent from remote.
    ndef = Receive();
  }

  return ndef;
}

NdefMessage* HandoverClient::Receive()
{
  std::vector<uint8_t> buffer;
  while (true) {
    std::vector<uint8_t> partial;
    int size = mSocket->Receive(partial);
    if (size < 0) {
      NFCD_ERROR("connection broken");
      break;
    } else {
      buffer.insert(buffer.end(), partial.begin(), partial.end());
    }

    NdefMessage* ndef = new NdefMessage();
    if (ndef->Init(buffer)) {
      NFCD_DEBUG("get a complete NDEF message");
      return ndef;
    } else {
      delete ndef;
    }
  }
  return NULL;
}

bool HandoverClient::Put(NdefMessage& aMsg)
{
  NFCD_DEBUG("enter");

  if (mState != HandoverClient::CONNECTED) {
    NFCD_ERROR("not connected");
    return false;
  }

  std::vector<uint8_t> buf;
  aMsg.ToByteArray(buf);
  mSocket->Send(buf);

  NFCD_DEBUG("exit");
  return true;
}

void HandoverClient::Close()
{
  NFCD_DEBUG("enter");

  if (mSocket) {
    mSocket->Close();
    delete mSocket;
    mSocket = NULL;
  }

  mState = HandoverClient::DISCONNECTED;

  NFCD_DEBUG("exit");
}
