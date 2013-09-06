#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

char* mServiceName;
int mServiceSap;
int mFragmentLength;
int mMiu;
int mRwSize;

void* connectionThreadFunc(void* arg)
{
  bool running = true;

  while(running) {
    // Handle message
  }

  return NULL;
}

void* serverThreadFunc(void* arg)
{
  INfcManager* pINfcManager = NfcService::getNfcManager();
  ILlcpServerSocket* pServerSocket = pINfcManager->createLlcpServerSocket(
    mServiceSap, mServiceName, mMiu, mRwSize, 1024);

  while(true) {
    if (pServerSocket == NULL) {
        ALOGD("Server socket shut down.");
        return NULL;
    }

    ILlcpSocket* communicationSocket = pServerSocket->accept();
    
    if (communicationSocket != NULL) {
      int miu = communicationSocket->getRemoteMiu();
      // use math
      //int fragmentLength = (mFragmentLength == -1) ? miu : Math.min(miu, mFragmentLength);
      // use pthread
      //new ConnectionThread(communicationSocket, fragmentLength).start();
    }
  }

  return NULL;
}

SnepServer::SnepServer()
{

}

SnepServer::~SnepServer()
{

}

void SnepServer::start()
{
  pthread_t tid;
  if(pthread_create(&tid, NULL, serverThreadFunc, this) != 0)
  {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }
}
