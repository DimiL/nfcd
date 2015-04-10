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
#include "HandoverServer.h"
#include "IHandoverCallback.h"
#include "NdefMessage.h"
#include "NfcDebug.h"

// Registered LLCP Service Names.
const char* HandoverServer::DEFAULT_SERVICE_NAME = "urn:nfc:sn:handover";

// Handover conncetion thread is responsible for sending/receiving NDEF message.
void* HandoverConnectionThreadFunc(void* aArg)
{
  NFCD_DEBUG("connection thread enter");

  HandoverConnectionThread* pConnectionThread = reinterpret_cast<HandoverConnectionThread*>(aArg);
  if (!pConnectionThread) {
    NFCD_ERROR("invalid parameter");
    return NULL;
  }

  IHandoverCallback* ICallback = pConnectionThread->GetCallback();
  ILlcpSocket* socket = pConnectionThread->GetSocket();
  bool connectionBroken = false;
  std::vector<uint8_t> buffer;
  while (!connectionBroken) {
    std::vector<uint8_t> partial;
    int size = socket->Receive(partial);
    if (size < 0) {
      NFCD_ERROR("connection broken");
      connectionBroken = true;
      break;
    } else {
      buffer.insert(buffer.end(), partial.begin(), partial.end());
    }

    // Check if buffer can be create a NDEF message.
    // If yes. need to notify upper layer.
    NdefMessage* ndef = new NdefMessage();
    if(ndef->Init(buffer)) {
      NFCD_DEBUG("get a complete NDEF message");
      ICallback->OnMessageReceived(ndef);
    } else {
      NFCD_DEBUG("cannot get a complete NDEF message");
    }
  }

  if (socket)
    socket->Close();

  HandoverServer* server = pConnectionThread->GetServer();
  if (server)
    server->SetConnectionThread(NULL);

  // TODO : is this correct ??
  delete pConnectionThread;

  NFCD_DEBUG("connection thread exit");
  return NULL;
}

HandoverConnectionThread::HandoverConnectionThread(HandoverServer* aServer,
                                                   ILlcpSocket* aSocket,
                                                   IHandoverCallback* aCallback)
 : mSock(aSocket)
 , mCallback(aCallback)
 , mServer(aServer)
{
}

HandoverConnectionThread::~HandoverConnectionThread()
{
}

void HandoverConnectionThread::Run()
{
  pthread_t tid;
  if (pthread_create(&tid, NULL, HandoverConnectionThreadFunc, this) != 0) {
    NFCD_ERROR("connection pthread_create failed");
    abort();
  }
}

bool HandoverConnectionThread::IsServerRunning() const
{
  return mServer->mServerRunning;
}

// Handover server thread is responsible for handling incoming connect request.
void* HandoverServerThreadFunc(void* aArg)
{
  HandoverServer* pHandoverServer = reinterpret_cast<HandoverServer*>(aArg);
  if (!pHandoverServer) {
    NFCD_ERROR("invalid parameter");
    return NULL;
  }

  ILlcpServerSocket* serverSocket = pHandoverServer->mServerSocket;
  IHandoverCallback* ICallback = pHandoverServer->mCallback;

  if (!serverSocket) {
    NFCD_ERROR("no server socket");
    return NULL;
  }

  while (pHandoverServer->mServerRunning) {
    if (!serverSocket) {
      NFCD_ERROR("server socket shut down");
      return NULL;
    }

    ILlcpSocket* communicationSocket = serverSocket->Accept();

    if (communicationSocket != NULL) {
      HandoverConnectionThread* pConnectionThread =
        new HandoverConnectionThread(pHandoverServer, communicationSocket, ICallback);
      pHandoverServer->SetConnectionThread(pConnectionThread);
      pConnectionThread->Run();
    }
  }

  if (serverSocket) {
    serverSocket->Close();
    delete serverSocket;
  }

  return NULL;
}


HandoverServer::HandoverServer(IHandoverCallback* aCallback)
 : mServerSocket(NULL)
 , mServiceSap(HANDOVER_SAP)
 , mCallback(aCallback)
 , mServerRunning(false)
 , mConnectionThread(NULL)
{
}

HandoverServer::~HandoverServer()
{
}

void HandoverServer::Start()
{
  NFCD_DEBUG("enter");

  INfcManager* pINfcManager = NfcService::GetNfcManager();
  mServerSocket = pINfcManager->CreateLlcpServerSocket(
    mServiceSap, DEFAULT_SERVICE_NAME, DEFAULT_MIU, 1, 1024);

  if (!mServerSocket) {
    NFCD_ERROR("cannot create llcp server socket");
  }

  pthread_t tid;
  if (pthread_create(&tid, NULL, HandoverServerThreadFunc, this) != 0) {
    NFCD_ERROR("pthread_create failed");
    abort();
  }
  mServerRunning = true;

  NFCD_DEBUG("exit");
}

void HandoverServer::Stop()
{
  mServerSocket = NULL;
  mServerRunning = false;

  // use pthread_join here to make sure all thread is finished ?
}

bool HandoverServer::Put(NdefMessage& aMsg)
{
  NFCD_DEBUG("enter");

  if (!mConnectionThread || !mConnectionThread->GetSocket()) {
    NFCD_ERROR("connection is not established");
    return false;
  }

  std::vector<uint8_t> buf;
  aMsg.ToByteArray(buf);
  mConnectionThread->GetSocket()->Send(buf);

  NFCD_DEBUG("exit");
  return true;
}

void HandoverServer::SetConnectionThread(HandoverConnectionThread* aThread)
{
  if (aThread != NULL && mConnectionThread != NULL) {
    NFCD_ERROR("there is more than one connection, should not happen!");
  }
  mConnectionThread = aThread;
}
