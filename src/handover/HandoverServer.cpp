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

void* HandoverConnectionThreadFunc(void* arg)
{
  ALOGD("%s: connection thread enter", FUNC);

  HandoverConnectionThread* pConnectionThread = reinterpret_cast<HandoverConnectionThread*>(arg);
  if (!pConnectionThread) {
    ALOGE("%s: invalid parameter", FUNC);
    return NULL;
  }

  IHandoverCallback* ICallback = pConnectionThread->mCallback;

  bool connectionBroken = false;
  std::vector<uint8_t> buffer;
  while(!connectionBroken) {
    std::vector<uint8_t> partial;
    int size = pConnectionThread->mSock->receive(partial);
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
      ICallback->onMessageReceived(ndef);
      ALOGD("%s: get a complete NDEF message", FUNC);
    } else {
      ALOGD("%s: cannot get a complete NDEF message", FUNC);
    }
    delete ndef;
  }

  if (pConnectionThread->mSock)
    pConnectionThread->mSock->close();

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
      pConnectionThread->run();
    }
  }

  return NULL;
}


HandoverServer::HandoverServer(IHandoverCallback* ICallback) {
  mCallback = ICallback;
  mServiceSap = HANDOVER_SAP;
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

  ALOGD("%s exit", FUNC);
}

void HandoverServer::stop()
{
  // TODO : need to kill thread here
  mServerSocket->close();
  mServerRunning = false;

  // use pthread_join here to make sure all thread is finished ?
}
