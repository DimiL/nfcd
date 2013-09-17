#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "NdefPushServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

const char* NdefPushServer::DEFAULT_SERVICE_NAME = "com.android.npp";

NdefPushCallback::NdefPushCallback()
{
}

NdefPushCallback::~NdefPushCallback()
{
}

void* pushConnectionThreadFunc(void* arg)
{
  ALOGD("starting connection thread");

  bool running = true;
  PushConnectionThread* pConnectionThread = reinterpret_cast<PushConnectionThread*>(arg);
  if (pConnectionThread == NULL) {
    ALOGE("pushConnectionThreadFunc invalid parameter");
    return NULL;
  }

  INdefPushCallback* ICallback = pConnectionThread->mCallback;

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

  // TODO : Handle NdefPushProtocol

  if (pConnectionThread->mSock)
    pConnectionThread->mSock->close();

  // TODO : is this correct ??
  delete pConnectionThread;

  ALOGD("finished connection thread");

  return NULL;
}

PushConnectionThread::PushConnectionThread(
  NdefPushServer* server, ILlcpSocket* socket, INdefPushCallback* ICallback):
mSock(socket),
mCallback(ICallback),
mServer(server)
{
}

PushConnectionThread::~PushConnectionThread()
{
}

void PushConnectionThread::run()
{
  pthread_t tid;
  if(pthread_create(&tid, NULL, pushConnectionThreadFunc, this) != 0) {
    ALOGE("connection pthread_create failed");
    abort();
  }
}

bool PushConnectionThread::isServerRunning()
{
  return mServer->mServerRunning;
}

void* pushServerThreadFunc(void* arg)
{
  NdefPushServer* pNdefPushServer = reinterpret_cast<NdefPushServer*>(arg);
  if (pNdefPushServer == NULL) {
    ALOGE("no ndefpush server class");
    return NULL;
  }

  INdefPushCallback* ICallback = pNdefPushServer->mCallback;
  ILlcpServerSocket* serverSocket = pNdefPushServer->mServerSocket;

  if (serverSocket == NULL) {
    ALOGE("no server socket");
    return NULL;
  }

  while(pNdefPushServer->mServerRunning) {
    if (serverSocket == NULL) {
        ALOGD("Server socket shut down.");
        return NULL;
    }

    ILlcpSocket* communicationSocket = serverSocket->accept();
    
    if (communicationSocket != NULL) {
      PushConnectionThread* pConnectionThread = 
          new PushConnectionThread(pNdefPushServer, communicationSocket, ICallback);
      pConnectionThread->run();
    }
  }

  return NULL;
}


NdefPushServer::NdefPushServer(INdefPushCallback* ICallback) {
  mCallback = ICallback;
  mServiceSap = NDEFPUSH_SAP;
}

NdefPushServer::~NdefPushServer()
{

}

void NdefPushServer::start()
{
  ALOGD("%s enter", __func__);
  INfcManager* pINfcManager = NfcService::getNfcManager();
  mServerSocket = pINfcManager->createLlcpServerSocket(mServiceSap, DEFAULT_SERVICE_NAME, DEFAULT_MIU, 1, 1024);

  if (mServerSocket == NULL) {
    ALOGE("%s cannot create llcp serfer socket", __func__);
  }

  pthread_t tid;
  if(pthread_create(&tid, NULL, pushServerThreadFunc, this) != 0)
  {
    ALOGE("%s init_nfc_service pthread_create failed", __func__);
    abort();
  }
  mServerRunning = true;
  ALOGD("%s exit", __func__);
}

void NdefPushServer::stop()
{
  // TODO : need to kill thread here
  mServerSocket->close();
  mServerRunning = false;

  // use pthread_join here to make sure all thread is finished ?
}
