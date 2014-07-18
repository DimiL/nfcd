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
void* HandoverConnectionThreadFunc(void* arg)
{
  ALOGD("%s: connection thread enter", FUNC);

  HandoverConnectionThread* pConnectionThread = reinterpret_cast<HandoverConnectionThread*>(arg);
  if (!pConnectionThread) {
    ALOGE("%s: invalid parameter", FUNC);
    return NULL;
  }

  IHandoverCallback* ICallback = pConnectionThread->getCallback();
  ILlcpSocket* socket = pConnectionThread->getSocket();
  bool connectionBroken = false;
  std::vector<uint8_t> buffer;
  while(!connectionBroken) {
    std::vector<uint8_t> partial;
    int size = socket->receive(partial);
    if (size < 0) {
      ALOGE("%s: connection broken", FUNC);
      connectionBroken = true;
      break;
    } else {
      buffer.insert(buffer.end(), partial.begin(), partial.end());
    }

    // Check if buffer can be create a NDEF message.
    // If yes. need to notify upper layer.
    NdefMessage* ndef = new NdefMessage();
    if(ndef->init(buffer)) {
      ALOGD("%s: get a complete NDEF message", FUNC);
      ICallback->onMessageReceived(ndef);
    } else {
      ALOGD("%s: cannot get a complete NDEF message", FUNC);
    }
  }

  if (socket)
    socket->close();

  HandoverServer* server = pConnectionThread->getServer();
  if (server)
    server->setConnectionThread(NULL);

  // TODO : is this correct ??
  delete pConnectionThread;

  ALOGD("%s: connection thread exit", FUNC);
  return NULL;
}

HandoverConnectionThread::HandoverConnectionThread(
  HandoverServer* server, ILlcpSocket* socket, IHandoverCallback* ICallback)
 : mSock(socket)
 , mCallback(ICallback)
 , mServer(server)
{
}

HandoverConnectionThread::~HandoverConnectionThread()
{
}

void HandoverConnectionThread::run()
{
  pthread_t tid;
  if(pthread_create(&tid, NULL, HandoverConnectionThreadFunc, this) != 0) {
    ALOGE("connection pthread_create failed");
    abort();
  }
}

bool HandoverConnectionThread::isServerRunning() const
{
  return mServer->mServerRunning;
}

// Handover server thread is responsible for handling incoming connect request.
void* handoverServerThreadFunc(void* arg)
{
  HandoverServer* pHandoverServer = reinterpret_cast<HandoverServer*>(arg);
  if (!pHandoverServer) {
    ALOGE("%s: invalid parameter", FUNC);
    return NULL;
  }

  ILlcpServerSocket* serverSocket = pHandoverServer->mServerSocket;
  IHandoverCallback* ICallback = pHandoverServer->mCallback;

  if (!serverSocket) {
    ALOGE("%s: no server socket", FUNC);
    return NULL;
  }

  while(pHandoverServer->mServerRunning) {
    if (!serverSocket) {
        ALOGE("%s: server socket shut down", FUNC);
        return NULL;
    }

    ILlcpSocket* communicationSocket = serverSocket->accept();

    if (communicationSocket != NULL) {
      HandoverConnectionThread* pConnectionThread =
          new HandoverConnectionThread(pHandoverServer, communicationSocket, ICallback);
      pHandoverServer->setConnectionThread(pConnectionThread);
      pConnectionThread->run();
    }
  }

  return NULL;
}


HandoverServer::HandoverServer(IHandoverCallback* ICallback)
 : mServerSocket(NULL)
 , mServiceSap(HANDOVER_SAP)
 , mCallback(ICallback)
 , mServerRunning(false)
 , mConnectionThread(NULL)
{
}

HandoverServer::~HandoverServer()
{
}

void HandoverServer::start()
{
  ALOGD("%s: enter", FUNC);

  INfcManager* pINfcManager = NfcService::getNfcManager();
  mServerSocket = pINfcManager->createLlcpServerSocket(mServiceSap, DEFAULT_SERVICE_NAME, DEFAULT_MIU, 1, 1024);

  if (!mServerSocket) {
    ALOGE("%s: cannot create llcp server socket", FUNC);
  }

  pthread_t tid;
  if(pthread_create(&tid, NULL, handoverServerThreadFunc, this) != 0)
  {
    ALOGE("%s: pthread_create failed", FUNC);
    abort();
  }
  mServerRunning = true;

  ALOGD("%s: exit", FUNC);
}

void HandoverServer::stop()
{
  // TODO : need to kill thread here
  if (mServerSocket) {
    mServerSocket->close();
    delete mServerSocket;
    mServerSocket = NULL;
  }
  mServerRunning = false;

  // use pthread_join here to make sure all thread is finished ?
}

bool HandoverServer::put(NdefMessage& msg)
{
  ALOGD("%s: enter", FUNC);

  if (!mConnectionThread || !mConnectionThread->getSocket()) {
    ALOGE("%s: connection is not established", FUNC);
    return false;
  }

  std::vector<uint8_t> buf;
  msg.toByteArray(buf);
  mConnectionThread->getSocket()->send(buf);

  ALOGD("%s: exit", FUNC);
  return true;
}

void HandoverServer::setConnectionThread(HandoverConnectionThread* pThread)
{
  if (pThread != NULL && mConnectionThread != NULL) {
    ALOGE("%s: there is more than one connection, should not happen!", FUNC);
  }
  mConnectionThread = pThread;
}
