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

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"
#include "ISnepCallback.h"
#include "NfcDebug.h"

// Well-known LLCP SAP Values defined by NFC forum.
const char* SnepServer::DEFAULT_SERVICE_NAME = "urn:nfc:sn:snep";

// Connection thread, used to handle incoming connections.
void* SnepConnectionThreadFunc(void* aArg)
{
  NFCD_DEBUG("connection thread enter");

  SnepConnectionThread* pConnectionThread = reinterpret_cast<SnepConnectionThread*>(aArg);
  if (!pConnectionThread) {
    NFCD_ERROR("invalid parameter");
    return NULL;
  }

  ISnepCallback* ICallback = pConnectionThread->mCallback;
  while (pConnectionThread->IsServerRunning()) {
    // Handle message.
    if (!SnepServer::HandleRequest(pConnectionThread->mMessenger, ICallback)) {
      break;
    }
  }

  if (pConnectionThread->mSock)
    pConnectionThread->mSock->Close();

  // TODO : is this correct ??
  delete pConnectionThread;

  NFCD_DEBUG("connection thread exit");
  return NULL;
}

/**
 * Connection thread is created when Snep server accept a connection request.
 */
SnepConnectionThread::SnepConnectionThread(SnepServer* aServer,
                                           ILlcpSocket* aSocket,
                                           int aFragmentLength,
                                           ISnepCallback* aICallback)
 : mSock(aSocket)
 , mCallback(aICallback)
 , mServer(aServer)
{
  mMessenger = new SnepMessenger(false, aSocket, aFragmentLength);
}

SnepConnectionThread::~SnepConnectionThread()
{
  delete mMessenger;
}

void SnepConnectionThread::Run()
{
  pthread_t tid;
  if (pthread_create(&tid, NULL, SnepConnectionThreadFunc, this) != 0) {
    NFCD_ERROR("pthread_create fail");
    abort();
  }
}

bool SnepConnectionThread::IsServerRunning() const
{
  return mServer->mServerRunning;
}

/**
 * Server thread, used to listen for incoming connection request.
 */
void* SnepServerThreadFunc(void* aArg)
{
  SnepServer* pSnepServer = reinterpret_cast<SnepServer*>(aArg);
  if (!pSnepServer) {
    NFCD_ERROR("invalid parameter");
    return NULL;
  }

  ILlcpServerSocket* serverSocket = pSnepServer->mServerSocket;
  ISnepCallback* ICallback = pSnepServer->mCallback;
  const int fragmentLength = pSnepServer->mFragmentLength;

  if (!serverSocket) {
    NFCD_ERROR("no server socket");
    return NULL;
  }

  while (pSnepServer->mServerRunning) {
    if (!serverSocket) {
      NFCD_DEBUG("server socket shut down");
      return NULL;
    }

    ILlcpSocket* communicationSocket = serverSocket->Accept();

    if (communicationSocket) {
      const int miu = communicationSocket->GetRemoteMiu();
      const int length = (fragmentLength == -1) ? miu : miu < fragmentLength ? miu : fragmentLength;

      SnepConnectionThread* pConnectionThread =
          new SnepConnectionThread(pSnepServer, communicationSocket, length, ICallback);
      pConnectionThread->Run();
    }
  }

  if (serverSocket) {
    serverSocket->Close();
    delete serverSocket;
  }

  return NULL;
}

SnepServer::SnepServer(ISnepCallback* aICallback)
 : mServerSocket(NULL)
 , mCallback(aICallback)
 , mServerRunning(false)
 , mServiceName(DEFAULT_SERVICE_NAME)
 , mServiceSap(DEFAULT_PORT)
 , mFragmentLength(-1)
 , mMiu(DEFAULT_MIU)
 , mRwSize(DEFAULT_RW_SIZE)
{
}

SnepServer::SnepServer(const char* aServiceName,
                       int aServiceSap,
                       ISnepCallback* aICallback)
 : mServerSocket(NULL)
 , mCallback(aICallback)
 , mServerRunning(false)
 , mServiceName(aServiceName)
 , mServiceSap(aServiceSap)
 , mFragmentLength(-1)
 , mMiu(DEFAULT_MIU)
 , mRwSize(DEFAULT_RW_SIZE)
{
}

SnepServer::SnepServer(ISnepCallback* aICallback,
                       int aMiu,
                       int aRwSize)
 : mServerSocket(NULL)
 , mCallback(aICallback)
 , mServerRunning(false)
 , mServiceName(DEFAULT_SERVICE_NAME)
 , mServiceSap(DEFAULT_PORT)
 , mFragmentLength(-1)
 , mMiu(aMiu)
 , mRwSize(aRwSize)
{
}

SnepServer::SnepServer(const char* aServiceName,
                       int aServiceSap,
                       int aFragmentLength,
                       ISnepCallback* aICallback)
 : mServerSocket(NULL)
 , mCallback(aICallback)
 , mServerRunning(false)
 , mServiceName(aServiceName)
 , mServiceSap(aServiceSap)
 , mFragmentLength(aFragmentLength)
 , mMiu(DEFAULT_MIU)
 , mRwSize(DEFAULT_RW_SIZE)
{
}

SnepServer::~SnepServer()
{
  Stop();
}

void SnepServer::Start()
{
  NFCD_DEBUG("enter");

  INfcManager* pINfcManager = NfcService::GetNfcManager();
  mServerSocket = pINfcManager->CreateLlcpServerSocket(mServiceSap, mServiceName, mMiu, mRwSize, 1024);

  if (!mServerSocket) {
    NFCD_ERROR("cannot create llcp server socket");
    abort();
  }

  pthread_t tid;
  if (pthread_create(&tid, NULL, SnepServerThreadFunc, this) != 0)
  {
    NFCD_ERROR("pthread_create failed");
    abort();
  }
  mServerRunning = true;

  NFCD_DEBUG("exit");
}

void SnepServer::Stop()
{
  mServerSocket = NULL;
  mServerRunning = false;

  // Use pthread_join here to make sure all thread is finished ?
}

bool SnepServer::HandleRequest(SnepMessenger* aMessenger,
                               ISnepCallback* aCallback)
{
  if (!aMessenger || !aCallback) {
    NFCD_ERROR("invalid parameter");
    return false;
  }

  SnepMessage* request = aMessenger->GetMessage();
  SnepMessage* response = NULL;

  if (!request) {
    /**
     * Response Codes : BAD REQUEST
     * The request could not be understood by the server due to malformed syntax.
     */
    NFCD_ERROR("bad snep message");
    response = SnepMessage::GetMessage(SnepMessage::RESPONSE_BAD_REQUEST);
    if (response) {
      aMessenger->SendMessage(*response);
      delete response;
    }
    return false;
  }

  if (((request->GetVersion() & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    /**
     * Response Codes : UNSUPPORTED VERSION
     * The server does not support, or refuses to support, the SNEP protocol
     * version that was used in the request message.
     */
    NFCD_ERROR("unsupported version");
    response = SnepMessage::GetMessage(SnepMessage::RESPONSE_UNSUPPORTED_VERSION);

  } else if (request->GetField() == SnepMessage::REQUEST_GET) {
    NdefMessage* ndef = request->GetNdefMessage();
    response = aCallback->DoGet(request->GetAcceptableLength(), ndef);

  } else if (request->GetField() == SnepMessage::REQUEST_PUT) {
    NdefMessage* ndef = request->GetNdefMessage();
    response = aCallback->DoPut(ndef);

  } else {
    NFCD_ERROR("bad request");
    response = SnepMessage::GetMessage(SnepMessage::RESPONSE_BAD_REQUEST);
  }

  delete request;
  if (response) {
    aMessenger->SendMessage(*response);
    delete response;
  } else {
    NFCD_ERROR("no response message is generated");
    return false;
  }

  return true;
}
