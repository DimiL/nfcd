#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "HandoverServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

const char* HandoverServer::DEFAULT_SERVICE_NAME = "urn:nfc:sn:handover";

HandoverCallback::HandoverCallback()
{
}

HandoverCallback::~HandoverCallback()
{
}

void* HandoverConnectionThreadFunc(void* arg)
{
  ALOGD("starting connection thread");

  bool running = true;
  HandoverConnectionThread* pConnectionThread = reinterpret_cast<HandoverConnectionThread*>(arg);
  if (pConnectionThread == NULL) {
    ALOGE("HandoverConnectionThreadFunc invalid parameter");
    return NULL;
  }

  IHandoverCallback* ICallback = pConnectionThread->mCallback;

  bool connectionBroken = false;
  std::vector<uint8_t> buffer;
  while(!connectionBroken) {
    std::vector<uint8_t> partial;
    int size = pConnectionThread->mSock->receive(partial);
    if (size < 0) {
      connectionBroken = true;
      break;
    } else {
      buffer.insert(buffer.end(), partial.begin(), partial.end());
    }
  }

  // TODO : Handle handover

  if (pConnectionThread->mSock)
    pConnectionThread->mSock->close();

  // TODO : is this correct ??
  delete pConnectionThread;

  ALOGD("finished connection thread");

  return NULL;
}

HandoverConnectionThread::HandoverConnectionThread(
  HandoverServer* server, ILlcpSocket* socket, IHandoverCallback* ICallback):
mSock(socket),
mCallback(ICallback),
mServer(server)
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

bool HandoverConnectionThread::isServerRunning()
{
  return mServer->mServerRunning;
}

void* handoverServerThreadFunc(void* arg)
{
  HandoverServer* pHandoverServer = reinterpret_cast<HandoverServer*>(arg);
  if (pHandoverServer == NULL) {
    ALOGE("no handover server class");
    return NULL;
  }

  IHandoverCallback* ICallback = pHandoverServer->mCallback;
  ILlcpServerSocket* serverSocket = pHandoverServer->mServerSocket;

  if (serverSocket == NULL) {
    ALOGE("no server socket");
    return NULL;
  }

  while(pHandoverServer->mServerRunning) {
    if (serverSocket == NULL) {
        ALOGD("Server socket shut down.");
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
  ALOGD("%s enter", __func__);
  INfcManager* pINfcManager = NfcService::getNfcManager();
  mServerSocket = pINfcManager->createLlcpServerSocket(mServiceSap, DEFAULT_SERVICE_NAME, DEFAULT_MIU, 1, 1024);

  if (mServerSocket == NULL) {
    ALOGE("%s cannot create llcp serfer socket", __func__);
  }

  pthread_t tid;
  if(pthread_create(&tid, NULL, handoverServerThreadFunc, this) != 0)
  {
    ALOGE("%s init_nfc_service pthread_create failed", __func__);
    abort();
  }
  mServerRunning = true;
  ALOGD("%s exit", __func__);
}

void HandoverServer::stop()
{
  // TODO : need to kill thread here
  mServerSocket->close();
  mServerRunning = false;

  // use pthread_join here to make sure all thread is finished ?
}
